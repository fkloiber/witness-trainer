#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

class AOB {
public:
    AOB(std::string_view Pattern);
private:
    struct PatternByte {
        bool ScanByte;
        uint8_t Value;
    };
    std::vector<PatternByte> Data;

    static PatternByte Byte(uint8_t Value) { return { .ScanByte = true, .Value = Value }; }
    static PatternByte Wildcard() { return { .ScanByte = false }; }
};
