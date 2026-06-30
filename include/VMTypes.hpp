#pragma once
#include <expected>

enum class VMError : uint8_t {
    InvalidRegister,
    InvalidOperand,
    UnknownLabel,
    UnsupportedOpcode,
    StackUnderflow
};


template <typename T>
using VMResult = std::expected<T, VMError>;