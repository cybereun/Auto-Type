#pragma once

#include <string>

#include <windows.h>

namespace autotype {

class PreviewWindow {
public:
    bool create();
    bool show(const std::u32string& text, const RECT& caret);
    void hide();
    void destroy();

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT paint(HWND window);

    HWND window_ = nullptr;
    std::wstring text_;
};

}  // namespace autotype
