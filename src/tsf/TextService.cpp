#include "TextService.h"

#include <algorithm>
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

}  // namespace

// Writes to %LOCALAPPDATA%\AutoType\log.txt only when that folder has a file named "debug".
void debugLog(const char* message) {
    static int enabled = -1;
    static wchar_t path[MAX_PATH] = {};
    if (enabled < 0) {
        wchar_t dir[MAX_PATH] = {}, flag[MAX_PATH] = {};
        DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
        enabled = 0;
        if (n > 0 && n < MAX_PATH - 40) {
            wsprintfW(flag, L"%s\\AutoType\\debug", dir);
            wsprintfW(path, L"%s\\AutoType\\log.txt", dir);
            enabled = GetFileAttributesW(flag) != INVALID_FILE_ATTRIBUTES ? 1 : 0;
        }
    }
    if (!enabled) return;
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const wchar_t* name = wcsrchr(exe, L'\\');
    char line[600];
    SYSTEMTIME t;
    GetLocalTime(&t);
    wsprintfA(line, "%02d:%02d:%02d.%03d %ls  %s\r\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, name ? name + 1 : exe, message);
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(lstrlenA(line)), &written, nullptr);
    CloseHandle(file);
}

namespace {

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
    else if (IsEqualIID(riid, IID_ITfCompartmentEventSink))
        *out = static_cast<ITfCompartmentEventSink*>(this);
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
    announceKoreanMode();
    watchCompartments(true);
    debugLog("activated");
    return S_OK;
}

// Tells Windows the IME is open and in Hangul mode. Without this, apps that ask the IMM32 status
// (Excel, KakaoTalk) treat Auto-Type as "IME off" and take the first keys as plain letters.
void TextService::announceKoreanMode() {
    if (!threadMgr_) return;
    ITfCompartmentMgr* compartments = nullptr;
    if (FAILED(threadMgr_->QueryInterface(IID_ITfCompartmentMgr, reinterpret_cast<void**>(&compartments)))) return;
    const struct { const GUID* id; LONG value; } values[] = {
        {&GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, 1},
        {&GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, TF_CONVERSIONMODE_NATIVE},
    };
    for (const auto& item : values) {
        ITfCompartment* compartment = nullptr;
        if (SUCCEEDED(compartments->GetCompartment(*item.id, &compartment))) {
            VARIANT value;
            VariantInit(&value);
            value.vt = VT_I4;
            value.lVal = item.value;
            compartment->SetValue(clientId_, &value);
            compartment->Release();
        }
    }
    compartments->Release();
}

STDMETHODIMP TextService::Deactivate() {
    watchCompartments(false);
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
    resume_ = false;
    clientId_ = TF_CLIENTID_NULL;
    return S_OK;
}

TextService::Action TextService::classify(WPARAM key, char* letter) const {
    const bool composing = !engine_.empty() && !resume_;
    const bool shift = down(VK_SHIFT);
    // Keep the IME active while changing the initial language hint or the current word.
    if (key == VK_HANGUL || (key == VK_SPACE && shift)) return Action::Toggle;
    if (key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT || key == VK_CAPITAL) return Action::Pass;
    if (down(VK_CONTROL) || down(VK_MENU) || down(VK_LWIN) || down(VK_RWIN))
        return composing ? Action::CommitThenPass : Action::Pass;
    if (key >= 'A' && key <= 'Z') {
        const bool caps = (GetKeyState(VK_CAPITAL) & 1) != 0;
        *letter = static_cast<char>(shift != caps ? key : key + 32);
        return Action::Append;
    }
    if (key >= '0' && key <= '9' && !shift && (composing || resume_)) {
        *letter = static_cast<char>(key);
        return Action::Append;
    }
    if (!composing) return Action::Pass;
    if (key == VK_BACK) return Action::Backspace;
    if (key == VK_SPACE) return Action::CommitSpace;
    return Action::CommitThenPass;
}

STDMETHODIMP TextService::OnSetFocus(BOOL foreground) {
    if (foreground) announceKoreanMode();
    return S_OK;
}

