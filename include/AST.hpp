#pragma once
#include "Instruction.hpp"
#include <string_view>
#include <expected>
#include <variant>

namespace AST {
    struct MovOp {
        VirtualRegister dest;
        Operand src;
    };

    struct AddOp {
        VirtualRegister dest;
        Operand src;
    };
    
    struct SubOp {
        VirtualRegister dest;
        Operand src;
    };

    struct JmpOp {
        std::string_view target;
    };

    struct PushOp {
        Operand src;
    };

    struct PopOp {
        VirtualRegister dest;
    };

    struct CallOp {
        std::string_view target; 
    };

    struct RetOp {}; 

    struct LabelDecl {
        std::string_view name;
    };

    // this way I can wrap all my strutural nods under a single type
    using ASTNode = std::variant<
        MovOp,
        AddOp,
        SubOp,
        JmpOp,
        PushOp,
        PopOp,
        CallOp,
        RetOp,
        LabelDecl
    >;
}

struct SyntaxError {
    std::string_view message; 
    size_t line;              
    size_t column;            
};

template <typename T>
using ParseResult = std::expected<T, SyntaxError>;