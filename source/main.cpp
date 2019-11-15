#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Psapi.h>
#include <fmt/format.h>

#include <cwchar>
#include <vector>
#include <cstdio>
#include <memory>

#include "game_interface.hpp"
#include "ui/label.hpp"

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

struct UiState {
    Label XLbl, YLbl, ZLbl;
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

    Label::New(10, 10, 60, 20, "Position: ");

    g_UiState.XLbl = Label::New(75, 10, 200, 20, "X: ??");
    g_UiState.YLbl = Label::New(75, 30, 200, 20, "Y: ??");
    g_UiState.ZLbl = Label::New(75, 50, 200, 20, "Z: ??");

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

static LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
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
                g_UiState.XLbl.SetText("X: {: = 6.1f}", g_GameInterface->X());
                g_UiState.YLbl.SetText("Y: {: = 6.1f}", g_GameInterface->Y());
                g_UiState.ZLbl.SetText("Z: {: = 6.1f}", g_GameInterface->Z());
            }
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
