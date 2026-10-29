#include "PreviewWindow.h"

#include <algorithm>

#include "Globals.h"

namespace autotype {
namespace {
constexpr wchar_t kWindowClass[] = L"AutoTypeTSFWordPreview_0_2_8";
constexpr int kWidth = 340;
constexpr int kHeight = 58;
constexpr COLORREF kBackground = RGB(255, 255, 255);
constexpr COLORREF kBorder = RGB(224, 229, 240);
constexpr COLORREF kAccent = RGB(63, 91, 214);
constexpr COLORREF kText = RGB(35, 45, 68);
}

bool PreviewWindow::create() {
    if (window_) return true;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = g_module;
    wc.lpfnWndProc = windowProc;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    window_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                              kWindowClass, L"", WS_POPUP, 0, 0, kWidth, kHeight,
                              nullptr, nullptr, g_module, this);
    if (window_) {
        HRGN region = CreateRoundRectRgn(0, 0, kWidth + 1, kHeight + 1, 12, 12);
        if (region && !SetWindowRgn(window_, region, TRUE)) DeleteObject(region);
    } else {
        UnregisterClassW(kWindowClass, g_module);
    }
    return window_ != nullptr;
}

bool PreviewWindow::show(const std::u32string& text, const RECT& caret) {
    if (!window_ || text.empty()) {
        hide();
        return false;
    }
    const std::u16string utf16 = toUtf16(text);
    text_.assign(utf16.begin(), utf16.end());

    RECT work{};
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    const HMONITOR target = MonitorFromRect(&caret, MONITOR_DEFAULTTONEAREST);
    if (GetMonitorInfoW(target, &monitor)) work = monitor.rcWork;
    else SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);

    int x = std::clamp(static_cast<int>(caret.left), static_cast<int>(work.left),
                       static_cast<int>(work.right) - kWidth);
    int y = static_cast<int>(caret.bottom) + 7;
    if (y + kHeight > work.bottom) y = static_cast<int>(caret.top) - kHeight - 7;
    y = std::clamp(y, static_cast<int>(work.top), static_cast<int>(work.bottom) - kHeight);

    if (!SetWindowPos(window_, HWND_TOPMOST, x, y, kWidth, kHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW)) return false;
    InvalidateRect(window_, nullptr, TRUE);
    UpdateWindow(window_);
    return true;
}

void PreviewWindow::hide() {
    if (window_) ShowWindow(window_, SW_HIDE);
}

void PreviewWindow::destroy() {
    if (window_) DestroyWindow(window_);
    window_ = nullptr;
    text_.clear();
    UnregisterClassW(kWindowClass, g_module);
}

LRESULT CALLBACK PreviewWindow::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    PreviewWindow* self = reinterpret_cast<PreviewWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<PreviewWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT && self) return self->paint(window);
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT PreviewWindow::paint(HWND window) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(window, &ps);
    RECT bounds{};
    GetClientRect(window, &bounds);
    FillRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    HBRUSH background = CreateSolidBrush(kBackground);
    HPEN border = CreatePen(PS_SOLID, 1, kBorder);
    HGDIOBJ oldBrush = SelectObject(dc, background);
    HGDIOBJ oldPen = SelectObject(dc, border);
    RoundRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom, 12, 12);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(background);
    DeleteObject(border);

    HBRUSH accent = CreateSolidBrush(kAccent);
    RECT accentRect{1, 12, 4, bounds.bottom - 12};
    FillRect(dc, &accentRect, accent);
    DeleteObject(accent);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, kText);
    HFONT font = CreateFontW(-21, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, 0, 0, 0, CLEARTYPE_QUALITY, L"Malgun Gothic");
    HGDIOBJ oldFont = SelectObject(dc, font);
    RECT text = bounds;
    text.left += 18;
    text.right -= 14;
    DrawTextW(dc, text_.c_str(), -1, &text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, oldFont);
    DeleteObject(font);
    EndPaint(window, &ps);
    return 0;
}

}  // namespace autotype
