#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <type_traits>
#include <stdexcept>

#include "aob.hpp"

class ForeignProcessMemory {
public:
    ForeignProcessMemory();
    ~ForeignProcessMemory();
    void Connect(HANDLE ProcessHandle);
    void Disconnect();

    uintptr_t FindFirstOccurrenceInMainModule(const AOB&) const;
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

private:
    HANDLE ProcessHandle;
    HMODULE MainModule;
    uint8_t * Buffer;
};
