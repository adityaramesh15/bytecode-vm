#pragma once
#include <expected>

enum class VMError : uint8_t {
    InvalidRegister,
    InvalidOperand,
    UnsupportedOpcode,
    StackUnderflow, 
    UnknownLabel,
    InvalidBytecode
};


template <typename T>
using VMResult = std::expected<T, VMError>;