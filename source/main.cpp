#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Psapi.h>

#include <cwchar>
#include <vector>
#include <cstdio>
#include <memory>

#include "game_interface.hpp"

#define CONNECT_TIMER_ID 5

std::unique_ptr<GameInterface> g_GameInterface = nullptr;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void SetupConnectTimer(HWND);
void KillConnectTimer(HWND);
bool ConnectToGameProcess();

int CALLBACK WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CommandLine,
    int Show
) {
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

    HWND Window = CreateWindowW(
        ClassName,
        L"The Witness - Trainer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 500,
        nullptr,
        nullptr,
        Instance,
        nullptr
    );

    ShowWindow(Window, SW_SHOW);
    SetupConnectTimer(Window);

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
    return !!g_GameInterface;
}
