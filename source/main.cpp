#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Psapi.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <fmt/format.h>

#include <cwchar>
#include <vector>
#include <cstdio>
#include <memory>

#include "game_interface.hpp"

#define CONNECT_TIMER_ID 5
#define UI_TIMER_ID 6

std::unique_ptr<GameInterface> g_GameInterface = nullptr;
HINSTANCE g_Instance = nullptr;
HWND g_Window = nullptr;
HFONT g_UiFont = nullptr;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void SetupConnectTimer(HWND);
void KillConnectTimer(HWND);
bool ConnectToGameProcess();

struct MutLabel {
    MutLabel() = default;
    explicit MutLabel(HWND Handle) : Handle(Handle) {}
    template<typename S, typename... Args>
    void ChangeText(const S &FormatString, Args&&... Arguments) {
        Buffer.clear();
        fmt::format_to(Buffer, FormatString, Arguments...);
        SetWindowTextA(Handle, Buffer.data());
    }
private:
    HWND Handle;
    fmt::memory_buffer Buffer;
};

void CreateLabel(int X, int Y, int Width, int Height, char * Text) {
    HWND Handle = CreateWindowA("STATIC", Text, WS_VISIBLE | WS_CHILD, X, Y, Width, Height, g_Window, nullptr, g_Instance, nullptr);
    SendMessageA(Handle, WM_SETFONT, (WPARAM)g_UiFont, TRUE);
}
MutLabel CreateMutLabel(int X, int Y, int Width, int Height, char * Text) {
    HWND Handle = CreateWindowA("STATIC", Text, WS_VISIBLE | WS_CHILD, X, Y, Width, Height, g_Window, nullptr, g_Instance, nullptr);
    SendMessageA(Handle, WM_SETFONT, (WPARAM)g_UiFont, TRUE);
    return MutLabel(Handle);
}

struct UiState {
    MutLabel XLbl, YLbl, ZLbl;
} g_UiState;

int CALLBACK WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CommandLine,
    int Show
) {
    g_Instance = Instance;
    static const auto ClassName = L"WitnessTrainer";

    WNDCLASSW WindowClass = {};
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = WndProc;
    WindowClass.hInstance = Instance;
    WindowClass.hIcon = LoadIcon(Instance, IDI_APPLICATION);
    WindowClass.hCursor = LoadCursor(Instance, IDC_ARROW);
    WindowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    WindowClass.lpszMenuName = nullptr;
    WindowClass.lpszClassName = ClassName;

    if (!RegisterClassW(&WindowClass)) {
        MessageBoxW(nullptr, L"Window Creation Failed", L"This really shouldn't happen...", 0);
        return 1;
    }

    g_Window = CreateWindowW(
        ClassName,
        L"Witness Trainer",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 500,
        nullptr,
        nullptr,
        Instance,
        nullptr
    );

    g_UiFont = CreateFontA(
        -16, 0, 0, 0,
        FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Segoe UI"
    );

    CreateLabel(10, 10, 60, 20, "Position: ");

    g_UiState.XLbl = CreateMutLabel(75, 10, 80, 20, "X: ??");
    g_UiState.YLbl = CreateMutLabel(75, 30, 80, 20, "Y: ??");
    g_UiState.ZLbl = CreateMutLabel(75, 50, 80, 20, "Z: ??");

    ShowWindow(g_Window, SW_SHOW);
    SetupConnectTimer(g_Window);
    SetTimer(g_Window, UI_TIMER_ID, 20, nullptr);

    MSG Message;
    while (GetMessageW(&Message, nullptr, 0, 0)) {
        TranslateMessage(&Message);
        DispatchMessageW(&Message);
    }

    return (int) Message.wParam;
}

LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    switch (Message) {
        case WM_DESTROY: {
            KillConnectTimer(Window);
            g_GameInterface.reset();
            PostQuitMessage(0);
        } break;
        case WM_TIMER: {
            if (WParam == CONNECT_TIMER_ID && ConnectToGameProcess()) {
                KillConnectTimer(Window);
            } else if (WParam == UI_TIMER_ID && g_GameInterface) {
                if (!g_GameInterface->ProcessRunning()) {
                    g_GameInterface.reset();
                    SetupConnectTimer(Window);
                    break;
                }
                g_GameInterface->DoRead();
                g_UiState.XLbl.ChangeText(FMT_STRING("X: {: = 5.1f}"), g_GameInterface->X());
                g_UiState.YLbl.ChangeText(FMT_STRING("Y: {: = 5.1f}"), g_GameInterface->Y());
                g_UiState.ZLbl.ChangeText(FMT_STRING("Z: {: = 5.1f}"), g_GameInterface->Z());
            }
        } break;
        case WM_CTLCOLORSTATIC: {
            HDC Hdc = (HDC)WParam;
            SetTextColor(Hdc, GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor(Hdc, GetSysColor(COLOR_WINDOW));
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        } break;
        default: {
            return DefWindowProcW(Window, Message, WParam, LParam);
        } break;
    }

    return 0;
}

void SetupConnectTimer(HWND Window) {
    bool Success = SetTimer(Window, CONNECT_TIMER_ID, 500, nullptr);
    if (Success) {
        PostMessageW(Window, WM_TIMER, CONNECT_TIMER_ID, 0);
    }
}

void KillConnectTimer(HWND Window) {
    KillTimer(Window, CONNECT_TIMER_ID);
}

bool ConnectToGameProcess() {
    g_GameInterface = GameInterface::New();
    if (!!g_GameInterface) {
        std::string title = fmt::format(FMT_STRING("Witness Trainer - Connected to PID {}"), g_GameInterface->ProcessId());
        SetWindowTextA(g_Window, title.data());
    }
    return !!g_GameInterface;
}
