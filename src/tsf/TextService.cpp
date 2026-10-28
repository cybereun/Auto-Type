#include "TextService.h"

#include <string>

namespace autotype {

namespace {

// Runs a lambda inside a TSF edit session (the only time a text service may touch the document).
class EditSession final : public ITfEditSession {
public:
    explicit EditSession(std::function<void(TfEditCookie)> body) : body_(std::move(body)) {}
    STDMETHODIMP QueryInterface(REFIID riid, void** out) override {
        if (!out) return E_INVALIDARG;
        *out = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) *out = static_cast<ITfEditSession*>(this);
        if (!*out) return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG n = InterlockedDecrement(&refs_);
        if (n == 0) delete this;
        return n;
    }
    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        body_(ec);
        return S_OK;
    }

private:
    LONG refs_ = 1;
    std::function<void(TfEditCookie)> body_;
};

bool down(int vk) { return (GetKeyState(vk) & 0x8000) != 0; }

// Re-sends a key we held back, so the app receives it after our text is committed.
void resend(WPARAM key) {
    INPUT inputs[2] = {};
    for (auto& input : inputs) {
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = static_cast<WORD>(key);
    }
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void moveCaretToEnd(TfEditCookie ec, ITfContext* context, ITfRange* range) {
    ITfRange* caret = nullptr;
    if (FAILED(range->Clone(&caret))) return;
    caret->Collapse(ec, TF_ANCHOR_END);
    TF_SELECTION selection = {};
    selection.range = caret;
    selection.style.ase = TF_AE_NONE;
    selection.style.fInterimChar = FALSE;
    context->SetSelection(ec, 1, &selection);
    caret->Release();
}

}  // namespace

TextService::TextService() : engine_(sharedModel()) { dllAddRef(); }
TextService::~TextService() { dllRelease(); }

STDMETHODIMP TextService::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_INVALIDARG;
    *out = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor) ||
        IsEqualIID(riid, IID_ITfTextInputProcessorEx))
        *out = static_cast<ITfTextInputProcessorEx*>(this);
    else if (IsEqualIID(riid, IID_ITfKeyEventSink))
        *out = static_cast<ITfKeyEventSink*>(this);
    else if (IsEqualIID(riid, IID_ITfCompositionSink))
        *out = static_cast<ITfCompositionSink*>(this);
    if (!*out) return E_NOINTERFACE;
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) TextService::AddRef() { return InterlockedIncrement(&refs_); }
STDMETHODIMP_(ULONG) TextService::Release() {
    LONG n = InterlockedDecrement(&refs_);
    if (n == 0) delete this;
    return n;
}

STDMETHODIMP TextService::Activate(ITfThreadMgr* threadMgr, TfClientId clientId) {
    return ActivateEx(threadMgr, clientId, 0);
}

STDMETHODIMP TextService::ActivateEx(ITfThreadMgr* threadMgr, TfClientId clientId, DWORD) {
    threadMgr_ = threadMgr;
    threadMgr_->AddRef();
    clientId_ = clientId;
    ITfKeystrokeMgr* keystrokes = nullptr;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keystrokes)))) {
        keystrokes->AdviseKeyEventSink(clientId_, static_cast<ITfKeyEventSink*>(this), TRUE);
        keystrokes->Release();
    }
    return S_OK;
}

STDMETHODIMP TextService::Deactivate() {
    if (threadMgr_) {
        ITfKeystrokeMgr* keystrokes = nullptr;
        if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keystrokes)))) {
            keystrokes->UnadviseKeyEventSink(clientId_);
            keystrokes->Release();
        }
        threadMgr_->Release();
        threadMgr_ = nullptr;
    }
    if (composition_) {
        composition_->Release();
        composition_ = nullptr;
    }
    engine_.clear();
    clientId_ = TF_CLIENTID_NULL;
    return S_OK;
}

TextService::Action TextService::classify(WPARAM key, char* letter) const {
    const bool composing = !engine_.empty();
    const bool shift = down(VK_SHIFT);
    if (key == VK_HANGUL || (key == VK_SPACE && shift)) return composing ? Action::Toggle : Action::Pass;
    if (key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT || key == VK_CAPITAL) return Action::Pass;
    if (down(VK_CONTROL) || down(VK_MENU) || down(VK_LWIN) || down(VK_RWIN))
        return composing ? Action::CommitThenPass : Action::Pass;
    if (key >= 'A' && key <= 'Z') {
        const bool caps = (GetKeyState(VK_CAPITAL) & 1) != 0;
        *letter = static_cast<char>(shift != caps ? key : key + 32);
        return Action::Append;
    }
    if (key >= '0' && key <= '9' && !shift && composing) {
        *letter = static_cast<char>(key);
        return Action::Append;
    }
    if (!composing) return Action::Pass;
    if (key == VK_BACK) return Action::Backspace;
    if (key == VK_SPACE) return Action::CommitSpace;
    return Action::CommitThenPass;
}

