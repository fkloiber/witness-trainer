#include "game_interface.hpp"
#include "foreign_process_memory.hpp"
#include "aob.hpp"
#include "defer.hpp"

#include <stdexcept>

GameInterface::GameInterface(
    GameInterface::Token,
    std::unique_ptr<ForeignProcessMemory> && Memory,
    uintptr_t DetourAddress,
    uintptr_t DetourBuffer,
    uintptr_t ReadAddress,
    uintptr_t WriteAddress
) :
    Memory(std::move(Memory)),
    DetourAddress(DetourAddress),
    DetourBuffer(DetourBuffer),
    ReadAddress(ReadAddress),
    WriteAddress(WriteAddress)
{ }

const AOB DetourAOB("66 0F 6E C1 F3 0F E6 D2 F3 0F E6 C0 F2 41 0F 59 D0");
const AOB GetThePlayerCallAOB("E8 ?? ?? ?? ?? 48 85 C0 74 ?? F2 0F 10 40");
const AOB GetCameraParametersFunctionAOB("48 85 C9 74 0C F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 01 48 85 D2 74 0C F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 02 C3");

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
const static uint8_t DetourFragmentTemplate[] = { // 0xcc bytes are uninitialized and must be filled in
    /* 000: */ 0x50,                                    // push   rax
    /* 001: */ 0x53,                                    // push   rbx
    /* 002: */ 0x51,                                    // push   rcx
    /* 003: */ 0x52,                                    // push   rdx
    /* 004: */ 0x57,                                    // push   rdi
    /* 005: */ 0x48,0x8b,0x3d,0x9b,0x00,0x00,0x00,      // mov    rdi,QWORD PTR [rip+0x9b]        # a7 <GetPlayer>
    /* 00c: */ 0xff,0xd7,                               // call   rdi
    /* 00e: */ 0x48,0x8b,0x3d,0xaa,0x00,0x00,0x00,      // mov    rdi,QWORD PTR [rip+0xaa]        # bf <Read>
    /* 015: */ 0x8b,0x58,0xcc,                          // mov    ebx,DWORD PTR [rax+0x??]
    /* 018: */ 0x8b,0x48,0xcc,                          // mov    ecx,DWORD PTR [rax+0x??]
    /* 01b: */ 0x8b,0x50,0xcc,                          // mov    edx,DWORD PTR [rax+0x??]
    /* 01e: */ 0x89,0x5f,0xcc,                          // mov    DWORD PTR [rdi+0x??],ebx
    /* 021: */ 0x89,0x4f,0xcc,                          // mov    DWORD PTR [rdi+0x??],ecx
    /* 024: */ 0x89,0x57,0xcc,                          // mov    DWORD PTR [rdi+0x??],edx
    /* 027: */ 0x48,0x8b,0x1d,0x81,0x00,0x00,0x00,      // mov    rbx,QWORD PTR [rip+0x81]        # af <Theta>
    /* 02e: */ 0x48,0x8b,0x0d,0x82,0x00,0x00,0x00,      // mov    rcx,QWORD PTR [rip+0x82]        # b7 <Phi>
    /* 035: */ 0x8b,0x1b,                               // mov    ebx,DWORD PTR [rbx]
    /* 037: */ 0x8b,0x09,                               // mov    ecx,DWORD PTR [rcx]
    /* 039: */ 0x89,0x5f,0xcc,                          // mov    DWORD PTR [rdi+0x??],ebx
    /* 03c: */ 0x89,0x4f,0xcc,                          // mov    DWORD PTR [rdi+0x??],ecx
    /* 03f: */ 0x48,0x8b,0x3d,0x81,0x00,0x00,0x00,      // mov    rdi,QWORD PTR [rip+0x81]        # c7 <Write>    /* 046: */ 0x80,0x7f,0x01,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
    /* 04a: */ 0x74,0x48,                               // je     94 <Exit>
    /* 04c: */ 0x80,0x7f,0xcc,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
    /* 050: */ 0x74,0x06,                               // je     58 <SkipX>
    /* 052: */ 0x8b,0x5f,0xcc,                          // mov    ebx,DWORD PTR [rdi+0x??]
    /* 055: */ 0x89,0x58,0xcc,                          // mov    DWORD PTR [rax+0x??],ebx
    // 0000000000000058 <SkipX>:
    /* 058: */ 0x80,0x7f,0xcc,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
    /* 05c: */ 0x74,0x06,                               // je     64 <SkipY>
    /* 05e: */ 0x8b,0x5f,0xcc,                          // mov    ebx,DWORD PTR [rdi+0x??]
    /* 061: */ 0x89,0x58,0xcc,                          // mov    DWORD PTR [rax+0x??],ebx
    // 0000000000000064 <SkipY>:
    /* 064: */ 0x80,0x7f,0xcc,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
    /* 068: */ 0x74,0x06,                               // je     70 <SkipZ>
    /* 06a: */ 0x8b,0x5f,0xcc,                          // mov    ebx,DWORD PTR [rdi+0x??]
    /* 06d: */ 0x89,0x58,0xcc,                          // mov    DWORD PTR [rax+0x??],ebx
    // 0000000000000070 <SkipZ>:
    /* 070: */ 0x80,0x7f,0xcc,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
    /* 074: */ 0x74,0x0c,                               // je     82 <SkipTheta>
    /* 076: */ 0x48,0x8b,0x1d,0x32,0x00,0x00,0x00,      // mov    rbx,QWORD PTR [rip+0x32]        # af <Theta>
    /* 07d: */ 0x8b,0x5f,0xcc,                          // mov    ebx,DWORD PTR [rdi+0x??]
    /* 080: */ 0x89,0x1b,                               // mov    DWORD PTR [rbx],ebx
    // 0000000000000082 <SkipTheta>:
    /* 082: */ 0x80,0x7f,0xcc,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
    /* 086: */ 0x74,0x0c,                               // je     94 <Exit>
    /* 088: */ 0x48,0x8b,0x0d,0x28,0x00,0x00,0x00,      // mov    rcx,QWORD PTR [rip+0x28]        # b7 <Phi>
    /* 08f: */ 0x8b,0x5f,0xcc,                          // mov    ebx,DWORD PTR [rdi+0x??]
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

std::unique_ptr<GameInterface> GameInterface::New() {
    auto Memory = ForeignProcessMemory::NewFromProcessName(L"witness64_d3d11.exe");
    if (!Memory) {
        return nullptr;
    }

    // Find assembly parameters
    auto DetourAddress = Memory->FindFirstOccurrenceInMainModule(DetourAOB);
    auto GetPlayerCallAddress = Memory->FindFirstOccurrenceInMainModule(GetThePlayerCallAOB);
    auto GetCameraParametersAddress = Memory->FindFirstOccurrenceInMainModule(GetCameraParametersFunctionAOB);
    if (!DetourAddress || !GetPlayerCallAddress || !GetCameraParametersAddress) {
        return nullptr;
    }

    auto RelativeFunctionAddress = Memory->ReadValue<int32_t>(GetPlayerCallAddress + 1);
    auto GetPlayerAddress = GetPlayerCallAddress + 5 + RelativeFunctionAddress;
    auto XPositionOffset = Memory->ReadValue<int8_t>(GetPlayerCallAddress + 0x0E);

    auto RelativeAddressTheta = Memory->ReadValue<int32_t>(GetCameraParametersAddress + 0x09);
    auto RelativeAddressPhi = Memory->ReadValue<int32_t>(GetCameraParametersAddress + 0x1A);
    auto ThetaAddress = GetCameraParametersAddress + 0x0D + RelativeAddressTheta;
    auto PhiAddress = GetCameraParametersAddress + 0x1E + RelativeAddressPhi;

    auto ReadWriteBuffers = Memory->AllocateMemory(sizeof(ReadData) + sizeof(WriteData), PAGE_READWRITE);
    auto ReadWriteBuffersDeallocator = Defer([&](){ Memory->DeallocateMemory(ReadWriteBuffers, sizeof(ReadData) + sizeof(WriteData)); });
    auto ReadAddress = ReadWriteBuffers;
    auto WriteAddress = ReadWriteBuffers + sizeof(ReadData);

    if (!ReadWriteBuffers) {
        return nullptr;
    }

    // Assemble the final detour fragment
    std::vector<uint8_t> DetourFragment(sizeof(DetourFragmentTemplate) + DetourAOB.Size(), 0x90);
    if (!Memory->ReadBuffer(DetourFragment.data(), DetourAOB.Size(), DetourAddress)) {
        return nullptr;
    }

    memcpy(DetourFragment.data() + DetourAOB.Size(), DetourFragmentTemplate, sizeof(DetourFragmentTemplate));
    uint8_t * Data = DetourFragment.data() + DetourAOB.Size();

    // Fixup addresses and offsets
  #define OFFSETOF(s, m) ((uint8_t)(uintptr_t)&(((s*)0)->m))
    Data[0x17] = XPositionOffset + 0;
    Data[0x1a] = XPositionOffset + 4;
    Data[0x1d] = XPositionOffset + 8;
    Data[0x20] = OFFSETOF(ReadData, X);
    Data[0x23] = OFFSETOF(ReadData, Y);
    Data[0x26] = OFFSETOF(ReadData, Z);
    Data[0x3b] = OFFSETOF(ReadData, Theta);
    Data[0x3e] = OFFSETOF(ReadData, Phi);
    Data[0x48] = OFFSETOF(WriteData, WriteAny);
    Data[0x4e] = OFFSETOF(WriteData, WriteX);
    Data[0x54] = OFFSETOF(WriteData, X);
    Data[0x57] = XPositionOffset + 0;
    Data[0x5a] = OFFSETOF(WriteData, WriteY);
    Data[0x60] = OFFSETOF(WriteData, Y);
    Data[0x63] = XPositionOffset + 4;
    Data[0x66] = OFFSETOF(WriteData, WriteZ);
    Data[0x6c] = OFFSETOF(WriteData, Z);
    Data[0x6f] = XPositionOffset + 8;
    Data[0x72] = OFFSETOF(WriteData, WriteTheta);
    Data[0x7f] = OFFSETOF(WriteData, Theta);
    Data[0x84] = OFFSETOF(WriteData, WritePhi);
    Data[0x91] = OFFSETOF(WriteData, Phi);
    *((uintptr_t*)&Data[0x9f]) = DetourAddress + DetourAOB.Size();
    *((uintptr_t*)&Data[0xa7]) = GetPlayerAddress;
    *((uintptr_t*)&Data[0xaf]) = ThetaAddress;
    *((uintptr_t*)&Data[0xb7]) = PhiAddress;
    *((uintptr_t*)&Data[0xbf]) = ReadAddress;
    *((uintptr_t*)&Data[0xc7]) = WriteAddress;
  #undef OFFSETOF

    // Copy the detour code over to the game process
    auto DetourBuffer = Memory->AllocateMemory(DetourFragment.size(), PAGE_EXECUTE_READWRITE);
    if (!DetourBuffer) {
        return nullptr;
    }
    auto DetourBufferDeallocator = Defer([&](){ Memory->DeallocateMemory(DetourBuffer, DetourFragment.size()); });
    if (!Memory->WriteBuffer(DetourFragment.data(), DetourFragment.size(), DetourBuffer)) {
        return nullptr;
    }

    // Assemble the detour jump pad
    std::vector<uint8_t> JumpPad(DetourAOB.Size(), 0x90);
    // 00: jmp [rip + 0]
    JumpPad[0] = 0xff;
    JumpPad[1] = 0x25;
    JumpPad[2] = 0x00;
    JumpPad[3] = 0x00;
    JumpPad[4] = 0x00;
    JumpPad[5] = 0x00;
    // 05: .quad <DetourBuffer>
    *((uintptr_t*)&JumpPad[6]) = DetourBuffer;

    // Patch the detour site to jump to our code
    Memory->Suspend();
    auto Resumer = Defer([&](){ Memory->Resume(); });
    if (!Memory->ReprotectMemory(DetourAddress, DetourAOB.Size(), PAGE_READWRITE)) {
        return nullptr;
    }
    auto Reprotector = Defer([&](){ Memory->ReprotectMemory(DetourAddress, DetourAOB.Size(), PAGE_EXECUTE_READ); });

    if (!Memory->WriteBuffer(JumpPad.data(), JumpPad.size(), DetourAddress)) {
        return nullptr;
    }

    Reprotector.Do();
    Memory->Flush(DetourAddress, DetourAOB.Size());
    Resumer.Do();

    DetourBufferDeallocator.Kill();
    ReadWriteBuffersDeallocator.Kill();

    return std::make_unique<GameInterface>(Token {}, std::move(Memory), DetourAddress, DetourBuffer, ReadAddress, WriteAddress);
}

GameInterface::~GameInterface() {
    // Unpatch the detour site
    std::vector<uint8_t> Buffer(DetourAOB.Size(), 0x90);
    Memory->ReadBuffer(Buffer.data(), Buffer.size(), DetourAddress);

    Memory->Suspend();
    Memory->ReprotectMemory(DetourAddress, DetourAOB.Size(), PAGE_READWRITE);
    Memory->WriteBuffer(Buffer.data(), Buffer.size(), DetourAddress);
    Memory->ReprotectMemory(DetourAddress, DetourAOB.Size(), PAGE_EXECUTE_READ);
    Memory->Flush(DetourAddress, DetourAOB.Size());
    Memory->Resume();

    // Deallocate our buffers
    Memory->DeallocateMemory(DetourBuffer, sizeof(DetourFragmentTemplate) + DetourAOB.Size());
    Memory->DeallocateMemory(ReadAddress, sizeof(ReadData) + sizeof(WriteData));
}
