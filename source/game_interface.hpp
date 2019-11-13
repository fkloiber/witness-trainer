#pragma once

#include <memory>

#include "foreign_process_memory.hpp"

struct ReadData {
    float X, Y, Z, Theta, Phi;
};
struct WriteData {
    float X, Y, Z, Theta, Phi;
    bool WriteAny, WriteX, WriteY, WriteZ, WriteTheta, WritePhi;
};

class GameInterface {
    struct Token {};

public:
    GameInterface(Token, std::unique_ptr<ForeignProcessMemory> &&, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    ~GameInterface();

    static std::unique_ptr<GameInterface> New();

    bool DoRead();
    float X() const     { return ReadBuffer.X; }
    float Y() const     { return ReadBuffer.Y; }
    float Z() const     { return ReadBuffer.Z; }
    float Theta() const { return ReadBuffer.Theta; }
    float Phi() const   { return ReadBuffer.Phi; }

    void WriteX(float Val)     { WriteBuffer.X     = Val; WriteBuffer.WriteX     = true; }
    void WriteY(float Val)     { WriteBuffer.Y     = Val; WriteBuffer.WriteY     = true; }
    void WriteZ(float Val)     { WriteBuffer.Z     = Val; WriteBuffer.WriteZ     = true; }
    void WriteTheta(float Val) { WriteBuffer.Theta = Val; WriteBuffer.WriteTheta = true; }
    void WritePhi(float Val)   { WriteBuffer.Phi   = Val; WriteBuffer.WritePhi   = true; }
    bool DoWrite();

private:
    std::unique_ptr<ForeignProcessMemory> Memory;
    uintptr_t DetourAddress;
    uintptr_t DetourBuffer;
    uintptr_t ReadAddress;
    uintptr_t WriteAddress;
    ReadData ReadBuffer;
    WriteData WriteBuffer;
};
