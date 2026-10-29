#include "TextService.h"

#include <algorithm>
#include <string>
#include <vector>

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

// Some applications expose a TSF context but reject ITfInsertAtSelection for this edit session.
// The non-activating preview keeps focus in the target app, so Unicode input is a safe fallback.
bool sendUnicodeText(const std::u32string& text) {
    const std::u16string utf16 = toUtf16(text);
    if (utf16.empty()) return false;

    std::vector<INPUT> inputs;
    inputs.reserve(utf16.size() * 2);
    for (char16_t value : utf16) {
        INPUT downInput{};
        downInput.type = INPUT_KEYBOARD;
        downInput.ki.wScan = static_cast<WORD>(value);
        downInput.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(downInput);

        INPUT upInput = downInput;
        upInput.ki.dwFlags |= KEYEVENTF_KEYUP;
        inputs.push_back(upInput);
    }

    const UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    if (sent != inputs.size()) {
        char line[64];
        wsprintfA(line, "unicode fallback sent=%u/%u", sent, static_cast<unsigned>(inputs.size()));
        debugLog(line);
        return false;
    }
    debugLog("unicode fallback committed");
    return true;
}

}  // namespace

TextService::TextService() : engine_(sharedModel()) { dllAddRef(); }
TextService::~TextService() {
    preview_.destroy();
    dllRelease();
}

STDMETHODIMP TextService::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_INVALIDARG;
    *out = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor) ||
        IsEqualIID(riid, IID_ITfTextInputProcessorEx))
        *out = static_cast<ITfTextInputProcessorEx*>(this);
    else if (IsEqualIID(riid, IID_ITfKeyEventSink))
        *out = static_cast<ITfKeyEventSink*>(this);
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
    // Some hosts, especially immersive Windows UI fields, do not allow a TIP to create
    // a regular top-level preview window. The preview is optional; failing to create it
    // must not prevent the text service from receiving and committing keys through TSF.
    if (!preview_.create()) debugLog("preview window unavailable; continuing without preview");
    threadMgr_ = threadMgr;
    threadMgr_->AddRef();
    clientId_ = clientId;
    ITfKeystrokeMgr* keystrokes = nullptr;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keystrokes)))) {
        const HRESULT adviseResult = keystrokes->AdviseKeyEventSink(clientId_, static_cast<ITfKeyEventSink*>(this), TRUE);
        char line[64];
        wsprintfA(line, "AdviseKeyEventSink=0x%08lX", adviseResult);
        debugLog(line);
        keystrokes->Release();
    } else {
        debugLog("ITfKeystrokeMgr query failed");
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
    preview_.hide();
    preview_.destroy();
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
    engine_.clear();
    clientId_ = TF_CLIENTID_NULL;
    return S_OK;
}

TextService::Action TextService::classify(WPARAM key, char* letter) const {
    const bool composing = !engine_.empty();
    const bool shift = down(VK_SHIFT);
    // Idle 한/영 key is swallowed: it would otherwise switch Windows to "IME off" and stop Auto-Type.
    if (key == VK_HANGUL) return composing ? Action::Toggle : Action::Swallow;
    if (key == VK_SPACE && shift) return composing ? Action::Toggle : Action::Pass;
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
    if (key == VK_ESCAPE) return Action::Cancel;
    if (key == VK_SPACE) return Action::CommitSpace;
    return Action::CommitThenPass;
}

