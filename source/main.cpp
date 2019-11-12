#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <TlHelp32.h>
#include <Psapi.h>

#include <cwchar>
#include <vector>
#include <cstdio>

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



const AOB DetourAOB("66 0F 6E C1 F3 0F E6 D2 F3 0F E6 C0 F2 41 0F 59 D0");
//const AOB DetourAOB("FF 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 84 C0");
const AOB GetThePlayerCallAOB("E8 ?? ?? ?? ?? 48 85 C0 74 ?? F2 0F 10 40");
const AOB GetCameraParametersFunctionAOB("48 85 C9 74 0C F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 01 48 85 D2 74 0C F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 02 C3");


struct DetourParameters {
    // input parameters
    uintptr_t DetourAddress;
    uintptr_t GetThePlayerFunction;
    uintptr_t ThetaAddress, PhiAddress;
    size_t DetourWholeInstructionBytes;
    uint8_t XPositionOffset;
    uintptr_t ReadAddress;
    uintptr_t WriteAddress;
} g_DetourParameters;

struct ReadData {
    float X, Y, Z, Theta, Phi;
};
struct WriteData {
    float X;
    float Y;
    float Z;
    float Theta;
    float Phi;
    bool WriteAny;
    bool WriteX;
    bool WriteY;
    bool WriteZ;
    bool WriteTheta;
    bool WritePhi;
};

