#include "foreign_process_memory.hpp"

#include <Psapi.h>
#include <TlHelp32.h>

#include <limits>
#include <stdexcept>

#include "aob.hpp"
#include "defer.hpp"

#undef max
#define PAGE_SIZE 4096

HANDLE TryOpenProcess(const wchar_t * const ProcessName) {
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

ForeignProcessMemory::ForeignProcessMemory(
    ForeignProcessMemory::Token,
    HANDLE ProcessHandle,
    HMODULE MainModule,
    uint8_t * Buffer,
    _NtSuspendProcess NtSuspendProcess,
    _NtResumeProcess NtResumeProcess
) :
    ProcessHandle(ProcessHandle),
    MainModule(MainModule),
    Buffer(Buffer),
    NtSuspendProcess(NtSuspendProcess),
    NtResumeProcess(NtResumeProcess)
{ }

ForeignProcessMemory::~ForeignProcessMemory() {
    if (ProcessHandle) {
        CloseHandle(ProcessHandle);
        ProcessHandle = nullptr;
        MainModule = nullptr;
    }
    if (Buffer) {
        VirtualFree(Buffer, 2*PAGE_SIZE, MEM_RELEASE);
        Buffer = nullptr;
    }
    NtSuspendProcess = nullptr;
    NtResumeProcess = nullptr;
}

std::unique_ptr<ForeignProcessMemory> ForeignProcessMemory::NewFromProcessName(const wchar_t * const Name) {
    auto ProcessHandle = TryOpenProcess(Name);
    if (!ProcessHandle) {
        return nullptr;
    }
    auto HandleCloser = Defer([&](){ CloseHandle(ProcessHandle); });

    HMODULE MainModule;
    if (!EnumProcessModules(ProcessHandle, &MainModule, sizeof(MainModule), nullptr)) {
        return nullptr;
    }

    auto Buffer = (uint8_t *) VirtualAlloc(nullptr, 2 * PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!Buffer) {
        return nullptr;
    }
    auto BufferDeallocator = Defer([&](){ VirtualFree(Buffer, 2 * PAGE_SIZE, MEM_RELEASE); });

    static _NtSuspendProcess NtSuspendProcess = nullptr;
    static _NtResumeProcess NtResumeProcess = nullptr;

    if (!NtSuspendProcess) {
        NtSuspendProcess = (_NtSuspendProcess)GetProcAddress(GetModuleHandleA("ntdll"), "NtSuspendProcess");
    }
    if (!NtResumeProcess) {
        NtResumeProcess = (_NtResumeProcess)GetProcAddress(GetModuleHandleA("ntdll"), "NtResumeProcess");
    }

    if (!NtSuspendProcess || !NtResumeProcess) {
        return nullptr;
    }

    BufferDeallocator.Kill();
    HandleCloser.Kill();

    return std::make_unique<ForeignProcessMemory>(Token {}, ProcessHandle, MainModule, Buffer, NtSuspendProcess, NtResumeProcess);
}

uintptr_t ForeignProcessMemory::FindFirstOccurrenceInMainModule(const AOB & Pattern) const {
    MODULEINFO ModuleInfo = {};
    GetModuleInformation(ProcessHandle, MainModule, &ModuleInfo, sizeof(MODULEINFO));
    size_t LastReadOffset = 0;
    size_t BytesRead;
    for (size_t i = 0; i < ModuleInfo.SizeOfImage; i += BytesRead) {
        BOOL Result = ReadProcessMemory(ProcessHandle, (uint8_t*)MainModule + i, Buffer + PAGE_SIZE, PAGE_SIZE, &BytesRead);
        uintptr_t Match = Pattern.FindIn(Buffer + PAGE_SIZE - LastReadOffset, BytesRead + LastReadOffset, i - LastReadOffset);
        if (Match != std::numeric_limits<uintptr_t>::max()) {
            return (uintptr_t)MainModule + i - LastReadOffset + Match;
        }
        LastReadOffset = Pattern.Size();
        memcpy(Buffer + PAGE_SIZE - LastReadOffset, Buffer + PAGE_SIZE + BytesRead - LastReadOffset, LastReadOffset);
    }
    return 0;
}

uintptr_t ForeignProcessMemory::AllocateMemory(size_t Size, DWORD Flags) const {
    if (!ProcessHandle) {
        return 0;
    }
    return (uintptr_t)VirtualAllocEx(ProcessHandle, nullptr, Size, MEM_RESERVE | MEM_COMMIT, Flags);
}

void ForeignProcessMemory::DeallocateMemory(uintptr_t Addr, size_t Size) const {
    if (!ProcessHandle || Addr == 0) {
        return;
    }
    VirtualFreeEx(ProcessHandle, (void*)Addr, Size, MEM_DECOMMIT|MEM_RELEASE);
}

bool ForeignProcessMemory::ReadBuffer(uint8_t * const Buffer, size_t Size, uintptr_t Addr) const {
    if (!ProcessHandle || Addr == 0) {
        return false;
    }
    return ReadProcessMemory(ProcessHandle, (void*)Addr, Buffer, Size, nullptr) == TRUE;
}

bool ForeignProcessMemory::WriteBuffer(uint8_t * const Buffer, size_t Size, uintptr_t Addr) const {
    if (!ProcessHandle || Addr == 0) {
        return false;
    }
    return WriteProcessMemory(ProcessHandle, (void*)Addr, Buffer, Size, nullptr) == TRUE;
}

bool ForeignProcessMemory::ReprotectMemory(uintptr_t Addr, size_t Size, DWORD Flags) const {
    if (!ProcessHandle || Addr == 0) {
        return false;
    }
    return VirtualProtectEx(ProcessHandle, (void*)Addr, Size, Flags, nullptr) == TRUE;
}
