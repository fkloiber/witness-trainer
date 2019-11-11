#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <TlHelp32.h>
#include <Psapi.h>

#include <cwchar>

#include "aob.hpp"
#include "foreign_process_memory.hpp"

UINT_PTR g_ConnectTimer = 0;
ForeignProcessMemory g_WitnessMemory;

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
            g_WitnessMemory.Disconnect();
            PostQuitMessage(0);
        } break;
        case WM_TIMER: {
            if (g_ConnectTimer && WParam == g_ConnectTimer && ConnectToGameProcess()) {
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

const AOB DetourAOB("FF 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 84 C0");
const AOB GetThePlayerCallAOB("E8 ?? ?? ?? ?? 48 85 C0 74 ?? F2 0F 10 40");
const AOB GetCameraParametersFunctionAOB("48 85 C9 74 0C F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 01 48 85 D2 74 0C F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 02 C3");

struct DetourParameters {
    // input parameters
    uintptr_t DetourAddress;
    uintptr_t GetThePlayerFunction;
    uintptr_t ThetaAddress, PhiAddress;
    size_t DetourWholeInstructionBytes;
    uint8_t XPositionOffset;

    // output parameters
} g_DetourParameters;
bool ConnectToGameProcess() {
    HANDLE WitnessProcessHandle = TryOpenProcess(L"witness64_d3d11.exe");
    if (!WitnessProcessHandle) {
        return false;
    }
    g_WitnessMemory.Connect(WitnessProcessHandle);

    g_DetourParameters = {};

    g_DetourParameters.DetourAddress = g_WitnessMemory.FindFirstOccurrenceInMainModule(DetourAOB);

    uintptr_t GetThePlayerCallAddress = g_WitnessMemory.FindFirstOccurrenceInMainModule(GetThePlayerCallAOB);
    auto RelativeFunctionAddress = g_WitnessMemory.ReadValue<int32_t>(GetThePlayerCallAddress + 1);
    g_DetourParameters.GetThePlayerFunction = GetThePlayerCallAddress + 5 + RelativeFunctionAddress;
    g_DetourParameters.XPositionOffset = g_WitnessMemory.ReadValue<uint8_t>(GetThePlayerCallAddress + 14);

    uintptr_t GetCameraParametersFunctionAddress = g_WitnessMemory.FindFirstOccurrenceInMainModule(GetCameraParametersFunctionAOB);
    auto RelativeAddressTheta = g_WitnessMemory.ReadValue<int32_t>(GetCameraParametersFunctionAddress + 9);
    auto RelativeAddressPhi = g_WitnessMemory.ReadValue<int32_t>(GetCameraParametersFunctionAddress + 26);
    g_DetourParameters.ThetaAddress = GetCameraParametersFunctionAddress + 13 + RelativeAddressTheta;
    g_DetourParameters.PhiAddress = GetCameraParametersFunctionAddress + 30 + RelativeAddressPhi;

    return true;
}