#define OFFSETOF(s, m) ((uint8_t)(uintptr_t)&(((s*)0)->m))
std::vector<uint8_t> AssembleDetour() {
    const static uint8_t FragmentTemplate[] = {
        /* 000: */ 0x50,                                    // push   rax
        /* 001: */ 0x53,                                    // push   rbx
        /* 002: */ 0x51,                                    // push   rcx
        /* 003: */ 0x52,                                    // push   rdx
        /* 004: */ 0x57,                                    // push   rdi
        /* 005: */ 0x48,0x8b,0x3d,0x9b,0x00,0x00,0x00,      // mov    rdi,QWORD PTR [rip+0x9b]        # a7 <GetPlayer>
        /* 00c: */ 0xff,0xd7,                               // call   rdi
        /* 00e: */ 0x48,0x8b,0x3d,0xaa,0x00,0x00,0x00,      // mov    rdi,QWORD PTR [rip+0xaa]        # bf <Read>
        /* 015: */ 0x8b,0x58,0x01,                          // mov    ebx,DWORD PTR [rax+0x??]
        /* 018: */ 0x8b,0x48,0x01,                          // mov    ecx,DWORD PTR [rax+0x??]
        /* 01b: */ 0x8b,0x50,0x01,                          // mov    edx,DWORD PTR [rax+0x??]
        /* 01e: */ 0x89,0x5f,0x01,                          // mov    DWORD PTR [rdi+0x??],ebx
        /* 021: */ 0x89,0x4f,0x01,                          // mov    DWORD PTR [rdi+0x??],ecx
        /* 024: */ 0x89,0x57,0x01,                          // mov    DWORD PTR [rdi+0x??],edx
        /* 027: */ 0x48,0x8b,0x1d,0x81,0x00,0x00,0x00,      // mov    rbx,QWORD PTR [rip+0x81]        # af <Theta>
        /* 02e: */ 0x48,0x8b,0x0d,0x82,0x00,0x00,0x00,      // mov    rcx,QWORD PTR [rip+0x82]        # b7 <Phi>
        /* 035: */ 0x8b,0x1b,                               // mov    ebx,DWORD PTR [rbx]
        /* 037: */ 0x8b,0x09,                               // mov    ecx,DWORD PTR [rcx]
        /* 039: */ 0x89,0x5f,0x01,                          // mov    DWORD PTR [rdi+0x??],ebx
        /* 03c: */ 0x89,0x4f,0x01,                          // mov    DWORD PTR [rdi+0x??],ecx
        /* 03f: */ 0x48,0x8b,0x3d,0x81,0x00,0x00,0x00,      // mov    rdi,QWORD PTR [rip+0x81]        # c7 <Write>
        /* 046: */ 0x80,0x7f,0x01,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
        /* 04a: */ 0x74,0x48,                               // je     94 <Exit>
        /* 04c: */ 0x80,0x7f,0x01,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
        /* 050: */ 0x74,0x06,                               // je     58 <SkipX>
        /* 052: */ 0x8b,0x5f,0x01,                          // mov    ebx,DWORD PTR [rdi+0x??]
        /* 055: */ 0x89,0x58,0x01,                          // mov    DWORD PTR [rax+0x??],ebx
        // 0000000000000058 <SkipX>:
        /* 058: */ 0x80,0x7f,0x01,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
        /* 05c: */ 0x74,0x06,                               // je     64 <SkipY>
        /* 05e: */ 0x8b,0x5f,0x01,                          // mov    ebx,DWORD PTR [rdi+0x??]
        /* 061: */ 0x89,0x58,0x01,                          // mov    DWORD PTR [rax+0x??],ebx
        // 0000000000000064 <SkipY>:
        /* 064: */ 0x80,0x7f,0x01,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
        /* 068: */ 0x74,0x06,                               // je     70 <SkipZ>
        /* 06a: */ 0x8b,0x5f,0x01,                          // mov    ebx,DWORD PTR [rdi+0x??]
        /* 06d: */ 0x89,0x58,0x01,                          // mov    DWORD PTR [rax+0x??],ebx
        // 0000000000000070 <SkipZ>:
        /* 070: */ 0x80,0x7f,0x01,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
        /* 074: */ 0x74,0x0c,                               // je     82 <SkipTheta>
        /* 076: */ 0x48,0x8b,0x1d,0x32,0x00,0x00,0x00,      // mov    rbx,QWORD PTR [rip+0x32]        # af <Theta>
        /* 07d: */ 0x8b,0x5f,0x01,                          // mov    ebx,DWORD PTR [rdi+0x??]
        /* 080: */ 0x89,0x1b,                               // mov    DWORD PTR [rbx],ebx
        // 0000000000000082 <SkipTheta>:
        /* 082: */ 0x80,0x7f,0x01,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
        /* 086: */ 0x74,0x0c,                               // je     94 <Exit>
        /* 088: */ 0x48,0x8b,0x0d,0x28,0x00,0x00,0x00,      // mov    rcx,QWORD PTR [rip+0x28]        # b7 <Phi>
        /* 08f: */ 0x8b,0x5f,0x01,                          // mov    ebx,DWORD PTR [rdi+0x??]
        /* 092: */ 0x89,0x19,                               // mov    DWORD PTR [rcx],ebx
        // 0000000000000094 <Exit>:
        /* 094: */ 0x5f,                                    // pop    rdi
        /* 095: */ 0x5a,                                    // pop    rdx
        /* 096: */ 0x59,                                    // pop    rcx
        /* 097: */ 0x5b,                                    // pop    rbx
        /* 098: */ 0x58,                                    // pop    rax
        /* 099: */ 0xff,0x25,0x00,0x00,0x00,0x00,           // jmp    QWORD PTR [rip+0x0]        # 9f <Exit+0xb>
        /* 09f: */ 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, // .quad 0xcccccccccccccccc
        // 00000000000000a7 <GetPlayer>:
        /* 0a7: */ 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, // .quad 0xcccccccccccccccc
        // 00000000000000af <Theta>:
        /* 0af: */ 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, // .quad 0xcccccccccccccccc
        // 00000000000000b7 <Phi>:
        /* 0b7: */ 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, // .quad 0xcccccccccccccccc
        // 00000000000000bf <Read>:
        /* 0bf: */ 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, // .quad 0xcccccccccccccccc
        // 00000000000000c7 <Write>:
        /* 0c7: */ 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, // .quad 0xcccccccccccccccc
    };

    std::vector<uint8_t> Detour(sizeof(FragmentTemplate) + g_DetourParameters.DetourWholeInstructionBytes, '\0');
    g_WitnessMemory.ReadBuffer(Detour.data(), g_DetourParameters.DetourWholeInstructionBytes, g_DetourParameters.DetourAddress);
    memcpy(Detour.data() + g_DetourParameters.DetourWholeInstructionBytes, FragmentTemplate, sizeof(FragmentTemplate));

    uint8_t * Data = Detour.data() + g_DetourParameters.DetourWholeInstructionBytes;

    Data[0x17] = g_DetourParameters.XPositionOffset + 0;
    Data[0x1a] = g_DetourParameters.XPositionOffset + 4;
    Data[0x1d] = g_DetourParameters.XPositionOffset + 8;
    Data[0x20] = OFFSETOF(ReadData, X);
    Data[0x23] = OFFSETOF(ReadData, Y);
    Data[0x26] = OFFSETOF(ReadData, Z);
    Data[0x3b] = OFFSETOF(ReadData, Theta);
    Data[0x3e] = OFFSETOF(ReadData, Phi);
    Data[0x48] = OFFSETOF(WriteData, WriteAny);
    Data[0x4e] = OFFSETOF(WriteData, WriteX);
    Data[0x54] = OFFSETOF(WriteData, X);
    Data[0x57] = g_DetourParameters.XPositionOffset + 0;
    Data[0x5a] = OFFSETOF(WriteData, WriteY);
    Data[0x60] = OFFSETOF(WriteData, Y);
    Data[0x63] = g_DetourParameters.XPositionOffset + 4;
    Data[0x66] = OFFSETOF(WriteData, WriteZ);
    Data[0x6c] = OFFSETOF(WriteData, Z);
    Data[0x6f] = g_DetourParameters.XPositionOffset + 8;
    Data[0x72] = OFFSETOF(WriteData, WriteTheta);
    Data[0x7f] = OFFSETOF(WriteData, Theta);
    Data[0x84] = OFFSETOF(WriteData, WritePhi);
    Data[0x91] = OFFSETOF(WriteData, Phi);
    *((uintptr_t*)&Data[0x9f]) = g_DetourParameters.DetourAddress + g_DetourParameters.DetourWholeInstructionBytes;
    *((uintptr_t*)&Data[0xa7]) = g_DetourParameters.GetThePlayerFunction;
    *((uintptr_t*)&Data[0xaf]) = g_DetourParameters.ThetaAddress;
    *((uintptr_t*)&Data[0xb7]) = g_DetourParameters.PhiAddress;
    *((uintptr_t*)&Data[0xbf]) = g_DetourParameters.ReadAddress;
    *((uintptr_t*)&Data[0xc7]) = g_DetourParameters.WriteAddress;

    return Detour;
}
#undef OFFSETOF

