#include "aob.hpp"

#include <stdexcept>

static bool IsHexadecimalDigit(char Char) {
    return ((Char >= '0' && Char <= '9') ||
            (Char >= 'a' && Char <= 'f') ||
            (Char >= 'A' && Char <= 'F'));
}

static uint8_t HexadecimalValue(char Char) {
    if (Char >= '0' && Char <= '9') {
        return Char - '0';
    } else if (Char >= 'a' && Char <= 'f') {
        return Char - 'a' + 10;
    } else if (Char >= 'A' && Char <= 'F') {
        return Char - 'A' + 10;
    }
}

AOB::AOB(std::string_view Pattern) {
    size_t Length = 0;
    bool HalfByte = false;
    for (size_t i = 0; i < Pattern.size(); ++i) {
        char Char = Pattern[i];
        if (IsHexadecimalDigit(Char) || Char == '?') {
            HalfByte = !HalfByte;
            if (!HalfByte) {
                Length++;
            }
        } else if (Char != ' ') {
            throw std::invalid_argument("Invalid character in AOB pattern");
        }
    }
    if (HalfByte) {
        throw std::invalid_argument("Non whole number of bytes in AOB pattern");
    }

    enum class ParseState {
        BeginByte,
        KnownByte,
        UnknownByte,
    } State = ParseState::BeginByte;
    char LastChar;
    Data.reserve(Length);
    for (size_t i = 0; i < Pattern.size(); ++i) {
        char Char = Pattern[i];
        if (IsHexadecimalDigit(Char)) {
            if (State == ParseState::BeginByte) {
                State = ParseState::KnownByte;
                LastChar = Char;
            } else if (State == ParseState::KnownByte) {
                State = ParseState::BeginByte;
                uint8_t Value = (HexadecimalValue(LastChar) << 4) + HexadecimalValue(Char);
                Data.push_back(AOB::Byte(Value));
            } else {
                throw std::invalid_argument("Mixed known/unknown nibbles in AOB pattern");
            }
        } else if (Char == '?') {
            if (State == ParseState::BeginByte) {
                State = ParseState::UnknownByte;
            } else if (State == ParseState::UnknownByte) {
                State = ParseState::BeginByte;
                Data.push_back(AOB::Wildcard());
            } else {
                throw std::invalid_argument("Mixed known/unknown nibbles in AOB pattern");
            }
        }
    }
}
