#include "label.hpp"

static const char * ClassName = "WitnessLabel";

extern HWND g_Window;
extern HINSTANCE g_Instance;

static LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);

Label Label::New(int X, int Y, int Width, int Height, const char * Text, HFONT * Font) {
    static bool Initialized = false;
    if (!Initialized) {
        Initialized = true;

        WNDCLASSA WindowClass = {};
        WindowClass.style = CS_HREDRAW | CS_VREDRAW;
        WindowClass.lpfnWndProc = WndProc;
        WindowClass.cbWndExtra = sizeof(void*);
        WindowClass.hInstance = g_Instance;
        WindowClass.hIcon = LoadIcon(g_Instance, IDI_APPLICATION);
        WindowClass.hCursor = LoadCursor(g_Instance, IDC_ARROW);
        WindowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        WindowClass.lpszMenuName = nullptr;
        WindowClass.lpszClassName = ClassName;

        RegisterClassA(&WindowClass);
    }

    return Label(X, Y, Width, Height, Text, *Font);
}

Label::Label(int X, int Y, int Width, int Height, const char * InitialText, HFONT Font) :
    Font(Font),
    Bitmap(nullptr)
{
    fmt::format_to(Text, InitialText);
    Handle = CreateWindowA(ClassName, "", WS_VISIBLE | WS_CHILD, X, Y, Width, Height, g_Window, nullptr, g_Instance, this);
}

static LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    auto This = (Label *) GetWindowLongPtrA(Window, 0);
    switch (Message) {
        case WM_CREATE: {
            auto CreateStruct = (CREATESTRUCT *) LParam;
            SetWindowLongPtrA(Window, 0, (LONG_PTR)CreateStruct->lpCreateParams);
            return 0;
        } break;
        case WM_SETTEXT: {
            InvalidateRect(Window, nullptr, FALSE);
        } break;
        case WM_SETFONT: {
            This->Font = (HFONT) WParam;
        } break;
        case WM_SIZE: {
            HDC Hdc = GetDC(Window);
            RECT Rect;
            GetClientRect(Window, &Rect);
            if (This->Bitmap) {
                DeleteObject(This->Bitmap);
            }
            This->Bitmap = CreateCompatibleBitmap(Hdc, Rect.right - Rect.left, Rect.bottom - Rect.top);
            ReleaseDC(Window, Hdc);
            return 0;
        } break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            RECT Rect;
            HDC HdcMem;
            HBITMAP OldBitmap;
            HFONT OldFont = nullptr;
            HBRUSH Background;

            BeginPaint(Window, &ps);
            GetClientRect(Window, &Rect);
            HdcMem = CreateCompatibleDC(ps.hdc);
            if (!This->Bitmap) {
                This->Bitmap = CreateCompatibleBitmap(HdcMem, Rect.right - Rect.left, Rect.bottom - Rect.top);
            }
    	    OldBitmap = (HBITMAP) SelectObject(HdcMem, This->Bitmap);
            Background = GetSysColorBrush(COLOR_WINDOW);
            FillRect(HdcMem, &Rect, Background);
            if (This->Font) {
                OldFont = (HFONT) SelectObject(HdcMem, This->Font);
            }
            SetBkMode(HdcMem, TRANSPARENT);
            SetTextColor(HdcMem, GetSysColor(COLOR_WINDOWTEXT));
            DrawTextA(HdcMem, This->Text.data(), (int)This->Text.size(), &Rect, DT_LEFT | DT_TOP);
            if (OldFont) {
                SelectObject(HdcMem, OldFont);
            }
            BitBlt(ps.hdc, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, HdcMem, 0, 0, SRCCOPY);
            SelectObject(HdcMem, OldBitmap);
            DeleteDC(HdcMem);
            EndPaint(Window, &ps);
        } break;
        default: {
            return DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }

    return 0;
}
