#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Psapi.h>
#include <hidusage.h>
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
HFONT g_UiMonoFont = nullptr;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void SetupConnectTimer(HWND);
void KillConnectTimer(HWND);
bool ConnectToGameProcess();

struct TeleportSlot {
    float X, Y, Z, Theta, Phi;
    bool Present = false;
};

struct UiState {
    Label XLbl, YLbl, ZLbl, ThetaLbl, PhiLbl;
    TeleportSlot Slots[10];
} g_UiState;

extern "C" const char SegoeUiMono[];
extern "C" const size_t SegoeUiMono_len;

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

    AddFontMemResourceEx((void*)SegoeUiMono, (DWORD)SegoeUiMono_len, 0, nullptr);

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
    g_UiMonoFont = CreateFontA(
        -16, 0, 0, 0,
        FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_DONTCARE, "SegoeUIMonoW01-Regular"
    );

    Label::New(10, 10, 60, 20, "Position: ");

    g_UiState.XLbl = Label::New(75, 10, 200, 20, "X: ??", &g_UiMonoFont);
    g_UiState.YLbl = Label::New(75, 30, 200, 20, "Y: ??", &g_UiMonoFont);
    g_UiState.ZLbl = Label::New(75, 50, 200, 20, "Z: ??", &g_UiMonoFont);
    g_UiState.ThetaLbl = Label::New(75, 70, 200, 20, "Theta: ??", &g_UiMonoFont);
    g_UiState.PhiLbl = Label::New(75, 90, 200, 20, "Phi: ??", &g_UiMonoFont);

    RAWINPUTDEVICE RawInputDevice[2];

    RawInputDevice[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    RawInputDevice[0].usUsage = HID_USAGE_GENERIC_KEYBOARD;
    RawInputDevice[0].dwFlags = RIDEV_INPUTSINK;
    RawInputDevice[0].hwndTarget = g_Window;

    RawInputDevice[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
    RawInputDevice[1].usUsage = HID_USAGE_GENERIC_KEYPAD;
    RawInputDevice[1].dwFlags = RIDEV_INPUTSINK;
    RawInputDevice[1].hwndTarget = g_Window;

    auto Result = RegisterRawInputDevices(RawInputDevice, 2, sizeof(RAWINPUTDEVICE));
    if (Result != TRUE) {
        auto Error = GetLastError();
        fmt::print("{} (0x{:x})\n", Error, Error);
    }

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
                    SetWindowTextA(g_Window, "Witness Trainer");
                    SetupConnectTimer(Window);
                    break;
                }
                g_GameInterface->DoRead();
                g_UiState.XLbl.SetText("X: {: = 6.1f}", g_GameInterface->X());
                g_UiState.YLbl.SetText("Y: {: = 6.1f}", g_GameInterface->Y());
                g_UiState.ZLbl.SetText("Z: {: = 6.1f}", g_GameInterface->Z());
                g_UiState.ThetaLbl.SetText("Theta: {:.1f}", g_GameInterface->Theta() * 57.2957795131f);
                g_UiState.PhiLbl.SetText("Phi: {:.1f}", g_GameInterface->Phi() * 57.2957795131f);
            }
        } break;
        case WM_INPUT: {
            static std::vector<uint8_t> RawInputData;
            enum {
                M_SHIFT = 0x01,
                M_CTRL = 0x02,
                M_ALT = 0x04,
                M_WINL = 0x08,
                M_WINR = 0x10,
            };
            static int ModifierState = 0;
            UINT Size;

            GetRawInputData((HRAWINPUT)LParam, RID_INPUT, nullptr, &Size, sizeof(RAWINPUTHEADER));
            if (RawInputData.capacity() < Size) {
                RawInputData.reserve(Size);
            }
            RawInputData.resize(Size);

            GetRawInputData((HRAWINPUT)LParam, RID_INPUT, RawInputData.data(), &Size, sizeof(RAWINPUTHEADER));

            auto Input = (RAWINPUT*) RawInputData.data();
            assert(Input->header.dwType == RIM_TYPEKEYBOARD);

            int bit = -1;
            switch (Input->data.keyboard.VKey) {
                case VK_SHIFT:
                case VK_CONTROL:
                case VK_MENU: {
                    bit = 1 << (Input->data.keyboard.VKey - VK_SHIFT);
                } break;
                case VK_LWIN:
                case VK_RWIN: {
                    bit = 1 << (Input->data.keyboard.VKey - VK_LWIN + 3);
                } break;
            }

            if (bit != -1) {
                if ((Input->data.keyboard.Flags & RI_KEY_BREAK) == 0) {
                    ModifierState |= bit;
                } else {
                    ModifierState &= ~bit;
                }
                break;
            }

            bool HasFocus = g_GameInterface->HasFocus();
            bool WasPressed = (Input->data.keyboard.Flags & RI_KEY_BREAK) == 0;

            if (!HasFocus || !WasPressed) {
                break;
            }

            switch (Input->data.keyboard.VKey) {
                case VK_F1:
                case VK_F2:
                case VK_F3:
                case VK_F4:
                case VK_F5:
                case VK_F6:
                case VK_F7:
                case VK_F8:
                case VK_F9: {
                    int SlotId = Input->data.keyboard.VKey - VK_F1;

                    if (ModifierState == M_SHIFT) {
                        if (SlotId == 8) {
                            return 0;
                        }
                        g_GameInterface->DoRead();
                        g_UiState.Slots[SlotId].X = g_GameInterface->X();
                        g_UiState.Slots[SlotId].Y = g_GameInterface->Y();
                        g_UiState.Slots[SlotId].Z = g_GameInterface->Z();
                        g_UiState.Slots[SlotId].Theta = g_GameInterface->Theta();
                        g_UiState.Slots[SlotId].Phi = g_GameInterface->Phi();
                        g_UiState.Slots[SlotId].Present = true;

                    } else if (ModifierState == 0) {
                        if (!g_UiState.Slots[SlotId].Present) {
                            break;
                        }
                        TeleportSlot Backup = {};

                        g_GameInterface->DoRead();
                        Backup.X = g_GameInterface->X();
                        Backup.Y = g_GameInterface->Y();
                        Backup.Z = g_GameInterface->Z();
                        Backup.Theta = g_GameInterface->Theta();
                        Backup.Phi = g_GameInterface->Phi();
                        Backup.Present = true;

                        g_GameInterface->WriteX(g_UiState.Slots[SlotId].X);
                        g_GameInterface->WriteY(g_UiState.Slots[SlotId].Y);
                        g_GameInterface->WriteZ(g_UiState.Slots[SlotId].Z);
                        g_GameInterface->WriteTheta(g_UiState.Slots[SlotId].Theta);
                        g_GameInterface->WritePhi(g_UiState.Slots[SlotId].Phi);
                        g_GameInterface->DoWrite();

                        g_UiState.Slots[8] = Backup;
                    }
                } break;
            }
            return 0;
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
