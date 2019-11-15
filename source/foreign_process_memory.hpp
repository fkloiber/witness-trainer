#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <type_traits>
#include <stdexcept>
#include <memory>

class AOB;

typedef LONG ( NTAPI *_NtSuspendProcess )( IN HANDLE ProcessHandle );
typedef LONG ( NTAPI *_NtResumeProcess )( IN HANDLE ProcessHandle );

class ForeignProcessMemory {
    struct Token {};

public:
    ForeignProcessMemory(Token, HANDLE, HMODULE, uint8_t * Buffer, _NtSuspendProcess, _NtResumeProcess);
    ~ForeignProcessMemory();

    static std::unique_ptr<ForeignProcessMemory> NewFromProcessName(const wchar_t * const);

    uintptr_t FindFirstOccurrenceInMainModule(const AOB&) const;

    uintptr_t AllocateMemory(size_t Size, DWORD Flags) const;
    void DeallocateMemory(uintptr_t Addr) const;

    bool ReadBuffer(uint8_t * const Buffer, size_t Size, uintptr_t Addr) const;
    bool WriteBuffer(uint8_t * Buffer, size_t Size, uintptr_t Addr) const;
    template<typename T>
    T ReadValue(uintptr_t Address) {
        static_assert(std::is_standard_layout_v<T> && std::is_trivial_v<T> && !std::is_array_v<T>, "");
        T Result;
        for (int i = 0; i < 5; ++i) {
            if (ReadProcessMemory(ProcessHandle, (void*)Address, &Result, sizeof(Result), nullptr)) {
                return Result;
            }
        }
        throw std::runtime_error("Could not read value");
    }
    bool ReprotectMemory(uintptr_t Addr, size_t Size, DWORD Flags) const;

    void Suspend() const {
        if (!NtSuspendProcess) {
            return;
        }
        NtSuspendProcess(ProcessHandle);
    }

    void Resume() const {
        if (!NtResumeProcess) {
            return;
        }
        NtResumeProcess(ProcessHandle);
    }

    void Flush(uintptr_t Addr, size_t Size) const {
        if (!ProcessHandle) {
            return;
        }
        FlushInstructionCache(ProcessHandle, (void*)Addr, Size);
    }

    bool ProcessRunning() const {
        return WaitForSingleObject(ProcessHandle, 0) == WAIT_TIMEOUT;
    }

    DWORD ProcessId() const {
        return GetProcessId(ProcessHandle);
    }

private:
    HANDLE ProcessHandle;
    HMODULE MainModule;
    uint8_t * Buffer;
    _NtSuspendProcess NtSuspendProcess;
    _NtResumeProcess NtResumeProcess;
};
