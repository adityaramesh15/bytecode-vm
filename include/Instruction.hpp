#pragma once
#include <cstdint>
#include <variant>
#include <string_view>

enum class Opcode : uint8_t {
    MOV,
    ADD,
    SUB,
    JMP,
    PUSH,
    POP,
    CALL,
    RET
};

struct VirtualRegister {
    uint8_t index{0};      // the index of the register, R5 becomes just 5
};

struct Operand {
    std::variant<VirtualRegister, int64_t, std::string_view> value;   
}; 

struct Instruction {
    Opcode opcode;
    Operand operands[2];        // limiting project to take max 2 operands
    uint8_t operand_count{0}; 
};