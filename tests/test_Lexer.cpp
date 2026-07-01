#include <catch2/catch_test_macros.hpp>
#include "Lexer.hpp"

TEST_CASE("Lexer - Empty Input and EOF Boundary", "[Lexer]") {
    std::string_view source{};
    Lexer lexer(source);
    auto tokens = lexer.lex_input();

    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens[0].token == TokenType::EndOfFile);
    REQUIRE(tokens[0].lexeme.empty());
    REQUIRE(tokens[0].line == 1);
    REQUIRE(tokens[0].column == 1);
}

TEST_CASE("Lexer - Whitespace and Comment Stripping", "[Lexer]") {
    std::string_view source = "   \t  \r\n  ; This is a comment inside TitanVM\n   ";
    Lexer lexer(source);
    auto tokens = lexer.lex_input();

    // All elements are skipped, leaving only the EOF sentinel
    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens[0].token == TokenType::EndOfFile);
    // Line 1: space skips, \n moves to line 2.
    // Line 2: spaces, comment skips up to \n, moves to line 3.
    // Line 3: spaces up to EOF.
    REQUIRE(tokens[0].line == 3);
}

TEST_CASE("Lexer - Single Character Punctuation", "[Lexer]") {
    std::string_view source = ",,,,";
    Lexer lexer(source);
    auto tokens = lexer.lex_input();

    REQUIRE(tokens.size() == 5); // 4 commas + 1 EndOfFile
    for (size_t tok_idx = 0; tok_idx < 4; ++tok_idx) {
        REQUIRE(tokens[tok_idx].token == TokenType::Comma);
        REQUIRE(tokens[tok_idx].lexeme == ",");
        REQUIRE(tokens[tok_idx].line == 1);
        REQUIRE(tokens[tok_idx].column == (tok_idx + 1));
    }
}

TEST_CASE("Lexer - Numeric Immediates", "[Lexer]") {
    std::string_view source = "42 -100 0 -5";
    Lexer lexer(source);
    auto tokens = lexer.lex_input();

    REQUIRE(tokens.size() == 5); // 4 numbers + 1 EndOfFile

    REQUIRE(tokens[0].token == TokenType::Immediate);
    REQUIRE(tokens[0].lexeme == "42");

    REQUIRE(tokens[1].token == TokenType::Immediate);
    REQUIRE(tokens[1].lexeme == "-100");

    REQUIRE(tokens[2].token == TokenType::Immediate);
    REQUIRE(tokens[2].lexeme == "0");

    REQUIRE(tokens[3].token == TokenType::Immediate);
    REQUIRE(tokens[3].lexeme == "-5");
}

TEST_CASE("Lexer - Opcodes Identification", "[Lexer]") {
    std::string_view source = "MOV ADD SUB JMP PUSH POP CALL RET";
    Lexer lexer(source);
    auto tokens = lexer.lex_input();

    REQUIRE(tokens.size() == 9); // 8 opcodes + 1 EndOfFile
    
    std::vector<std::string_view> expected = {"MOV", "ADD", "SUB", "JMP", "PUSH", "POP", "CALL", "RET"};
    for (size_t op_idx = 0; op_idx < expected.size(); ++op_idx) {
        REQUIRE(tokens[op_idx].token == TokenType::Opcode);
        REQUIRE(tokens[op_idx].lexeme == expected[op_idx]);
    }
}

TEST_CASE("Lexer - Hardware Register Constraints (R0-R15)", "[Lexer]") {
    SECTION("Valid Registers Within Bounds") {
        std::string_view source = "R0 R7 R15";
        Lexer lexer(source);
        auto tokens = lexer.lex_input();

        REQUIRE(tokens.size() == 4);
        REQUIRE(tokens[0].token == TokenType::Register);
        REQUIRE(tokens[0].lexeme == "R0");
        
        REQUIRE(tokens[1].token == TokenType::Register);
        REQUIRE(tokens[1].lexeme == "R7");
        
        REQUIRE(tokens[2].token == TokenType::Register);
        REQUIRE(tokens[2].lexeme == "R15");
    }

    SECTION("Invalid Registers Out of Architectural Bounds") {
        // R16 and R99 exceed the 16 register CPU matrix, so they must degrade to regular Identifiers
        std::string_view source = "R16 R99 Rabc";
        Lexer lexer(source);
        auto tokens = lexer.lex_input();

        REQUIRE(tokens.size() == 4);
        REQUIRE(tokens[0].token == TokenType::Identifier);
        REQUIRE(tokens[0].lexeme == "R16");

        REQUIRE(tokens[1].token == TokenType::Identifier);
        REQUIRE(tokens[1].lexeme == "R99");

        REQUIRE(tokens[2].token == TokenType::Identifier);
        REQUIRE(tokens[2].lexeme == "Rabc");
    }
}

