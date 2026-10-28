// Real Windows Edit/RichEdit controls for manual IME regression checks.
// No synthetic typing: use physical key events through the UI.
#include <windows.h>
#include <msctf.h>
#include <richedit.h>
#include <string>

static HWND plain, rich, status;
static ITfThreadMgr* manager;
static const CLSID clsid = {0x103733a2,0xf0d5,0x4df0,{0xb8,0x8a,0xd6,0x28,0x15,0x28,0xac,0x32}};
static const GUID profile = {0x45f819e1,0x0a0c,0x47b2,{0xbf,0x9c,0x0a,0x94,0xa5,0xa4,0xa4,0xab}};
static void activate() {
    ITfInputProcessorProfileMgr* p = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&p));
    if (SUCCEEDED(hr)) {
        hr = p->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0412,clsid,profile,nullptr,0);
        p->Release();
    }
    wchar_t text[100]; wsprintfW(text,L"Auto-Type profile activation: 0x%08X",hr);
    SetWindowTextW(status,text);
    SetFocus(plain);
}
static LRESULT CALLBACK proc(HWND w, UINT m, WPARAM a, LPARAM b) {
    if (m == WM_CREATE) {
        auto inst = GetModuleHandleW(nullptr);
        CreateWindowW(L"BUTTON",L"Activate Auto-Type",WS_CHILD|WS_VISIBLE,20,10,220,35,w,(HMENU)1,inst,nullptr);
        status=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,260,18,500,30,w,nullptr,inst,nullptr);
        CreateWindowW(L"STATIC",L"Win32 Edit (Explorer-style)",WS_CHILD|WS_VISIBLE,20,60,500,25,w,nullptr,inst,nullptr);
        plain=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,20,90,720,50,w,(HMENU)2,inst,nullptr);
        CreateWindowW(L"STATIC",L"RichEdit (legacy messaging control)",WS_CHILD|WS_VISIBLE,20,160,500,25,w,nullptr,inst,nullptr);
        rich=CreateWindowExW(WS_EX_CLIENTEDGE,MSFTEDIT_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE,20,190,720,160,w,(HMENU)3,inst,nullptr);
        HFONT font=CreateFontW(-24,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Malgun Gothic");
        SendMessageW(plain,WM_SETFONT,(WPARAM)font,TRUE); SendMessageW(rich,WM_SETFONT,(WPARAM)font,TRUE);
        return 0;
    }
    if(m==WM_COMMAND && LOWORD(a)==1) { activate(); return 0; }
    if(m==WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(w,m,a,b);
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,LPWSTR,int) {
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    LoadLibraryW(L"Msftedit.dll");
    CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager));
    TfClientId id; if(manager) manager->Activate(&id);
    WNDCLASSW wc={}; wc.lpfnWndProc=proc; wc.hInstance=inst; wc.lpszClassName=L"AutoTypeTestHost"; wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); RegisterClassW(&wc);
    HWND w=CreateWindowW(wc.lpszClassName,L"Auto-Type input verification",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,800,430,nullptr,nullptr,inst,nullptr);
    MSG msg; while(GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if(manager){manager->Deactivate();manager->Release();} CoUninitialize(); return 0;
}
