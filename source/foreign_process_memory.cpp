#include "foreign_process_memory.hpp"

#include <Psapi.h>

#include <limits>

#undef max
#define PAGE_SIZE 4096

ForeignProcessMemory::ForeignProcessMemory() :
    ProcessHandle(nullptr),
    MainModule(nullptr),
    Buffer((uint8_t *)VirtualAlloc(nullptr, 2*PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)),
    NtSuspendProcess((_NtSuspendProcess)GetProcAddress(GetModuleHandleA("ntdll"), "NtSuspendProcess")),
    NtResumeProcess((_NtResumeProcess)GetProcAddress(GetModuleHandleA("ntdll"), "NtResumeProcess"))
{}

ForeignProcessMemory::~ForeignProcessMemory() {
    Disconnect();
    if (Buffer) {
        VirtualFree(Buffer, 2*PAGE_SIZE, MEM_RELEASE);
        Buffer = nullptr;
    }
}

void ForeignProcessMemory::Connect(HANDLE Handle) {
    Disconnect();
    ProcessHandle = Handle;
    EnumProcessModules(ProcessHandle, &MainModule, sizeof(HMODULE), nullptr);
}

void ForeignProcessMemory::Disconnect() {
    if (ProcessHandle) {
        CloseHandle(ProcessHandle);
        ProcessHandle = nullptr;
        MainModule = nullptr;
    }
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
    //ReadProcessMemory(ProcessHandle, MainModule, Buffer, PAGE_SIZE, nullptr);
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