TEST_CASE("Lexer - Labels and Target Identifiers", "[Lexer]") {
    std::string_view source = "main: loop_start loop_start:";
    Lexer lexer(source);
    auto tokens = lexer.lex_input();

    REQUIRE(tokens.size() == 4);

    // Label declaration includes the colon in its lexeme slice
    REQUIRE(tokens[0].token == TokenType::Label);
    REQUIRE(tokens[0].lexeme == "main:");

    // Jump target target label reference (no colon)
    REQUIRE(tokens[1].token == TokenType::Identifier);
    REQUIRE(tokens[1].lexeme == "loop_start");

    REQUIRE(tokens[2].token == TokenType::Label);
    REQUIRE(tokens[2].lexeme == "loop_start:");
}

TEST_CASE("Lexer - Coordinate Tracking and Complex Structural Assembly", "[Lexer]") {
    // Multi-line real-world TitanVM assembly file simulation snippet
    std::string_view source = 
        "; Setup section\n"
        "main:\n"
        "    MOV R1, 42\n"
        "    ADD R1, -10\n"
        "    RET";

    Lexer lexer(source);
    auto tokens = lexer.lex_input();

    // Expected tokens: 
    // 0: Label(main:)
    // 1: Opcode(MOV), 2: Register(R1), 3: Comma(,), 4: Immediate(42)
    // 5: Opcode(ADD), 6: Register(R1), 7: Comma(,), 8: Immediate(-10)
    // 9: Opcode(RET)
    // 10: EndOfFile
    REQUIRE(tokens.size() == 11);

    // Validate specific strategic lookups
    
    // Check main: layout
    REQUIRE(tokens[0].token == TokenType::Label);
    REQUIRE(tokens[0].lexeme == "main:");
    REQUIRE(tokens[0].line == 2);
    REQUIRE(tokens[0].column == 1);

    // Check "MOV" coordinates on Line 3
    REQUIRE(tokens[1].token == TokenType::Opcode);
    REQUIRE(tokens[1].lexeme == "MOV");
    REQUIRE(tokens[1].line == 3);
    REQUIRE(tokens[1].column == 5); // 4 spaces indentation, starts at index 5

    // Check intermediate comma placement
    REQUIRE(tokens[3].token == TokenType::Comma);
    REQUIRE(tokens[3].line == 3);
    REQUIRE(tokens[3].column == 11);

    // Check "ADD" coordinates on Line 4
    REQUIRE(tokens[5].token == TokenType::Opcode);
    REQUIRE(tokens[5].lexeme == "ADD");
    REQUIRE(tokens[5].line == 4);
    REQUIRE(tokens[5].column == 5);

    // Check negative immediate tracing 
    REQUIRE(tokens[8].token == TokenType::Immediate);
    REQUIRE(tokens[8].lexeme == "-10");
    REQUIRE(tokens[8].line == 4);
    REQUIRE(tokens[8].column == 13);

    // Check terminal exit
    REQUIRE(tokens[9].token == TokenType::Opcode);
    REQUIRE(tokens[9].lexeme == "RET");
    REQUIRE(tokens[9].line == 5);
}

TEST_CASE("Lexer - Error Resilience / Faulty Inputs", "[Lexer]") {
    // Tests that unexpected layout symbols (like '@' or '$') do not trap the engine in an infinite loop
    std::string_view source = "@ $";
    Lexer lexer(source);
    auto tokens = lexer.lex_input();

    REQUIRE(tokens.size() == 3); // Fallback Identifier x2 + EOF
    REQUIRE(tokens[0].token == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "@");
    REQUIRE(tokens[1].token == TokenType::Identifier);
    REQUIRE(tokens[1].lexeme == "$");
}