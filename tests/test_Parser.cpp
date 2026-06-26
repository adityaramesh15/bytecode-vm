#include <catch2/catch_test_macros.hpp>
#include "Parser.hpp"
#include "LinearArena.hpp"

struct ParseFixture {
    MemoryEngine::LinearArena arena{1024UZ * 64UZ};
    ParseResult<std::vector<AST::ASTNode>> result;

    static ParseFixture run(std::string_view source) {
        ParseFixture fixture;
        Lexer lexer(source);
        Parser parser(lexer.lex_input(), fixture.arena);
        fixture.result = parser.parse_program();
        return fixture;
    }
};

TEST_CASE("Parser - Valid Complex Assembly Sequence", "[Parser]") {
    std::string_view source = 
        "main:\n"
        "    MOV R1, 42\n"
        "    ADD R2, R1\n"
        "    PUSH R2\n"
        "    CALL print_val\n"
        "    RET";

    auto fixture = ParseFixture::run(source);
    
    REQUIRE(fixture.result.has_value());
    const auto& ast = fixture.result.value();
    REQUIRE(ast.size() == 6); // Label, MOV, ADD, PUSH, CALL, RET

    REQUIRE(std::holds_alternative<AST::LabelDecl>(ast[0]));
    REQUIRE(std::get<AST::LabelDecl>(ast[0]).name == "main");

    REQUIRE(std::holds_alternative<AST::MovOp>(ast[1]));
    const auto& mov = std::get<AST::MovOp>(ast[1]);
    REQUIRE(mov.dest.index == 1);
    REQUIRE(std::holds_alternative<int64_t>(mov.src.value));
    REQUIRE(std::get<int64_t>(mov.src.value) == 42);

    REQUIRE(std::holds_alternative<AST::AddOp>(ast[2]));
    const auto& add = std::get<AST::AddOp>(ast[2]);
    REQUIRE(add.dest.index == 2);
    REQUIRE(std::holds_alternative<VirtualRegister>(add.src.value));
    REQUIRE(std::get<VirtualRegister>(add.src.value).index == 1);

    REQUIRE(std::holds_alternative<AST::PushOp>(ast[3]));
    const auto& push = std::get<AST::PushOp>(ast[3]);
    REQUIRE(std::holds_alternative<VirtualRegister>(push.src.value));
    REQUIRE(std::get<VirtualRegister>(push.src.value).index == 2);

    REQUIRE(std::holds_alternative<AST::CallOp>(ast[4]));
    REQUIRE(std::get<AST::CallOp>(ast[4]).target == "print_val");

    REQUIRE(std::holds_alternative<AST::RetOp>(ast[5]));
}

TEST_CASE("Parser - Syntactic Error Bounds Handling", "[Parser]") {
    
    SECTION("Missing Delimiter Comma") {
        auto fixture = ParseFixture::run("MOV R1 100");
        REQUIRE_FALSE(fixture.result.has_value());
        REQUIRE(fixture.result.error().line == 1);
        REQUIRE_FALSE(fixture.result.error().message.empty());
    }

    SECTION("Invalid Instruction Target Operand Type") {
        auto fixture = ParseFixture::run("POP 500");
        REQUIRE_FALSE(fixture.result.has_value());
        REQUIRE(fixture.result.error().line == 1);
    }

    SECTION("Out of Bounds Hardware Register Slicing") {
        auto fixture = ParseFixture::run("MOV R16, R0");
        REQUIRE_FALSE(fixture.result.has_value());
    }

    SECTION("Malformed Execution Operation") {
        auto fixture = ParseFixture::run("NOT_AN_OPCODE R1, R2");
        REQUIRE_FALSE(fixture.result.has_value());
    }
}

TEST_CASE("Parser - Malformed Input Edge Cases", "[Parser]") {
    SECTION("Abrupt Instruction Truncation") {
        auto fixture = ParseFixture::run("MOV");
        REQUIRE_FALSE(fixture.result.has_value());
    }

    SECTION("Dangling Separation Delimiters") {
        auto fixture = ParseFixture::run("MOV R1, ");
        REQUIRE_FALSE(fixture.result.has_value());
    }

    SECTION("Illegal Token Infiltration") {
        auto fixture = ParseFixture::run("ADD R1, @");
        REQUIRE_FALSE(fixture.result.has_value());
    }

    SECTION("Jump target with label colon suffix") {
        auto fixture = ParseFixture::run("JMP main:");
        REQUIRE(fixture.result.has_value());
        const auto& ast = fixture.result.value();
        REQUIRE(ast.size() == 1);
        REQUIRE(std::holds_alternative<AST::JmpOp>(ast[0]));
        REQUIRE(std::get<AST::JmpOp>(ast[0]).target == "main");
    }
}
