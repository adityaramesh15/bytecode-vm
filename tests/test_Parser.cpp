#include <catch2/catch_test_macros.hpp>
#include "Parser.hpp"

// Utility helper to chain Lexer and Parser sequences seamlessly
static ParseResult<std::vector<AST::ASTNode>> parse_assembly(std::string_view source) noexcept {
    Lexer lexer(source);
    auto tokens = lexer.lex_input();
    Parser parser(std::move(tokens));
    return parser.parse_program();
}

TEST_CASE("Parser - Valid Complex Assembly Sequence", "[Parser]") {
    std::string_view source = 
        "main:\n"
        "    MOV R1, 42\n"
        "    ADD R2, R1\n"
        "    PUSH R2\n"
        "    CALL print_val\n"
        "    RET";

    auto result = parse_assembly(source);
    
    // Assert structural parsing success
    REQUIRE(result.has_value());
    const auto& ast = *result;
    REQUIRE(ast.size() == 6); // Label, MOV, ADD, PUSH, CALL, RET

    // 1. Verify Label Declaration Node
    REQUIRE(std::holds_alternative<AST::LabelDecl>(ast[0]));
    REQUIRE(std::get<AST::LabelDecl>(ast[0]).name == "main");

    // 2. Verify MOV R1, 42 Node
    REQUIRE(std::holds_alternative<AST::MovOp>(ast[1]));
    const auto& mov = std::get<AST::MovOp>(ast[1]);
    REQUIRE(mov.dest.index == 1);
    REQUIRE(std::holds_alternative<int64_t>(mov.src.value));
    REQUIRE(std::get<int64_t>(mov.src.value) == 42);

    // 3. Verify ADD R2, R1 Node
    REQUIRE(std::holds_alternative<AST::AddOp>(ast[2]));
    const auto& add = std::get<AST::AddOp>(ast[2]);
    REQUIRE(add.dest.index == 2);
    REQUIRE(std::holds_alternative<VirtualRegister>(add.src.value));
    REQUIRE(std::get<VirtualRegister>(add.src.value).index == 1);

    // 4. Verify PUSH R2 Node
    REQUIRE(std::holds_alternative<AST::PushOp>(ast[3]));
    const auto& push = std::get<AST::PushOp>(ast[3]);
    REQUIRE(std::holds_alternative<VirtualRegister>(push.src.value));
    REQUIRE(std::get<VirtualRegister>(push.src.value).index == 2);

    // 5. Verify CALL print_val Node
    REQUIRE(std::holds_alternative<AST::CallOp>(ast[4]));
    REQUIRE(std::get<AST::CallOp>(ast[4]).target == "print_val");

    // 6. Verify RET Node
    REQUIRE(std::holds_alternative<AST::RetOp>(ast[5]));
}

TEST_CASE("Parser - Syntactic Error Bounds Handling", "[Parser]") {
    
    SECTION("Missing Delimiter Comma") {
        std::string_view broken_src = "MOV R1 100";
        auto result = parse_assembly(broken_src);
        
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().line == 1);
        // The error drops precisely where the comma was expected instead of the immediate
        REQUIRE_FALSE(result.error().message.empty());
    }

    SECTION("Invalid Instruction Target Operand Type") {
        std::string_view broken_src = "POP 500"; // POP requires a register target
        auto result = parse_assembly(broken_src);
        
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().line == 1);
    }

    SECTION("Out of Bounds Hardware Register Slicing") {
        // R16 degrades to an Identifier inside our Lexer, violating the MOV opcode's constraint
        std::string_view broken_src = "MOV R16, R0"; 
        auto result = parse_assembly(broken_src);
        
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Malformed Execution Operation") {
        std::string_view broken_src = "NOT_AN_OPCODE R1, R2";
        auto result = parse_assembly(broken_src);
        
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("Parser - Malformed Input Edge Cases", "[Parser]") {
    SECTION("Abrupt Instruction Truncation") {
        // Opcode cut off completely before any operands are parsed
        std::string_view malformed_src = "MOV";
        auto result = parse_assembly(malformed_src);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Dangling Separation Delimiters") {
        // Trailing comma with zero secondary argument data
        std::string_view malformed_src = "MOV R1, ";
        auto result = parse_assembly(malformed_src);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Illegal Token Infiltration") {
        // Stray characters that must cleanly fail monadic parsing
        std::string_view malformed_src = "ADD R1, @";
        auto result = parse_assembly(malformed_src);
        REQUIRE_FALSE(result.has_value());
    }
}