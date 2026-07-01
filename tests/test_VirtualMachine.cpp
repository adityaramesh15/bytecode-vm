#include <catch2/catch_test_macros.hpp>
#include "VirtualMachine.hpp"
#include "BytecodeEmitter.hpp"
#include "BytecodeFile.hpp"
#include "Parser.hpp"
#include "Lexer.hpp"
#include "LinearArena.hpp"

namespace {

struct VMFixture {
    MemoryEngine::LinearArena arena{1024UZ * 64UZ};
    MemoryEngine::MemoryManagementUnit mmu{arena};
    VirtualMachine virtual_machine{mmu, arena};
};

void load_source(VMFixture& fixture, std::string_view source) {
    Lexer lexer(source);
    Parser parser(lexer.lex_input(), fixture.arena);
    const auto ast = parser.parse_program();
    REQUIRE(ast.has_value());

    BytecodeEmitter emitter;
    const auto emitted = emitter.emit(std::span{ast->data(), ast->size()});
    REQUIRE(emitted.has_value());

    const auto load_result = fixture.virtual_machine.load_program(std::span{emitted->code});
    REQUIRE(load_result.has_value());
}

void run_source(VMFixture& fixture, std::string_view source) {
    load_source(fixture, source);
    const auto run_result = fixture.virtual_machine.run();
    REQUIRE(run_result.has_value());
}

}  // namespace

TEST_CASE("VirtualMachine - MOV immediate", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture, "MOV R0, 42");
    REQUIRE(fixture.virtual_machine.read_register(0) == 42);
}

TEST_CASE("VirtualMachine - ADD register operand", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture,
        "MOV R1, 10\n"
        "ADD R2, R1\n");
    REQUIRE(fixture.virtual_machine.read_register(1) == 10);
    REQUIRE(fixture.virtual_machine.read_register(2) == 10);
}

TEST_CASE("VirtualMachine - SUB immediate", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture,
        "MOV R0, 50\n"
        "SUB R0, 8\n");
    REQUIRE(fixture.virtual_machine.read_register(0) == 42);
}

TEST_CASE("VirtualMachine - ADD immediate", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture,
        "MOV R0, 0\n"
        "ADD R0, 1\n");
    REQUIRE(fixture.virtual_machine.read_register(0) == 1);
}

TEST_CASE("VirtualMachine - JMP forward skips instruction", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture,
        "MOV R0, 10\n"
        "JMP skip\n"
        "SUB R0, 5\n"
        "skip:\n"
        "MOV R1, 1\n");
    REQUIRE(fixture.virtual_machine.read_register(0) == 10);
    REQUIRE(fixture.virtual_machine.read_register(1) == 1);
}

TEST_CASE("VirtualMachine - chained JMP resolves labels", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture,
        "MOV R0, 10\n"
        "JMP step\n"
        "SUB R0, 3\n"
        "step:\n"
        "SUB R0, 4\n"
        "JMP finish\n"
        "SUB R0, 99\n"
        "finish:\n"
        "MOV R1, R0\n");
    REQUIRE(fixture.virtual_machine.read_register(0) == 6);
    REQUIRE(fixture.virtual_machine.read_register(1) == 6);
}

TEST_CASE("VirtualMachine - label targets next instruction", "[VirtualMachine]") {
    VMFixture fixture;
    load_source(fixture,
        "start:\n"
        "MOV R0, 7\n");
    const auto run_result = fixture.virtual_machine.run();
    REQUIRE(run_result.has_value());
    REQUIRE(fixture.virtual_machine.read_register(0) == 7);
}

TEST_CASE("VirtualMachine - unknown label on JMP fails at emit", "[VirtualMachine]") {
    VMFixture fixture;
    Lexer lexer("JMP nowhere");
    Parser parser(lexer.lex_input(), fixture.arena);
    const auto ast = parser.parse_program();
    REQUIRE(ast.has_value());

    BytecodeEmitter emitter;
    const auto emitted = emitter.emit(std::span{ast->data(), ast->size()});
    REQUIRE_FALSE(emitted.has_value());
    REQUIRE(emitted.error() == VMError::UnknownLabel);
}

TEST_CASE("VirtualMachine - PUSH and POP round trip", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture,
        "MOV R1, 42\n"
        "PUSH R1\n"
        "MOV R1, 0\n"
        "POP R2\n");
    REQUIRE(fixture.virtual_machine.read_register(1) == 0);
    REQUIRE(fixture.virtual_machine.read_register(2) == 42);
    REQUIRE(fixture.virtual_machine.stack_pointer() == 0);
}

TEST_CASE("VirtualMachine - CALL and RET resume caller", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture,
        "MOV R0, 0\n"
        "MOV R1, 10\n"
        "CALL add_ten\n"
        "MOV R2, R0\n"
        "JMP done\n"
        "add_ten:\n"
        "ADD R0, R1\n"
        "RET\n"
        "done:\n"
        "MOV R3, 1\n");
    REQUIRE(fixture.virtual_machine.read_register(0) == 10);
    REQUIRE(fixture.virtual_machine.read_register(2) == 10);
    REQUIRE(fixture.virtual_machine.read_register(3) == 1);
}

