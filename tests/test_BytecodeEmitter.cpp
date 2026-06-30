#include <catch2/catch_test_macros.hpp>
#include "BytecodeEmitter.hpp"
#include "BytecodeFormat.hpp"
#include "Parser.hpp"
#include "Lexer.hpp"
#include "LinearArena.hpp"

namespace {
struct EmitterFixture {
    MemoryEngine::LinearArena arena{1024UZ * 64UZ};

    static EmitterFixture emit(std::string_view source) {
        EmitterFixture fixture;
        Lexer lexer(source);
        Parser parser(lexer.lex_input(), fixture.arena);
        auto ast = parser.parse_program();
        REQUIRE(ast.has_value());

        BytecodeEmitter emitter;
        fixture.result = emitter.emit(std::span{ast->data(), ast->size()});
        REQUIRE(fixture.result.has_value());
        return fixture;
    }

    VMResult<BytecodeEmitter::EmitResult> result{std::unexpected(VMError::UnsupportedOpcode)};
};

[[nodiscard]] Bytecode::DecodedInstruction decode_first(const BytecodeEmitter::EmitResult& emitted) {
    Bytecode::DecodedInstruction decoded{};
    REQUIRE(Bytecode::decode_at(emitted.code.data(), 0UZ, emitted.code.size(), decoded));
    return decoded;
}
}  // namespace

TEST_CASE("BytecodeEmitter - MOV immediate encodes 8 bytes", "[BytecodeEmitter]") {
    const auto fixture = EmitterFixture::emit("MOV R1, 42");
    REQUIRE(fixture.result->code.size() == 8UZ);

    const auto decoded = decode_first(*fixture.result);
    REQUIRE(decoded.opcode == Opcode::MOV);
    REQUIRE(decoded.reg_a == 1U);
    REQUIRE(decoded.op1_kind == Bytecode::OperandKind::Immediate);
    REQUIRE(decoded.ext1 == 42);
}

TEST_CASE("BytecodeEmitter - ADD register encodes 4 bytes", "[BytecodeEmitter]") {
    const auto fixture = EmitterFixture::emit(
        "MOV R1, 10\n"
        "ADD R2, R1\n");
    REQUIRE(fixture.result->code.size() == 12UZ);

    Bytecode::DecodedInstruction add{};
    REQUIRE(Bytecode::decode_at(fixture.result->code.data(), 8UZ, fixture.result->code.size(), add));
    REQUIRE(add.opcode == Opcode::ADD);
    REQUIRE(add.reg_a == 2U);
    REQUIRE(add.reg_b == 1U);
    REQUIRE(add.size_bytes == 4UZ);
}

TEST_CASE("BytecodeEmitter - JMP resolves forward label byte offset", "[BytecodeEmitter]") {
    const auto fixture = EmitterFixture::emit(
        "MOV R0, 10\n"
        "JMP skip\n"
        "SUB R0, 5\n"
        "skip:\n"
        "MOV R1, 1\n");

    Bytecode::DecodedInstruction jmp{};
    REQUIRE(Bytecode::decode_at(fixture.result->code.data(), 8UZ, fixture.result->code.size(), jmp));
    REQUIRE(jmp.opcode == Opcode::JMP);
    REQUIRE(jmp.ext0 == 24);  // skip: begins after MOV(8) + JMP(8) + SUB(8)
}

TEST_CASE("BytecodeEmitter - unknown label fails", "[BytecodeEmitter]") {
    MemoryEngine::LinearArena arena{1024UZ};
    Lexer lexer("JMP nowhere");
    Parser parser(lexer.lex_input(), arena);
    auto ast = parser.parse_program();
    REQUIRE(ast.has_value());

    BytecodeEmitter emitter;
    const auto result = emitter.emit(std::span{ast->data(), ast->size()});
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == VMError::UnknownLabel);
}

TEST_CASE("BytecodeEmitter - CALL and RET program emits successfully", "[BytecodeEmitter]") {
    const auto fixture = EmitterFixture::emit(
        "MOV R0, 0\n"
        "CALL add_ten\n"
        "RET\n"
        "add_ten:\n"
        "ADD R0, R0\n"
        "RET\n");
    REQUIRE_FALSE(fixture.result->code.empty());
}
