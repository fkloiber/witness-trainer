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
    WriteAddress(WriteAddress),
    ReadBuffer({}),
    WriteBuffer({})
{ }

bool GameInterface::DoRead() {
    return Memory->ReadBuffer((uint8_t*)&ReadBuffer, sizeof(ReadBuffer), ReadAddress);
}

bool GameInterface::DoWrite() {
    WriteBuffer.WriteAny = true;
    return Memory->WriteBuffer((uint8_t*)&WriteBuffer, sizeof(WriteBuffer), WriteAddress);
}

const AOB DetourAOB("66 0F 6E C1 F3 0F E6 D2 F3 0F E6 C0 F2 41 0F 59 D0");
const AOB GetThePlayerCallAOB("E8 ?? ?? ?? ?? 48 85 C0 74 ?? F2 0F 10 40");
const AOB GetCameraParametersFunctionAOB("48 85 C9 74 0C F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 01 48 85 D2 74 0C F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 02 C3");

const static uint8_t DetourFragmentTemplate[] = { // 0xcc bytes are uninitialized and must be filled in
    /* 000: */ 0x50,                                    // push   rax
    /* 001: */ 0x53,                                    // push   rbx
    /* 002: */ 0x51,                                    // push   rcx
    /* 003: */ 0x52,                                    // push   rdx
    /* 004: */ 0x57,                                    // push   rdi
    /* 005: */ 0x48,0x8b,0x3d,0xb0,0x00,0x00,0x00,      // mov    rdi,QWORD PTR [rip+0xb0]        # bc <GetPlayer>
    /* 00c: */ 0xff,0xd7,                               // call   rdi
    /* 00e: */ 0x48,0x8b,0x3d,0xbf,0x00,0x00,0x00,      // mov    rdi,QWORD PTR [rip+0xbf]        # d4 <Read>
    /* 015: */ 0x8b,0x58,0xcc,                          // mov    ebx,DWORD PTR [rax+0x??]
    /* 018: */ 0x8b,0x48,0xcc,                          // mov    ecx,DWORD PTR [rax+0x??]
    /* 01b: */ 0x8b,0x50,0xcc,                          // mov    edx,DWORD PTR [rax+0x??]
    /* 01e: */ 0x89,0x5f,0xcc,                          // mov    DWORD PTR [rdi+0x??],ebx
    /* 021: */ 0x89,0x4f,0xcc,                          // mov    DWORD PTR [rdi+0x??],ecx
    /* 024: */ 0x89,0x57,0xcc,                          // mov    DWORD PTR [rdi+0x??],edx
    /* 027: */ 0x48,0x8b,0x1d,0x96,0x00,0x00,0x00,      // mov    rbx,QWORD PTR [rip+0x96]        # c4 <Theta>
    /* 02e: */ 0x48,0x8b,0x0d,0x97,0x00,0x00,0x00,      // mov    rcx,QWORD PTR [rip+0x97]        # cc <Phi>
    /* 035: */ 0x8b,0x1b,                               // mov    ebx,DWORD PTR [rbx]
    /* 037: */ 0x8b,0x09,                               // mov    ecx,DWORD PTR [rcx]
    /* 039: */ 0x89,0x5f,0xcc,                          // mov    DWORD PTR [rdi+0x??],ebx
    /* 03c: */ 0x89,0x4f,0xcc,                          // mov    DWORD PTR [rdi+0x??],ecx
    /* 03f: */ 0x48,0x8b,0x3d,0x96,0x00,0x00,0x00,      // mov    rdi,QWORD PTR [rip+0x96]        # dc <Write>
    /* 046: */ 0x48,0x31,0xc0,                          // xor    rax,rax
    /* 049: */ 0x80,0x7f,0xcc,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
    /* 04d: */ 0x88,0x47,0xcc,                          // mov    BYTE PTR [rdi+0x??],al
    /* 050: */ 0x74,0x57,                               // je     a9 <Exit>
    /* 052: */ 0x80,0x7f,0xcc,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
    /* 056: */ 0x88,0x47,0xcc,                          // mov    BYTE PTR [rdi+0x??],al
    /* 059: */ 0x74,0x06,                               // je     61 <SkipX>
    /* 05b: */ 0x8b,0x5f,0xcc,                          // mov    ebx,DWORD PTR [rdi+0x??]
    /* 05e: */ 0x89,0x58,0xcc,                          // mov    DWORD PTR [rax+0x??],ebx
    // 0000000000000061 <SkipX>:
    /* 061: */ 0x80,0x7f,0xcc,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
    /* 065: */ 0x88,0x47,0xcc,                          // mov    BYTE PTR [rdi+0x??],al
    /* 068: */ 0x74,0x06,                               // je     70 <SkipY>
    /* 06a: */ 0x8b,0x5f,0xcc,                          // mov    ebx,DWORD PTR [rdi+0x??]
    /* 06d: */ 0x89,0x58,0xcc,                          // mov    DWORD PTR [rax+0x??],ebx
    // 0000000000000070 <SkipY>:
    /* 070: */ 0x80,0x7f,0xcc,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
    /* 074: */ 0x88,0x47,0xcc,                          // mov    BYTE PTR [rdi+0x??],al
    /* 077: */ 0x74,0x06,                               // je     7f <SkipZ>
    /* 079: */ 0x8b,0x5f,0xcc,                          // mov    ebx,DWORD PTR [rdi+0x??]
    /* 07c: */ 0x89,0x58,0xcc,                          // mov    DWORD PTR [rax+0x??],ebx
    // 000000000000007f <SkipZ>:
    /* 07f: */ 0x80,0x7f,0xcc,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
    /* 083: */ 0x88,0x47,0xcc,                          // mov    BYTE PTR [rdi+0x??],al
    /* 086: */ 0x74,0x0c,                               // je     94 <SkipTheta>
    /* 088: */ 0x48,0x8b,0x1d,0x35,0x00,0x00,0x00,      // mov    rbx,QWORD PTR [rip+0x35]        # c4 <Theta>
    /* 08f: */ 0x8b,0x5f,0xcc,                          // mov    ebx,DWORD PTR [rdi+0x??]
    /* 092: */ 0x89,0x1b,                               // mov    DWORD PTR [rbx],ebx
    // 0000000000000094 <SkipTheta>:
    /* 094: */ 0x80,0x7f,0xcc,0x00,                     // cmp    BYTE PTR [rdi+0x??],0x0
    /* 098: */ 0x88,0x47,0xcc,                          // mov    BYTE PTR [rdi+0x??],al
    /* 09b: */ 0x74,0x0c,                               // je     a9 <Exit>
    /* 09d: */ 0x48,0x8b,0x0d,0x28,0x00,0x00,0x00,      // mov    rcx,QWORD PTR [rip+0x28]        # cc <Phi>
    /* 0a4: */ 0x8b,0x5f,0xcc,                          // mov    ebx,DWORD PTR [rdi+0x??]
    /* 0a7: */ 0x89,0x19,                               // mov    DWORD PTR [rcx],ebx
    // 00000000000000a9 <Exit>:
    /* 0a9: */ 0x5f,                                    // pop    rdi
    /* 0aa: */ 0x5a,                                    // pop    rdx
    /* 0ab: */ 0x59,                                    // pop    rcx
    /* 0ac: */ 0x5b,                                    // pop    rbx
    /* 0ad: */ 0x58,                                    // pop    rax
    /* 0ae: */ 0xff,0x25,0x00,0x00,0x00,0x00,           // jmp    QWORD PTR [rip+0x0]        # b4 <Exit+0xb>
    /* 0b4: */ 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, // .quad 0xcccccccccccccccc
    // 00000000000000bc <GetPlayer>:
    /* 0bc: */ 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, // .quad 0xcccccccccccccccc
    // 00000000000000c4 <Theta>:
    /* 0c4: */ 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, // .quad 0xcccccccccccccccc
    // 00000000000000cc <Phi>:
    /* 0cc: */ 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, // .quad 0xcccccccccccccccc
    // 00000000000000d4 <Read>:
    /* 0d4: */ 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, // .quad 0xcccccccccccccccc
    // 00000000000000dc <Write>:
    /* 0dc: */ 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, // .quad 0xcccccccccccccccc
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
    Data[0x4b] = OFFSETOF(WriteData, WriteAny);
    Data[0x4f] = OFFSETOF(WriteData, WriteAny);
    Data[0x52] = OFFSETOF(WriteData, WriteX);
    Data[0x58] = OFFSETOF(WriteData, WriteX);
    Data[0x5d] = OFFSETOF(WriteData, X);
    Data[0x60] = XPositionOffset + 0;
    Data[0x63] = OFFSETOF(WriteData, WriteY);
    Data[0x67] = OFFSETOF(WriteData, WriteY);
    Data[0x6c] = OFFSETOF(WriteData, Y);
    Data[0x6f] = XPositionOffset + 4;
    Data[0x72] = OFFSETOF(WriteData, WriteZ);
    Data[0x76] = OFFSETOF(WriteData, WriteZ);
    Data[0x7b] = OFFSETOF(WriteData, Z);
    Data[0x7e] = XPositionOffset + 8;
    Data[0x81] = OFFSETOF(WriteData, WriteTheta);
    Data[0x85] = OFFSETOF(WriteData, WriteTheta);
    Data[0x91] = OFFSETOF(WriteData, Theta);
    Data[0x96] = OFFSETOF(WriteData, WritePhi);
    Data[0x9a] = OFFSETOF(WriteData, WritePhi);
    Data[0xa6] = OFFSETOF(WriteData, Phi);
    *((uintptr_t*)&Data[0xb4]) = DetourAddress + DetourAOB.Size();
    *((uintptr_t*)&Data[0xbc]) = GetPlayerAddress;
    *((uintptr_t*)&Data[0xc4]) = ThetaAddress;
    *((uintptr_t*)&Data[0xcc]) = PhiAddress;
    *((uintptr_t*)&Data[0xd4]) = ReadAddress;
    *((uintptr_t*)&Data[0xdc]) = WriteAddress;
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