TEST_CASE("VirtualMachine - nested CALL and RET", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture,
        "MOV R0, 1\n"
        "CALL double_it\n"
        "JMP end\n"
        "double_it:\n"
        "ADD R0, R0\n"
        "CALL double_it_again\n"
        "RET\n"
        "double_it_again:\n"
        "ADD R0, R0\n"
        "RET\n"
        "end:\n"
        "MOV R1, R0\n");
    REQUIRE(fixture.virtual_machine.read_register(0) == 4);
    REQUIRE(fixture.virtual_machine.read_register(1) == 4);
}

TEST_CASE("VirtualMachine - RET on empty stack fails", "[VirtualMachine]") {
    VMFixture fixture;
    load_source(fixture, "RET");
    const auto run_result = fixture.virtual_machine.run();
    REQUIRE_FALSE(run_result.has_value());
    REQUIRE(run_result.error() == VMError::StackUnderflow);
}

TEST_CASE("VirtualMachine - POP on empty stack fails", "[VirtualMachine]") {
    VMFixture fixture;
    load_source(fixture, "POP R0");
    const auto run_result = fixture.virtual_machine.run();
    REQUIRE_FALSE(run_result.has_value());
    REQUIRE(run_result.error() == VMError::StackUnderflow);
}

TEST_CASE("VirtualMachine - unknown label on CALL fails at emit", "[VirtualMachine]") {
    VMFixture fixture;
    Lexer lexer("CALL nowhere");
    Parser parser(lexer.lex_input(), fixture.arena);
    const auto ast = parser.parse_program();
    REQUIRE(ast.has_value());

    BytecodeEmitter emitter;
    const auto emitted = emitter.emit(std::span{ast->data(), ast->size()});
    REQUIRE_FALSE(emitted.has_value());
    REQUIRE(emitted.error() == VMError::UnknownLabel);
}

TEST_CASE("VirtualMachine - PUSH loads successfully", "[VirtualMachine]") {
    VMFixture fixture;
    load_source(fixture, "PUSH R1");
    REQUIRE(fixture.virtual_machine.program_size() == 4UZ);
}

TEST_CASE("VirtualMachine - reset clears register state", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture, "MOV R3, 99");
    REQUIRE(fixture.virtual_machine.read_register(3) == 99);
    REQUIRE(fixture.virtual_machine.instruction_pointer() > 0);
    fixture.virtual_machine.reset();
    REQUIRE(fixture.virtual_machine.read_register(3) == 0);
    REQUIRE(fixture.virtual_machine.instruction_pointer() == 0);
    REQUIRE(fixture.virtual_machine.stack_pointer() == 0);
    REQUIRE_FALSE(fixture.virtual_machine.is_running());
}

TEST_CASE("VirtualMachine - empty program run succeeds", "[VirtualMachine]") {
    VMFixture fixture;
    const auto run_result = fixture.virtual_machine.run();
    REQUIRE(run_result.has_value());
    REQUIRE(fixture.virtual_machine.program_size() == 0);
}

TEST_CASE("VirtualMachine - arithmetic chain", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture,
        "MOV R0, 100\n"
        "SUB R0, 30\n"
        "ADD R0, 5\n"
        "MOV R1, R0\n");
    REQUIRE(fixture.virtual_machine.read_register(0) == 75);
    REQUIRE(fixture.virtual_machine.read_register(1) == 75);
}

TEST_CASE("VirtualMachine - rejects truncated bytecode", "[VirtualMachine]") {
    VMFixture fixture;
    // MOV R1, imm header without the required immediate extension word
    const std::vector<std::byte> truncated{
        std::byte{0x00}, std::byte{0xF0}, std::byte{0x21}, std::byte{0x01},
    };
    const auto load_result = fixture.virtual_machine.load_program(truncated);
    REQUIRE_FALSE(load_result.has_value());
    REQUIRE(load_result.error() == VMError::InvalidBytecode);
}

TEST_CASE("VirtualMachine - guest code pages are not writable after load", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture, "MOV R0, 42");

    const auto write_attempt = fixture.mmu.translate(0x00400000U, true);
    REQUIRE_FALSE(write_attempt.has_value());
}

TEST_CASE("VirtualMachine - load from file round trip", "[VirtualMachine]") {
    VMFixture fixture;
    run_source(fixture, "MOV R5, 99");

    constexpr const char* kPath = "test_vm_roundtrip.bcmv";
    Lexer lexer("MOV R5, 99");
    Parser parser(lexer.lex_input(), fixture.arena);
    const auto ast = parser.parse_program();
    REQUIRE(ast.has_value());

    BytecodeEmitter emitter;
    const auto emitted = emitter.emit(std::span{ast->data(), ast->size()});
    REQUIRE(emitted.has_value());
    REQUIRE(Bytecode::write_file(kPath, emitted->code).has_value());

    VMFixture fresh;
    REQUIRE(fresh.virtual_machine.load_program_from_file(kPath).has_value());
    REQUIRE(fresh.virtual_machine.run().has_value());
    REQUIRE(fresh.virtual_machine.read_register(5) == 99);
}
