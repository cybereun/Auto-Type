#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
#include <UIAutomation.h>
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include "PendingWord.h"
#include "Version.h"

using namespace autotype;
namespace {
constexpr ULONG_PTR kOwnInput = 0x4155544Fu;
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr int kToggleHotkeyId = 0x1001;
constexpr UINT kToggleHotkeyModifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
constexpr UINT kToggleHotkeyKey = 'H';
Model model;
std::unique_ptr<PendingWord> word;
HWND mainWindow=nullptr, previewWindow=nullptr, toggleButton=nullptr, statusLabel=nullptr;
HWND brandLabel=nullptr, versionLabel=nullptr, headingLabel=nullptr, descriptionLabel=nullptr, helperLabel=nullptr;
HWND ownerFocus=nullptr, ownerForeground=nullptr;
HWND passFocus=nullptr, passForeground=nullptr;
RECT ownerBounds{};
POINT lastClick{};
HWND lastClickForeground=nullptr;
HHOOK keyboardHook=nullptr, mouseHook=nullptr;
HFONT textFont=nullptr, uiFont=nullptr, titleFont=nullptr, smallFont=nullptr, microFont=nullptr, buttonFont=nullptr;
HICON brandIcon=nullptr;
HANDLE monitorStop=nullptr, monitorThread=nullptr;
SRWLOCK focusLock=SRWLOCK_INIT;
struct FocusSnapshot { HWND foreground=nullptr, focus=nullptr; RECT bounds{}; bool writable=false; ULONGLONG checkedAt=0, epoch=0; };
FocusSnapshot focusSnapshot;
std::atomic<ULONGLONG> focusEpoch{1};
bool enabled=false;
bool sending=false;
bool blockedWord=false;
bool toggleHotkeyRegistered=false;
constexpr COLORREF kCanvas=RGB(248,249,252);
constexpr COLORREF kInk=RGB(26,32,44);
constexpr COLORREF kMuted=RGB(111,120,137);
constexpr COLORREF kBlue=RGB(63,91,214);
constexpr COLORREF kStatusBg=RGB(238,241,248);
HBRUSH canvasBrush=nullptr;

std::wstring wide(const std::u32string& value) {
    auto s=toUtf16(value); return std::wstring(s.begin(),s.end());
}
HWND focusedWindow(HWND foreground) {
    if (!foreground) return nullptr;
    GUITHREADINFO info{}; info.cbSize=sizeof(info);
    return GetGUIThreadInfo(GetWindowThreadProcessId(foreground,nullptr),&info) ? info.hwndFocus : nullptr;
}
bool eligible(IUIAutomation* automation, HWND foreground, HWND focus, RECT* bounds) {
    if (!foreground || !focus || !automation) return false;
    DWORD pid=0; GetWindowThreadProcessId(foreground,&pid);
    if(pid==GetCurrentProcessId()) return false;
    wchar_t cls[64]{}; GetClassNameW(focus,cls,64);
    if (_wcsicmp(cls,L"Edit")==0 && (GetWindowLongPtrW(focus,GWL_STYLE)&ES_PASSWORD)) return false;
    IUIAutomationElement* element=nullptr;
    if(FAILED(automation->GetFocusedElement(&element)) || !element) return false;
    BOOL password=TRUE, active=FALSE; int process=0; CONTROLTYPEID type=0;
    HRESULT hr=element->get_CurrentIsPassword(&password);
    element->get_CurrentProcessId(&process);
    element->get_CurrentIsEnabled(&active);
    element->get_CurrentControlType(&type);
    const bool textType=type==UIA_EditControlTypeId || type==UIA_DocumentControlTypeId || type==UIA_DataItemControlTypeId;
    bool writable=false;
    IUIAutomationValuePattern* value=nullptr;
    if(SUCCEEDED(element->GetCurrentPatternAs(UIA_ValuePatternId,IID_PPV_ARGS(&value))) && value) {
        BOOL readOnly=TRUE;
        if(SUCCEEDED(value->get_CurrentIsReadOnly(&readOnly))) writable=!readOnly;
        value->Release();
    }
    RECT rect{};
    if (bounds && SUCCEEDED(element->get_CurrentBoundingRectangle(&rect)) && rect.right>rect.left && rect.bottom>rect.top)
        *bounds=rect;
    element->Release();
    return SUCCEEDED(hr) && !password && active && process==(int)pid && textType && writable;
}
void invalidateFocus() {
    focusEpoch.fetch_add(1,std::memory_order_relaxed);
}
bool excelGrid(HWND foreground, HWND focus, RECT* bounds);
DWORD WINAPI watchFocus(void*) {
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED))) return 1;
    IUIAutomation* automation=nullptr;
    if(FAILED(CoCreateInstance(CLSID_CUIAutomation,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation)))) {
        CoUninitialize(); return 1;
    }
    while(WaitForSingleObject(monitorStop,0)==WAIT_TIMEOUT) {
        FocusSnapshot next;
        next.epoch=focusEpoch.load(std::memory_order_relaxed);
        next.foreground=GetForegroundWindow(); next.focus=focusedWindow(next.foreground);
        // Excel's accessibility provider can block for several seconds, so the hook
        // handles the worksheet grid through its native window class instead.
        if(!excelGrid(next.foreground,next.focus,nullptr))
            next.writable=eligible(automation,next.foreground,next.focus,&next.bounds);
        // A delayed UI Automation reply must never authorize a different focused control.
        if(next.foreground!=GetForegroundWindow() || next.focus!=focusedWindow(next.foreground)) next.writable=false;
        next.checkedAt=GetTickCount64();
        AcquireSRWLockExclusive(&focusLock); focusSnapshot=next; ReleaseSRWLockExclusive(&focusLock);
        if(WaitForSingleObject(monitorStop,120)!=WAIT_TIMEOUT) break;
    }
    automation->Release(); CoUninitialize(); return 0;
}
bool cachedEligible(HWND foreground, HWND focus, RECT* bounds) {
    if(!TryAcquireSRWLockShared(&focusLock)) return false;
    FocusSnapshot current=focusSnapshot;
    ReleaseSRWLockShared(&focusLock);
    if(!current.writable || current.epoch!=focusEpoch.load(std::memory_order_relaxed) || current.foreground!=foreground || current.focus!=focus ||
       !current.checkedAt || GetTickCount64()-current.checkedAt>1200) return false;
    if(bounds) *bounds=current.bounds;
    return true;
}
bool excelGrid(HWND foreground, HWND focus, RECT* bounds) {
    wchar_t foregroundClass[32]{}, focusClass[32]{};
    if(!foreground || !focus || !GetClassNameW(foreground,foregroundClass,32) ||
       !GetClassNameW(focus,focusClass,32) || _wcsicmp(foregroundClass,L"XLMAIN") ||
       (_wcsicmp(focusClass,L"EXCEL7") && _wcsicmp(focusClass,L"EXCEL6"))) return false;
    RECT client{};
    if(bounds && lastClickForeground==foreground && GetWindowRect(focus,&client) && PtInRect(&client,lastClick)) {
        *bounds={lastClick.x,lastClick.y,lastClick.x+1,lastClick.y+18};
    }
    return true;
}
bool ownerIsFocused() {
    HWND foreground=GetForegroundWindow();
    return ownerFocus && foreground==ownerForeground && focusedWindow(foreground)==ownerFocus;
}
void refreshPreview() {
    if(!previewWindow || !word) return;
    if(word->empty() || blockedWord || !ownerIsFocused()) { ShowWindow(previewWindow,SW_HIDE); return; }
    GUITHREADINFO info{}; info.cbSize=sizeof(info);
    RECT caret{};
    if(GetGUIThreadInfo(GetWindowThreadProcessId(ownerForeground,nullptr),&info) && info.hwndCaret) {
        caret=info.rcCaret; MapWindowPoints(info.hwndCaret,nullptr,reinterpret_cast<POINT*>(&caret),2);
    } else if(ownerBounds.right>ownerBounds.left && ownerBounds.bottom>ownerBounds.top) {
        caret=ownerBounds;
    } else {
        GetWindowRect(ownerFocus,&caret); caret.bottom=caret.top+32;
    }
    MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
    HMONITOR nearest=MonitorFromRect(&caret,MONITOR_DEFAULTTONEAREST);
    RECT work{};
    if(GetMonitorInfoW(nearest,&monitor)) work=monitor.rcWork;
    else SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    int x=max(work.left,min((int)caret.left,work.right-380));
    int y=max(work.top,min((int)caret.bottom+5,work.bottom-58));
    SetWindowPos(previewWindow,HWND_TOPMOST,x,y,380,52,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    InvalidateRect(previewWindow,nullptr,TRUE);
}
void status(const wchar_t* value) { if(statusLabel) SetWindowTextW(statusLabel,value); }

void drawRoundedButton(const DRAWITEMSTRUCT* item) {
    HDC dc=item->hDC; RECT r=item->rcItem;
    const bool primary=item->CtlID==1;
    COLORREF fill=primary?kBlue:RGB(255,255,255);
    COLORREF edge=primary?kBlue:RGB(225,229,237);
    COLORREF ink=primary?RGB(255,255,255):RGB(61,69,84);
    if(item->itemState&ODS_SELECTED) fill=primary?RGB(48,70,178):RGB(239,242,248);
    HBRUSH brush=CreateSolidBrush(fill); HPEN pen=CreatePen(PS_SOLID,1,edge);
    HGDIOBJ oldBrush=SelectObject(dc,brush), oldPen=SelectObject(dc,pen);
    RoundRect(dc,r.left+1,r.top+1,r.right-1,r.bottom-1,10,10);
    SelectObject(dc,oldBrush); SelectObject(dc,oldPen); DeleteObject(brush); DeleteObject(pen);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,ink);
    HGDIOBJ oldFont=SelectObject(dc,buttonFont); RECT text=r;
    DrawTextW(dc,item->CtlID==1?(enabled?L"자동 입력 끄기":L"자동 입력 켜기"):L"대기 입력 취소",-1,&text,DT_SINGLELINE|DT_VCENTER|DT_CENTER|DT_NOPREFIX);
    SelectObject(dc,oldFont);
}