bool ConnectToGameProcess() {
    HANDLE WitnessProcessHandle = TryOpenProcess(L"witness64_d3d11.exe");
    if (!WitnessProcessHandle) {
        return false;
    }
    g_WitnessMemory.Connect(WitnessProcessHandle);

    g_DetourParameters = {};

    g_DetourParameters.DetourAddress = g_WitnessMemory.FindFirstOccurrenceInMainModule(DetourAOB);
    g_DetourParameters.DetourWholeInstructionBytes = DetourAOB.Size();

    uintptr_t GetThePlayerCallAddress = g_WitnessMemory.FindFirstOccurrenceInMainModule(GetThePlayerCallAOB);
    auto RelativeFunctionAddress = g_WitnessMemory.ReadValue<int32_t>(GetThePlayerCallAddress + 1);
    g_DetourParameters.GetThePlayerFunction = GetThePlayerCallAddress + 5 + RelativeFunctionAddress;
    g_DetourParameters.XPositionOffset = g_WitnessMemory.ReadValue<uint8_t>(GetThePlayerCallAddress + 14);

    uintptr_t GetCameraParametersFunctionAddress = g_WitnessMemory.FindFirstOccurrenceInMainModule(GetCameraParametersFunctionAOB);
    auto RelativeAddressTheta = g_WitnessMemory.ReadValue<int32_t>(GetCameraParametersFunctionAddress + 9);
    auto RelativeAddressPhi = g_WitnessMemory.ReadValue<int32_t>(GetCameraParametersFunctionAddress + 26);
    g_DetourParameters.ThetaAddress = GetCameraParametersFunctionAddress + 13 + RelativeAddressTheta;
    g_DetourParameters.PhiAddress = GetCameraParametersFunctionAddress + 30 + RelativeAddressPhi;

    uintptr_t ReadWriteBuffers = g_WitnessMemory.AllocateMemory(sizeof(ReadData) + sizeof(WriteData), PAGE_READWRITE);

    g_DetourParameters.ReadAddress = ReadWriteBuffers + 0;
    g_DetourParameters.WriteAddress = ReadWriteBuffers + sizeof(ReadData);

    auto DetourData = AssembleDetour();
    uintptr_t DetourBuffer = g_WitnessMemory.AllocateMemory(DetourData.size(), PAGE_EXECUTE_READWRITE);
    g_WitnessMemory.WriteBuffer(DetourData.data(), DetourData.size(), DetourBuffer);

    _NtSuspendProcess NtSuspendProcess = (_NtSuspendProcess) GetProcAddress(GetModuleHandleA("ntdll"), "NtSuspendProcess");
    _NtResumeProcess NtResumeProcess = (_NtResumeProcess) GetProcAddress(GetModuleHandleA("ntdll"), "NtResumeProcess");

    g_WitnessMemory.Suspend();
    g_WitnessMemory.ReprotectMemory(g_DetourParameters.DetourAddress, g_DetourParameters.DetourWholeInstructionBytes, PAGE_READWRITE);

    std::vector<uint8_t> JumpPad(g_DetourParameters.DetourWholeInstructionBytes, 0x90);
    JumpPad[0] = 0xff;
    JumpPad[1] = 0x25;
    JumpPad[2] = 0x00;
    JumpPad[3] = 0x00;
    JumpPad[4] = 0x00;
    JumpPad[5] = 0x00;
    *((uintptr_t*)&JumpPad[6]) = DetourBuffer;

    g_WitnessMemory.WriteBuffer(JumpPad.data(), JumpPad.size(), g_DetourParameters.DetourAddress);
    g_WitnessMemory.ReprotectMemory(g_DetourParameters.DetourAddress, g_DetourParameters.DetourWholeInstructionBytes, PAGE_EXECUTE_READ);
    g_WitnessMemory.Flush(g_DetourParameters.DetourAddress, g_DetourParameters.DetourWholeInstructionBytes);
    g_WitnessMemory.Resume();

    printf("%zX\n", ReadWriteBuffers);

    return true;
}