STDMETHODIMP TextService::OnSetFocus(BOOL) { return S_OK; }

STDMETHODIMP TextService::OnTestKeyDown(ITfContext*, WPARAM wParam, LPARAM, BOOL* eaten) {
    char letter = 0;
    *eaten = classify(wParam, &letter) != Action::Pass;
    return S_OK;
}

STDMETHODIMP TextService::OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) {
    *eaten = FALSE;
    return S_OK;
}

STDMETHODIMP TextService::OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) {
    *eaten = FALSE;
    return S_OK;
}

STDMETHODIMP TextService::OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) {
    *eaten = FALSE;
    return S_OK;
}

STDMETHODIMP TextService::OnKeyDown(ITfContext* context, WPARAM wParam, LPARAM, BOOL* eaten) {
    char letter = 0;
    const Action action = classify(wParam, &letter);
    *eaten = action != Action::Pass;
    switch (action) {
        case Action::Pass:
            break;
        case Action::Append:
            engine_.append(letter);
            runEdit(context, [&](TfEditCookie ec) { showText(ec, context, engine_.text()); });
            break;
        case Action::Backspace:
            engine_.backspace();
            if (engine_.empty()) runEdit(context, [&](TfEditCookie ec) { endComposition(ec, context, U""); });
            else runEdit(context, [&](TfEditCookie ec) { showText(ec, context, engine_.text()); });
            break;
        case Action::Toggle:
            engine_.toggleLanguage();
            runEdit(context, [&](TfEditCookie ec) { showText(ec, context, engine_.text()); });
            break;
        case Action::CommitSpace:
            commitNow(context, U" ");
            break;
        case Action::CommitThenPass:
            commitNow(context, U"");
            resend(wParam);
            break;
    }
    return S_OK;
}

STDMETHODIMP TextService::OnCompositionTerminated(TfEditCookie, ITfComposition*) {
    // The app ended the word (click elsewhere, focus change): keep what is on screen.
    if (composition_) {
        composition_->Release();
        composition_ = nullptr;
    }
    engine_.commit();
    return S_OK;
}

void TextService::runEdit(ITfContext* context, std::function<void(TfEditCookie)> body) {
    if (!context) return;
    auto* session = new EditSession(std::move(body));
    HRESULT result = S_OK;
    context->RequestEditSession(clientId_, session, TF_ES_SYNC | TF_ES_READWRITE, &result);
    session->Release();
}

void TextService::showText(TfEditCookie ec, ITfContext* context, const std::u32string& text) {
    if (!composition_) {
        ITfInsertAtSelection* insert = nullptr;
        ITfContextComposition* contextComposition = nullptr;
        ITfRange* range = nullptr;
        if (SUCCEEDED(context->QueryInterface(IID_ITfInsertAtSelection, reinterpret_cast<void**>(&insert))) &&
            SUCCEEDED(insert->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, nullptr, 0, &range)) &&
            SUCCEEDED(context->QueryInterface(IID_ITfContextComposition, reinterpret_cast<void**>(&contextComposition)))) {
            contextComposition->StartComposition(ec, range, static_cast<ITfCompositionSink*>(this), &composition_);
        }
        if (range) range->Release();
        if (insert) insert->Release();
        if (contextComposition) contextComposition->Release();
        if (!composition_) return;
    }
    ITfRange* range = nullptr;
    if (FAILED(composition_->GetRange(&range))) return;
    std::u16string utf16 = toUtf16(text);
    std::wstring wide(utf16.begin(), utf16.end());
    range->SetText(ec, 0, wide.c_str(), static_cast<LONG>(wide.size()));
    moveCaretToEnd(ec, context, range);
    range->Release();
}

void TextService::endComposition(TfEditCookie ec, ITfContext* context, const std::u32string& finalText) {
    if (!finalText.empty()) showText(ec, context, finalText);
    if (!composition_) return;
    if (finalText.empty()) {
        ITfRange* range = nullptr;
        if (SUCCEEDED(composition_->GetRange(&range))) {
            range->SetText(ec, 0, L"", 0);
            range->Release();
        }
    }
    composition_->EndComposition(ec);
    composition_->Release();
    composition_ = nullptr;
}

void TextService::commitNow(ITfContext* context, const std::u32string& extra) {
    std::u32string text = engine_.commit() + extra;
    runEdit(context, [&](TfEditCookie ec) { endComposition(ec, context, text); });
}

}  // namespace autotype
