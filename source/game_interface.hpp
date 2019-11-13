#pragma once

#include <memory>

#include "foreign_process_memory.hpp"

class GameInterface {
    struct Token {};

public:
    GameInterface(Token, std::unique_ptr<ForeignProcessMemory> &&, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    ~GameInterface();

    static std::unique_ptr<GameInterface> New();

private:
    std::unique_ptr<ForeignProcessMemory> Memory;
    uintptr_t DetourAddress;
    uintptr_t DetourBuffer;
    uintptr_t ReadAddress;
    uintptr_t WriteAddress;
};