// The target receives one complete word. No backspaces and no replacement of existing text.
bool sendWord(WPARAM followKey=0) {
    if(word->empty()) return true;
    if(blockedWord) return false;
    if(!ownerIsFocused()) { status(L"이전 입력창으로 돌아가 계속 입력하세요."); return false; }
    std::wstring text=wide(word->preview());
    std::vector<INPUT> events; events.reserve(text.size()*2+(followKey?2:0));
    for(wchar_t c:text) {
        INPUT event{}; event.type=INPUT_KEYBOARD; event.ki.wScan=c;
        event.ki.dwFlags=KEYEVENTF_UNICODE; event.ki.dwExtraInfo=kOwnInput; events.push_back(event);
        event.ki.dwFlags|=KEYEVENTF_KEYUP; events.push_back(event);
    }
    if(followKey) {
        INPUT event{}; event.type=INPUT_KEYBOARD; event.ki.wVk=(WORD)followKey; event.ki.dwExtraInfo=kOwnInput;
        events.push_back(event); event.ki.dwFlags=KEYEVENTF_KEYUP; events.push_back(event);
    }
    sending=true; UINT sent=SendInput((UINT)events.size(),events.data(),sizeof(INPUT)); sending=false;
    if(sent!=events.size()) {
        blockedWord=true;
        status(L"입력이 일부 전달됐을 수 있습니다. 대상 창을 확인하고 대기 입력을 취소하세요.");
        return false;
    }
    word->finish(); ownerFocus=nullptr; ownerForeground=nullptr; refreshPreview();
    return true;
}
LRESULT CALLBACK keyHook(int code,WPARAM message,LPARAM raw) {
    if(code<0 || !enabled || sending) return CallNextHookEx(nullptr,code,message,raw);
    auto* key=reinterpret_cast<KBDLLHOOKSTRUCT*>(raw);
    if(key->dwExtraInfo==kOwnInput) return CallNextHookEx(nullptr,code,message,raw);
    if(message!=WM_KEYDOWN && message!=WM_SYSKEYDOWN) return CallNextHookEx(nullptr,code,message,raw);
    HWND foreground=GetForegroundWindow(), focus=focusedWindow(foreground);
    if(!word->empty() && (foreground!=ownerForeground || focus!=ownerFocus)) {
        blockedWord=true;
        status(L"입력창이 바뀌었습니다. 대기 입력을 취소한 뒤 다시 입력하세요.");
        refreshPreview(); return CallNextHookEx(nullptr,code,message,raw);
    }
    if(blockedWord && !word->empty()) return CallNextHookEx(nullptr,code,message,raw);
    const bool shift=(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0;
    const bool ctrl=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0;
    const bool alt=(GetAsyncKeyState(VK_MENU)&0x8000)!=0;
    const bool win=((GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&0x8000)!=0;
    WPARAM vk=key->vkCode;
    if(word->empty() && passFocus) {
        if(passFocus!=focus || passForeground!=foreground) { passFocus=nullptr; passForeground=nullptr; }
        else {
            if(vk==VK_SPACE || vk==VK_RETURN || vk==VK_TAB || vk==VK_ESCAPE ||
               (vk>=VK_OEM_1 && vk<=VK_OEM_3) || (vk>=VK_OEM_4 && vk<=VK_OEM_8)) {
                passFocus=nullptr; passForeground=nullptr;
            }
            return CallNextHookEx(nullptr,code,message,raw);
        }
    }
    if(word->empty() && (ctrl||alt||win)) { invalidateFocus(); return CallNextHookEx(nullptr,code,message,raw); }
    if(word->empty() && !(vk==VK_HANGUL || (vk==VK_SPACE && shift) || (vk>='A' && vk<='Z')))
        { if(vk!=VK_SHIFT && vk!=VK_LSHIFT && vk!=VK_RSHIFT) invalidateFocus(); return CallNextHookEx(nullptr,code,message,raw); }
    RECT bounds{};
    // The same focused control owns the complete word. Accessibility calls into Excel
    // are expensive, so check writability once when the word begins, not on every key.
    if(word->empty() && !excelGrid(foreground,focus,&bounds) && !cachedEligible(foreground,focus,&bounds)) {
        // Never intercept halfway through a word that began before a safe focus check completed.
        if(vk>='A' && vk<='Z') { passFocus=focus; passForeground=foreground; }
        return CallNextHookEx(nullptr,code,message,raw);
    }
    if(vk==VK_HANGUL || (vk==VK_SPACE && shift && !ctrl && !alt && !win)) {
        word->toggle(); refreshPreview(); return 1;
    }
    if(ctrl||alt||win) {
        if(ctrl && alt && !shift && !win && vk=='H') return CallNextHookEx(nullptr,code,message,raw);
        if(!word->empty() && vk!=VK_CONTROL && vk!=VK_LCONTROL && vk!=VK_RCONTROL &&
           vk!=VK_MENU && vk!=VK_LMENU && vk!=VK_RMENU && vk!=VK_LWIN && vk!=VK_RWIN) {
            status(L"단축키를 쓰기 전에 스페이스로 단어를 확정하거나 Esc로 취소하세요.");
            return 1;
        }
        return CallNextHookEx(nullptr,code,message,raw);
    }
    if(vk>= 'A' && vk<= 'Z') {
        if(word->empty()) { ownerForeground=foreground; ownerFocus=focus; ownerBounds=bounds; }
        bool caps=(GetKeyState(VK_CAPITAL)&1)!=0;
        char letter=(char)((shift!=caps)?vk:vk+32);
        if(!word->append(letter)) {
            if(!sendWord()) return 1;
            ownerForeground=foreground; ownerFocus=focus; word->append(letter);
        }
        refreshPreview(); return 1;
    }
    if(vk>= '0' && vk<='9' && !shift && !word->empty()) {
        if(!word->append((char)vk)) {
            if(!sendWord()) return 1;
            ownerForeground=foreground; ownerFocus=focus; word->append((char)vk);
        }
        refreshPreview(); return 1;
    }
    if(word->empty()) return CallNextHookEx(nullptr,code,message,raw);
    if(vk==VK_BACK) { word->backspace(); refreshPreview(); return 1; }
    if(vk==VK_ESCAPE) { word->clear(); ownerFocus=nullptr; ownerForeground=nullptr; refreshPreview(); return 1; }
    if(vk==VK_SHIFT || vk==VK_LSHIFT || vk==VK_RSHIFT || vk==VK_CAPITAL) return CallNextHookEx(nullptr,code,message,raw);
    if(sendWord(vk)) return 1;
    return 1;
}
LRESULT CALLBACK mouseHookProc(int code,WPARAM message,LPARAM raw) {
    if(code>=0 && (message==WM_LBUTTONDOWN || message==WM_RBUTTONDOWN || message==WM_MBUTTONDOWN)) {
        invalidateFocus();
        lastClick=reinterpret_cast<MSLLHOOKSTRUCT*>(raw)->pt;
        HWND clicked=WindowFromPoint(lastClick);
        lastClickForeground=clicked?GetAncestor(clicked,GA_ROOT):nullptr;
    }
    if(code>=0 && enabled && word && !word->empty() && !blockedWord &&
       (message==WM_LBUTTONDOWN || message==WM_RBUTTONDOWN || message==WM_MBUTTONDOWN)) {
        blockedWord=true;
        status(L"입력 위치가 바뀌었을 수 있습니다. 대기 입력을 취소한 뒤 다시 입력하세요.");
        refreshPreview();
    }
    return CallNextHookEx(nullptr,code,message,raw);
}
LRESULT CALLBACK previewProc(HWND h,UINT m,WPARAM w,LPARAM l) {
    if(m==WM_PAINT) {
        PAINTSTRUCT ps{}; HDC dc=BeginPaint(h,&ps); RECT r{}; GetClientRect(h,&r);
        HBRUSH bg=CreateSolidBrush(RGB(250,250,250)); FillRect(dc,&r,bg); DeleteObject(bg);
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(35,45,68)); SelectObject(dc,textFont);
        auto text=word ? wide(word->preview()) : L""; r.left+=12; r.right-=10;
        DrawTextW(dc,text.c_str(),-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
        EndPaint(h,&ps); return 0;
    }
    if(m==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    return DefWindowProcW(h,m,w,l);
}
void setEnabled(bool value) {
    if(!value && word && !word->empty()) { status(L"입력 중인 단어를 스페이스로 확정하거나 Esc로 취소하세요."); return; }
    enabled=value; SetWindowTextW(toggleButton,enabled?L"자동 입력 끄기":L"자동 입력 켜기");
    status(enabled?L"켜짐 · 다른 앱의 입력창에서 입력하세요.":L"꺼짐 · Ctrl + Alt + H 또는 버튼으로 시작하세요.");
    InvalidateRect(toggleButton,nullptr,TRUE);
    refreshPreview();
}
LRESULT CALLBACK windowProc(HWND h,UINT m,WPARAM w,LPARAM l) {
    if(m==WM_CREATE) {
        brandLabel=CreateWindowW(L"STATIC",L"Auto-Type",WS_CHILD|WS_VISIBLE,84,20,210,29,h,nullptr,nullptr,nullptr);
        versionLabel=CreateWindowW(L"STATIC",L"v" AUTOTYPE_VERSION,WS_CHILD|WS_VISIBLE|SS_RIGHT,316,25,52,20,h,nullptr,nullptr,nullptr);
        headingLabel=CreateWindowW(L"STATIC",L"한·영 전환",WS_CHILD|WS_VISIBLE,85,51,205,19,h,nullptr,nullptr,nullptr);
        descriptionLabel=CreateWindowW(L"STATIC",L"입력 중인 단어를 보고 자연스럽게 바꿔요",WS_CHILD|WS_VISIBLE,23,86,345,20,h,nullptr,nullptr,nullptr);
        toggleButton=CreateWindowW(L"BUTTON",L"자동 입력 켜기",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,22,117,169,46,h,(HMENU)1,nullptr,nullptr);
        CreateWindowW(L"BUTTON",L"대기 입력 취소",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,199,117,169,46,h,(HMENU)2,nullptr,nullptr);
        statusLabel=CreateWindowW(L"STATIC",L"꺼짐 · 시작하려면 버튼을 눌러주세요",WS_CHILD|WS_VISIBLE|SS_LEFT,47,187,313,30,h,nullptr,nullptr,nullptr);
        helperLabel=CreateWindowW(L"STATIC",L"전역 단축키: Ctrl + Alt + H",WS_CHILD|WS_VISIBLE|SS_CENTER,24,226,342,18,h,nullptr,nullptr,nullptr);
        EnumChildWindows(h,[](HWND child, LPARAM font)->BOOL { SendMessageW(child,WM_SETFONT,(WPARAM)font,TRUE); return TRUE; },(LPARAM)uiFont);
        SendMessageW(brandLabel,WM_SETFONT,(WPARAM)titleFont,TRUE);
        SendMessageW(versionLabel,WM_SETFONT,(WPARAM)microFont,TRUE);
        SendMessageW(headingLabel,WM_SETFONT,(WPARAM)microFont,TRUE);
        SendMessageW(descriptionLabel,WM_SETFONT,(WPARAM)smallFont,TRUE);
        SendMessageW(statusLabel,WM_SETFONT,(WPARAM)smallFont,TRUE);
        SendMessageW(helperLabel,WM_SETFONT,(WPARAM)microFont,TRUE);
        SetTimer(h,1,300,nullptr); return 0;
    }
    if(m==WM_ERASEBKGND) {
        RECT r{}; GetClientRect(h,&r); FillRect((HDC)w,&r,canvasBrush); return 1;
    }
    if(m==WM_PAINT) {
        PAINTSTRUCT ps{}; HDC dc=BeginPaint(h,&ps);
        if(brandIcon) DrawIconEx(dc,22,20,brandIcon,48,48,0,nullptr,DI_NORMAL);
        RECT r{20,182,370,219}; COLORREF card=enabled?RGB(235,247,240):kStatusBg;
        HBRUSH brush=CreateSolidBrush(card); HPEN pen=CreatePen(PS_SOLID,1,card);
        HGDIOBJ oldBrush=SelectObject(dc,brush), oldPen=SelectObject(dc,pen);
        RoundRect(dc,r.left,r.top,r.right,r.bottom,12,12);
        SelectObject(dc,oldBrush); SelectObject(dc,oldPen); DeleteObject(brush); DeleteObject(pen);
        HBRUSH dot=CreateSolidBrush(enabled?RGB(44,160,103):RGB(150,159,174));
        HPEN dotPen=CreatePen(PS_SOLID,1,enabled?RGB(44,160,103):RGB(150,159,174));
        HGDIOBJ oldDot=SelectObject(dc,dot), oldDotPen=SelectObject(dc,dotPen);
        Ellipse(dc,31,196,39,204); SelectObject(dc,oldDot); SelectObject(dc,oldDotPen);
        DeleteObject(dot); DeleteObject(dotPen);
        EndPaint(h,&ps); return 0;
    }
    if(m==WM_CTLCOLORSTATIC) {
        HDC dc=(HDC)w; HWND control=(HWND)l;
        SetBkMode(dc,TRANSPARENT);
        if(control==statusLabel) SetTextColor(dc,enabled?RGB(42,120,82):kMuted);
        else SetTextColor(dc,control==brandLabel?kInk:(control==headingLabel?kBlue:kMuted));
        return (LRESULT)canvasBrush;
    }
    if(m==WM_DRAWITEM) { drawRoundedButton((DRAWITEMSTRUCT*)l); return TRUE; }
    if(m==WM_COMMAND && LOWORD(w)==1) { setEnabled(!enabled); InvalidateRect(h,nullptr,TRUE); return 0; }
    if(m==WM_COMMAND && LOWORD(w)==2) {
        word->clear(); ownerFocus=nullptr; ownerForeground=nullptr; blockedWord=false;
        status(enabled?L"켜짐 · 입력할 곳을 클릭하세요":L"꺼짐 · 시작하려면 버튼을 눌러주세요"); refreshPreview(); InvalidateRect(h,nullptr,TRUE); return 0;
    }
    if(m==WM_HOTKEY && w==(WPARAM)kToggleHotkeyId) {
        setEnabled(!enabled);
        InvalidateRect(h,nullptr,TRUE);
        return 0;
    }
    if(m==WM_TIMER) {
        if(word && !word->empty() && !ownerIsFocused() && !blockedWord) {
            blockedWord=true; status(L"입력창이 바뀌었습니다. 대기 입력을 취소한 뒤 다시 입력하세요.");
        }
        refreshPreview(); return 0;
    }
    if(m==WM_CLOSE && word && !word->empty()) { status(L"입력 중인 단어를 먼저 확정하거나 취소하세요."); return 0; }
    if(m==WM_CLOSE) { DestroyWindow(h); return 0; }
    if(m==WM_DESTROY) {
        if(toggleHotkeyRegistered) {
            UnregisterHotKey(h,kToggleHotkeyId);
            toggleHotkeyRegistered=false;
        }
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\AutoTypePreviewSingleInstance");
    if(!singleton) return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS) { MessageBoxW(nullptr,L"Auto-Type이 이미 실행 중입니다.",L"Auto-Type",MB_OK|MB_ICONINFORMATION); CloseHandle(singleton); return 0; }
    if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED))) return 1;
    HRSRC r=FindResourceW(instance,MAKEINTRESOURCEW(101),RT_RCDATA);
    HGLOBAL data=r?LoadResource(instance,r):nullptr;
    if(!data || !model.load((const uint8_t*)LockResource(data),SizeofResource(instance,r))) return 3;
    word=std::make_unique<PendingWord>(model);
    canvasBrush=CreateSolidBrush(kCanvas);
    textFont=CreateFontW(-23,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Malgun Gothic");
    uiFont=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Malgun Gothic");
    titleFont=CreateFontW(-22,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Malgun Gothic");
    smallFont=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Malgun Gothic");
    microFont=CreateFontW(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Malgun Gothic");
    buttonFont=CreateFontW(-15,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Malgun Gothic");
    brandIcon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(201),IMAGE_ICON,48,48,LR_DEFAULTCOLOR);
    WNDCLASSW wc{}; wc.hInstance=instance; wc.lpszClassName=L"AutoTypePreviewWord"; wc.lpfnWndProc=previewProc; wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); RegisterClassW(&wc);
    previewWindow=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"",WS_POPUP|WS_BORDER,0,0,380,52,nullptr,nullptr,instance,nullptr);
    wc.lpszClassName=L"AutoTypeStandaloneWindow"; wc.lpfnWndProc=windowProc; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(201)); RegisterClassW(&wc);
    DWORD windowStyle=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    RECT windowRect{0,0,390,252}; AdjustWindowRectEx(&windowRect,windowStyle,FALSE,0);
    mainWindow=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,L"Auto-Type v" AUTOTYPE_VERSION,windowStyle,100,100,
        windowRect.right-windowRect.left,windowRect.bottom-windowRect.top,nullptr,nullptr,instance,nullptr);
    if(mainWindow) {
        toggleHotkeyRegistered=RegisterHotKey(mainWindow,kToggleHotkeyId,kToggleHotkeyModifiers,kToggleHotkeyKey)!=FALSE;
        if(!toggleHotkeyRegistered) {
            status(L"Ctrl + Alt + H를 등록하지 못했습니다. 앱 버튼을 사용하세요.");
            SetWindowTextW(helperLabel,L"단축키 사용 불가 · 앱 버튼을 사용하세요");
        }
    }
    monitorStop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(monitorStop) monitorThread=CreateThread(nullptr,0,watchFocus,nullptr,0,nullptr);
    keyboardHook=SetWindowsHookExW(WH_KEYBOARD_LL,keyHook,instance,0);
    mouseHook=SetWindowsHookExW(WH_MOUSE_LL,mouseHookProc,instance,0);
    if(!keyboardHook || !mouseHook || !monitorThread) { MessageBoxW(mainWindow,L"입력 장치를 연결할 수 없습니다.",L"Auto-Type",MB_ICONERROR); DestroyWindow(mainWindow); }
    else ShowWindow(mainWindow,show);
    MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if(keyboardHook) UnhookWindowsHookEx(keyboardHook);
    if(mouseHook) UnhookWindowsHookEx(mouseHook);
    if(previewWindow) DestroyWindow(previewWindow);
    if(textFont) DeleteObject(textFont);
    if(uiFont) DeleteObject(uiFont);
    if(titleFont) DeleteObject(titleFont);
    if(smallFont) DeleteObject(smallFont);
    if(microFont) DeleteObject(microFont);
    if(buttonFont) DeleteObject(buttonFont);
    if(brandIcon) DestroyIcon(brandIcon);
    if(canvasBrush) DeleteObject(canvasBrush);
    if(monitorStop) SetEvent(monitorStop);
    DWORD monitorExit=monitorThread?WaitForSingleObject(monitorThread,2000):WAIT_OBJECT_0;
    if(monitorThread) CloseHandle(monitorThread);
    // A stuck third-party accessibility provider may outlive the timeout. Keep its
    // event valid until process exit instead of closing a handle it may still use.
    if(monitorStop && monitorExit==WAIT_OBJECT_0) CloseHandle(monitorStop);
    CoUninitialize(); CloseHandle(singleton); return 0;
}
