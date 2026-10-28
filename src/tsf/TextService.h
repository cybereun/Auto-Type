#pragma once

#include <functional>

#include "Globals.h"

namespace autotype {

class TextService final : public ITfTextInputProcessorEx, public ITfKeyEventSink, public ITfCompositionSink,
                          public ITfCompartmentEventSink {
public:
    TextService();
    ~TextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** out) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor(Ex)
    STDMETHODIMP Activate(ITfThreadMgr* threadMgr, TfClientId clientId) override;
    STDMETHODIMP ActivateEx(ITfThreadMgr* threadMgr, TfClientId clientId, DWORD flags) override;
    STDMETHODIMP Deactivate() override;

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL foreground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* context, WPARAM wParam, LPARAM lParam, BOOL* eaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* context, WPARAM wParam, LPARAM lParam, BOOL* eaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* context, WPARAM wParam, LPARAM lParam, BOOL* eaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* context, WPARAM wParam, LPARAM lParam, BOOL* eaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten) override;

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ec, ITfComposition* composition) override;

    // ITfCompartmentEventSink: an app switched the IME off or to English mode
    STDMETHODIMP OnChange(REFGUID compartment) override;

private:
    enum class Action { Pass, Swallow, Append, Backspace, Toggle, CommitSpace, CommitThenPass };
    Action classify(WPARAM key, char* letter) const;

    void runEdit(ITfContext* context, std::function<void(TfEditCookie)> body);
    void showText(TfEditCookie ec, ITfContext* context, const std::u32string& text);
    void endComposition(TfEditCookie ec, ITfContext* context, const std::u32string& finalText);
    void commitNow(ITfContext* context, const std::u32string& extra);
    bool reclaim(TfEditCookie ec, ITfContext* context);
    void announceKoreanMode();
    void watchCompartments(bool on);

    LONG refs_ = 1;
    ITfThreadMgr* threadMgr_ = nullptr;
    TfClientId clientId_ = TF_CLIENTID_NULL;
    ITfComposition* composition_ = nullptr;
    // Set when the app ended our composition mid-word (Excel, KakaoTalk do this). The word's text is
    // already in the document; the next letter takes it back so the word keeps being judged as one.
    bool resume_ = false;
    std::u32string orphan_;
    int pendingErase_ = 0;  // Backspaces we injected that the app has not received yet
    DWORD openCookie_ = TF_INVALID_COOKIE;
    DWORD modeCookie_ = TF_INVALID_COOKIE;
    Engine engine_;
};

}  // namespace autotype