STDMETHODIMP TextService::OnTestKeyDown(ITfContext*, WPARAM wParam, LPARAM, BOOL* eaten) {
    char letter = 0;
    *eaten = classify(wParam, &letter) != Action::Pass;
    char line[64];
    wsprintfA(line, "key 0x%02X eaten=%d composing=%d", static_cast<unsigned>(wParam), *eaten ? 1 : 0, engine_.empty() ? 0 : 1);
    debugLog(line);
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
    if (resume_) {
        bool resumed = false;
        if (action == Action::Append) {
            runEdit(context, [&](TfEditCookie ec) { resumed = reclaim(ec, context); });
        }
        resume_ = false;
        orphan_.clear();
        // Could not take the word back (the app moved the caret or changed the text): start fresh.
        debugLog(resumed ? "took the word back" : "could not take the word back");
        if (!resumed) engine_.commit();
    }
    switch (action) {
        case Action::Pass:
        case Action::Swallow:
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
            if (!engine_.empty()) runEdit(context, [&](TfEditCookie ec) { showText(ec, context, engine_.text()); });
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
    // The app ended the word. Keep what is on screen, and remember it so that, if the next key
    // continues the same word, the word is taken back instead of being split (되어야 + go).
    debugLog("app ended the composition");
    if (composition_) {
        composition_->Release();
        composition_ = nullptr;
    }
    if (!engine_.empty()) {
        orphan_ = engine_.text();
        resume_ = !orphan_.empty();
    }
    return S_OK;
}

namespace {
ITfCompartment* compartment(ITfThreadMgr* threadMgr, const GUID& id) {
    ITfCompartmentMgr* compartments = nullptr;
    ITfCompartment* result = nullptr;
    if (threadMgr && SUCCEEDED(threadMgr->QueryInterface(IID_ITfCompartmentMgr, reinterpret_cast<void**>(&compartments)))) {
        compartments->GetCompartment(id, &result);
        compartments->Release();
    }
    return result;
}

LONG readLong(ITfCompartment* item) {
    VARIANT value;
    VariantInit(&value);
    LONG result = 0;
    if (SUCCEEDED(item->GetValue(&value)) && value.vt == VT_I4) result = value.lVal;
    VariantClear(&value);
    return result;
}
}  // namespace

void TextService::watchCompartments(bool on) {
    const struct { const GUID* id; DWORD* cookie; } watched[] = {
        {&GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &openCookie_},
        {&GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, &modeCookie_},
    };
    for (const auto& item : watched) {
        ITfCompartment* target = compartment(threadMgr_, *item.id);
        if (!target) continue;
        ITfSource* source = nullptr;
        if (SUCCEEDED(target->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
            if (on && *item.cookie == TF_INVALID_COOKIE)
                source->AdviseSink(IID_ITfCompartmentEventSink, static_cast<ITfCompartmentEventSink*>(this), item.cookie);
            else if (!on && *item.cookie != TF_INVALID_COOKIE) {
                source->UnadviseSink(*item.cookie);
                *item.cookie = TF_INVALID_COOKIE;
            }
            source->Release();
        }
        target->Release();
    }
}

// Auto-Type has no English mode: an app (KakaoTalk remembers 한/영 per chat) that turns the IME off or
// to alphanumeric would make Windows bypass us and send raw letters, so the mode is put straight back.
STDMETHODIMP TextService::OnChange(REFGUID id) {
    const bool open = IsEqualGUID(id, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    const bool mode = IsEqualGUID(id, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);
    if (!open && !mode) return S_OK;
    ITfCompartment* target = compartment(threadMgr_, id);
    if (!target) return S_OK;
    const LONG value = readLong(target);
    const bool off = open ? value == 0 : (value & TF_CONVERSIONMODE_NATIVE) == 0;
    target->Release();
    if (off) {
        debugLog(open ? "app closed the IME; reopening" : "app set English mode; restoring Hangul");
        announceKoreanMode();
    }
    return S_OK;
}

// Restarts the composition over the word the app just committed, if it is still right before the caret.
bool TextService::reclaim(TfEditCookie ec, ITfContext* context) {
    const std::u16string expected = toUtf16(orphan_);
    if (expected.empty() || expected.size() > 64) return false;
    ITfInsertAtSelection* insert = nullptr;
    ITfContextComposition* contextComposition = nullptr;
    ITfRange* range = nullptr;
    bool ok = false;
    if (SUCCEEDED(context->QueryInterface(IID_ITfInsertAtSelection, reinterpret_cast<void**>(&insert))) &&
        SUCCEEDED(insert->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, nullptr, 0, &range))) {
        LONG shifted = 0;
        range->Collapse(ec, TF_ANCHOR_START);
        if (SUCCEEDED(range->ShiftStart(ec, -static_cast<LONG>(expected.size()), &shifted, nullptr)) &&
            -shifted == static_cast<LONG>(expected.size())) {
            WCHAR buffer[65] = {};
            ULONG got = 0;
            if (SUCCEEDED(range->GetText(ec, 0, buffer, 64, &got)) && got == expected.size() &&
                std::equal(expected.begin(), expected.end(), buffer) &&
                SUCCEEDED(context->QueryInterface(IID_ITfContextComposition, reinterpret_cast<void**>(&contextComposition))) &&
                SUCCEEDED(contextComposition->StartComposition(ec, range, static_cast<ITfCompositionSink*>(this), &composition_)) &&
                composition_) {
                ok = true;
            }
        }
    }
    if (contextComposition) contextComposition->Release();
    if (range) range->Release();
    if (insert) insert->Release();
    return ok;
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