STDMETHODIMP TextService::OnSetFocus(BOOL foreground) {
    if (foreground) {
        announceKoreanMode();
    } else {
        engine_.clear();
        preview_.hide();
    }
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
    switch (action) {
        case Action::Pass:
        case Action::Swallow:
            break;
        case Action::Append:
            engine_.append(letter);
            {
                bool shown = false;
                runEdit(context, [&](TfEditCookie ec) { shown = showPreview(ec, context, engine_.text()); });
                if (!shown) showPreview(0, nullptr, engine_.text());
            }
            break;
        case Action::Backspace:
            engine_.backspace();
            if (engine_.empty()) preview_.hide();
            else {
                bool shown = false;
                runEdit(context, [&](TfEditCookie ec) { shown = showPreview(ec, context, engine_.text()); });
                if (!shown) showPreview(0, nullptr, engine_.text());
            }
            break;
        case Action::Toggle:
            engine_.toggleLanguage();
            {
                bool shown = false;
                runEdit(context, [&](TfEditCookie ec) { shown = showPreview(ec, context, engine_.text()); });
                if (!shown) showPreview(0, nullptr, engine_.text());
            }
            break;
        case Action::Cancel:
            engine_.clear();
            preview_.hide();
            break;
        case Action::CommitSpace:
            if (!commitNow(context, U" ")) debugLog("space commit failed");
            break;
        case Action::CommitThenPass:
            if (commitNow(context, U"")) resend(wParam);
            else debugLog("text commit failed; pass-through key held");
            break;
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

HRESULT TextService::runEdit(ITfContext* context, std::function<void(TfEditCookie)> body) {
    if (!context) return E_INVALIDARG;
    auto* session = new EditSession(std::move(body));
    HRESULT sessionResult = E_FAIL;
    const HRESULT requestResult = context->RequestEditSession(clientId_, session, TF_ES_SYNC | TF_ES_READWRITE, &sessionResult);
    session->Release();
    if (FAILED(requestResult) || FAILED(sessionResult)) {
        char line[96];
        wsprintfA(line, "edit request=0x%08lX session=0x%08lX", requestResult, sessionResult);
        debugLog(line);
    }
    return FAILED(requestResult) ? requestResult : sessionResult;
}

bool TextService::showPreview(TfEditCookie ec, ITfContext* context, const std::u32string& text) {
    if (text.empty()) {
        preview_.hide();
        return false;
    }

    ITfContextView* view = nullptr;
    ULONG fetched = 0;
    TF_SELECTION selection{};
    RECT caret{};
    BOOL clipped = FALSE;
    bool haveCaret = false;

    if (context && SUCCEEDED(context->GetActiveView(&view)) && view &&
        SUCCEEDED(context->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) &&
        fetched && selection.range) {
        haveCaret = SUCCEEDED(view->GetTextExt(ec, selection.range, &caret, &clipped)) &&
                    caret.right >= caret.left && caret.bottom >= caret.top;
        selection.range->Release();
        selection.range = nullptr;
    }
    if (!haveCaret) {
        GUITHREADINFO info{};
        info.cbSize = sizeof(info);
        if (GetGUIThreadInfo(GetCurrentThreadId(), &info) && info.hwndCaret) {
            caret = info.rcCaret;
            MapWindowPoints(info.hwndCaret, nullptr, reinterpret_cast<POINT*>(&caret), 2);
            haveCaret = true;
        }
    }

    if (!haveCaret && view) {
        HWND target = nullptr;
        RECT window{};
        if (SUCCEEDED(view->GetWnd(&target)) && target && GetWindowRect(target, &window)) {
            caret = {window.left + 24, window.top + 24, window.left + 25, window.top + 25};
            haveCaret = true;
        }
    }
    if (view) view->Release();

    if (!haveCaret) {
        HWND foreground = GetForegroundWindow();
        RECT window{};
        if (foreground && GetWindowRect(foreground, &window)) {
            caret = {window.left + 24, window.top + 24, window.left + 25, window.top + 25};
            haveCaret = true;
        }
    }
    if (!haveCaret) {
        preview_.hide();
        debugLog("could not locate caret for preview");
        return false;
    }
    return preview_.show(text, caret);
}

bool TextService::insertCommittedText(TfEditCookie ec, ITfContext* context, const std::u32string& text) {
    if (!context || text.empty()) return false;
    ITfInsertAtSelection* insert = nullptr;
    const HRESULT queryResult = context->QueryInterface(IID_ITfInsertAtSelection, reinterpret_cast<void**>(&insert));
    if (FAILED(queryResult) || !insert) {
        char line[64];
        wsprintfA(line, "insert interface=0x%08lX", queryResult);
        debugLog(line);
        return false;
    }

    const std::u16string utf16 = toUtf16(text);
    const std::wstring wide(utf16.begin(), utf16.end());
    const HRESULT result = insert->InsertTextAtSelection(ec, TF_IAS_NOQUERY, wide.c_str(),
                                                          static_cast<LONG>(wide.size()), nullptr);
    insert->Release();
    if (FAILED(result)) {
        char line[64];
        wsprintfA(line, "insert text=0x%08lX", result);
        debugLog(line);
    }
    return SUCCEEDED(result);
}

bool TextService::commitNow(ITfContext* context, const std::u32string& extra) {
    if (engine_.empty()) return false;
    std::u32string text = engine_.text();
    text += extra;
    bool inserted = false;
    const HRESULT editResult = runEdit(context, [&](TfEditCookie ec) { inserted = insertCommittedText(ec, context, text); });
    if (FAILED(editResult)) {
        char line[64];
        wsprintfA(line, "commit edit=0x%08lX", editResult);
        debugLog(line);
    }
    if (!inserted && !sendUnicodeText(text)) return false;
    engine_.clear();
    preview_.hide();
    return true;
}

}  // namespace autotype
