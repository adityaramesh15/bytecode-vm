#include <catch2/catch_test_macros.hpp>
#include "CompilerConcepts.hpp"
#include <string>
#include <vector>

// --- Register Fault Configurations ---
struct RegisterWithNonIntegralIndex {
    std::string index; // Fails: requires std::integral<decltype(reg.index)>
};

struct NonTriviallyCopyableRegister {
    uint8_t index;
    std::vector<int> heap_allocated_bloat; // Fails: std::is_trivially_copyable_v
};

struct BloatedRegisterLayout {
    uint8_t index;
    uint8_t structural_padding[4]; // Size = 5. Fails: sizeof(T) <= 2
};

// --- Instruction Fault Configurations ---
struct InstructionWithNonEnumOpcode {
    int opcode; // Fails: std::is_enum_v
    uint8_t operand_count;
    Operand operands[2];
};

struct InstructionWithNonIntegralCounter {
    Opcode opcode;
    std::string operand_count; // Fails: requires std::integral<decltype(ins.operand_count)>
    Operand operands[2];
};

struct InstructionMissingSubscriptInterface {
    Opcode opcode;
    uint8_t operand_count;
    // Missing "operands" array property. Fails: { ins.operands[0] }
};

struct BloatedInstructionLayout {
    Opcode opcode;
    uint8_t operand_count;
    Operand operands[8]; // Exceeds size limits. Fails: sizeof(T) <= 64
};


// ============================================================================
// CATCH2 COMPILE-TIME CONCEPTS TEST CASES
// ============================================================================

TEST_CASE("Compiler Concepts - Architectural Register Constraints", "[Concepts]") {
    // 1. Validate Production Type Integrity
    STATIC_REQUIRE(IsRegister<VirtualRegister>);
    
    // 2. Validate Defensive Boundaries (Negative Cases)
    STATIC_REQUIRE_FALSE(IsRegister<RegisterWithNonIntegralIndex>);
    STATIC_REQUIRE_FALSE(IsRegister<NonTriviallyCopyableRegister>);
    STATIC_REQUIRE_FALSE(IsRegister<BloatedRegisterLayout>);
    
    // 3. Reject Fundamental Primitive Type Mismatches
    STATIC_REQUIRE_FALSE(IsRegister<int>);
    STATIC_REQUIRE_FALSE(IsRegister<std::string_view>);
}

TEST_CASE("Compiler Concepts - Executable Instruction Unit Constraints", "[Concepts]") {
    // 1. Validate Production Type Integrity
    STATIC_REQUIRE(IsInstruction<Instruction>);
    
    // 2. Validate Defensive Boundaries (Negative Cases)
    STATIC_REQUIRE_FALSE(IsInstruction<InstructionWithNonEnumOpcode>);
    STATIC_REQUIRE_FALSE(IsInstruction<InstructionWithNonIntegralCounter>);
    STATIC_REQUIRE_FALSE(IsInstruction<InstructionMissingSubscriptInterface>);
    STATIC_REQUIRE_FALSE(IsInstruction<BloatedInstructionLayout>);
    
    // 3. Reject Fundamental Primitive Type Mismatches
    STATIC_REQUIRE_FALSE(IsInstruction<double>);
    STATIC_REQUIRE_FALSE(IsInstruction<VirtualRegister>);
}