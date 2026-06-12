#pragma once
#include <concepts>
#include <type_traits>
#include "Instruction.hpp"

template <typename T>
concept IsRegister = std::is_trivially_copyable_v<T> && requires (T reg){
    requires std::integral<decltype(reg.index)>;
    requires sizeof(T) <= 2; 
};

template <typename T>
concept IsInstruction = std::is_trivially_copyable_v<T> && requires(T ins){
    requires std::is_enum_v<decltype(ins.opcode)>;
    requires std::integral<decltype(ins.operand_count)>;
    
    { ins.operands[0] }; 

    requires sizeof(T) <= 64;
};