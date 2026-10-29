#pragma once

#include <functional>

#include "Globals.h"
#include "PreviewWindow.h"

namespace autotype {

class TextService final : public ITfTextInputProcessorEx, public ITfKeyEventSink, public ITfCompartmentEventSink {
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

    // ITfCompartmentEventSink: an app switched the IME off or to English mode
    STDMETHODIMP OnChange(REFGUID compartment) override;

private:
    enum class Action { Pass, Swallow, Append, Backspace, Toggle, Cancel, CommitSpace, CommitThenPass };
    Action classify(WPARAM key, char* letter) const;

    HRESULT runEdit(ITfContext* context, std::function<void(TfEditCookie)> body);
    bool showPreview(TfEditCookie ec, ITfContext* context, const std::u32string& text);
    bool insertCommittedText(TfEditCookie ec, ITfContext* context, const std::u32string& text);
    bool commitNow(ITfContext* context, const std::u32string& extra);
    void announceKoreanMode();
    void watchCompartments(bool on);

    LONG refs_ = 1;
    ITfThreadMgr* threadMgr_ = nullptr;
    TfClientId clientId_ = TF_CLIENTID_NULL;
    DWORD openCookie_ = TF_INVALID_COOKIE;
    DWORD modeCookie_ = TF_INVALID_COOKIE;
    Engine engine_;
    PreviewWindow preview_;
};

}  // namespace autotype
