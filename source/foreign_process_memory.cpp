#include "foreign_process_memory.hpp"

#include <Psapi.h>

#include <limits>

#undef max
#define PAGE_SIZE 4096

ForeignProcessMemory::ForeignProcessMemory() :
    ProcessHandle(nullptr),
    MainModule(nullptr),
    Buffer((uint8_t *)VirtualAlloc(nullptr, 2*PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))
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
