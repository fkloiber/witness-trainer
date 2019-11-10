#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <TlHelp32.h>
#include <Psapi.h>

#include <cwchar>

#include "aob.hpp"

UINT_PTR g_ConnectTimer = 0;
HANDLE g_ProcessHandle = nullptr;
HMODULE g_MainModule = nullptr;

HANDLE TryOpenProcess(const wchar_t * ProcessName);
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

    auto aob = AOB("AF 909F ?? b9");

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
            if (g_ProcessHandle) {
                CloseHandle(g_ProcessHandle);
                g_ProcessHandle = nullptr;
                g_MainModule = nullptr;
            }
            PostQuitMessage(0);
        } break;
        case WM_TIMER: {
            if (WParam == g_ConnectTimer && ConnectToGameProcess()) {
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
    if (g_ConnectTimer == 0) {
        g_ConnectTimer = SetTimer(Window, 0, 500, nullptr);
        if (g_ConnectTimer) {
            PostMessageW(Window, WM_TIMER, g_ConnectTimer, 0);
        }
    }
}

void KillConnectTimer(HWND Window) {
    if (g_ConnectTimer != 0) {
        KillTimer(Window, g_ConnectTimer);
        g_ConnectTimer = 0;
    }
}

HANDLE TryOpenProcess(const wchar_t * ProcessName) {
    PROCESSENTRY32W Entry = {};
    Entry.dwSize = sizeof(PROCESSENTRY32W);

    HANDLE ProcessSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    if (Process32FirstW(ProcessSnapshot, &Entry) == TRUE) {
        do {
            if (wcsncmp(Entry.szExeFile, ProcessName, MAX_PATH) == 0) {
                HANDLE Result = OpenProcess(PROCESS_ALL_ACCESS, FALSE, Entry.th32ProcessID);
                CloseHandle(ProcessSnapshot);
                return Result;
            }
        } while (Process32NextW(ProcessSnapshot, &Entry) == TRUE);
    }
    CloseHandle(ProcessSnapshot);
    return nullptr;
}

bool ConnectToGameProcess() {
    HANDLE WitnessProcessHandle = TryOpenProcess(L"witness64_d3d11.exe");
    if (!WitnessProcessHandle) {
        return false;
    }
    HMODULE WitnessMainModule;
    EnumProcessModules(WitnessProcessHandle, &WitnessMainModule, sizeof(HMODULE), nullptr);
    g_ProcessHandle = WitnessProcessHandle;
    g_MainModule = WitnessMainModule;

    return true;
}
