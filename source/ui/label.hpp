#pragma once

#include <Windows.h>
#include <fmt/format.h>

extern HFONT g_UiMonoFont;

struct Label {
    Label() : Handle(nullptr) {}
    Label(Label && o) : Handle(o.Handle), Font(o.Font), Bitmap(o.Bitmap), Text(std::move(o.Text)) {
        SetWindowLongPtrA(Handle, 0, (LONG_PTR)this);
    }
    static Label New(int X, int Y, int Width, int Height, const char * Text, HFONT * Font = &g_UiMonoFont);

    Label& operator=(Label && o) {
        Handle = o.Handle;
        Font = o.Font;
        Bitmap = o.Bitmap;
        Text = std::move(o.Text);
        SetWindowLongPtrA(Handle, 0, (LONG_PTR)this);
        return *this;
    }

    template<typename S, typename... Args>
    void SetText(const S &FormatString, Args&&... Arguments) {
        Text.clear();
        fmt::format_to(Text, FormatString, Arguments...);
        SendMessageA(Handle, WM_SETTEXT, 0, 0);
    }

    HWND Handle;
    HFONT Font;
    HBITMAP Bitmap;
    fmt::memory_buffer Text;

private:
    Label(int X, int Y, int W, int H, const char * InitialText, HFONT Font);
};
