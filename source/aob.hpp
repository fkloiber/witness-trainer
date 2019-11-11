#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

class AOB {
public:
    AOB(std::string_view Pattern);
    size_t Size() const;
    uintptr_t FindIn(uint8_t * Data, size_t Length, uintptr_t DEBUG) const;
private:
    struct PatternByte {
        bool ScanByte;
        uint8_t Value;
    };
    std::vector<PatternByte> Data;

    static PatternByte Byte(uint8_t Value) { return { .ScanByte = true, .Value = Value }; }
    static PatternByte Wildcard() { return { .ScanByte = false }; }
};
