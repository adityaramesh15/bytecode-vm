#include <catch2/catch_test_macros.hpp>
#include "VirtualMachine.hpp"
#include "Parser.hpp"
#include "Lexer.hpp"
#include "LinearArena.hpp"

struct VMFixture {
    MemoryEngine::LinearArena arena{1024UZ * 64UZ};
    VirtualMachine vm;

    static VMFixture load(std::string_view source) {
        VMFixture fixture;
        Lexer lexer(source);
        Parser parser(lexer.lex_input(), fixture.arena);
        auto ast = parser.parse_program();
        REQUIRE(ast.has_value());
        auto load_result = fixture.vm.load_program(std::span{ast->data(), ast->size()});
        REQUIRE(load_result.has_value());
        return fixture;
    }

    static VMFixture run(std::string_view source) {
        auto fixture = load(source);
        auto run_result = fixture.vm.run();
        REQUIRE(run_result.has_value());
        return fixture;
    }
};

TEST_CASE("VirtualMachine - MOV immediate", "[VirtualMachine]") {
    auto fixture = VMFixture::run("MOV R0, 42");
    REQUIRE(fixture.vm.read_register(0) == 42);
}

TEST_CASE("VirtualMachine - ADD register operand", "[VirtualMachine]") {
    auto fixture = VMFixture::run(
        "MOV R1, 10\n"
        "ADD R2, R1\n");
    REQUIRE(fixture.vm.read_register(1) == 10);
    REQUIRE(fixture.vm.read_register(2) == 10);
}

TEST_CASE("VirtualMachine - SUB immediate", "[VirtualMachine]") {
    auto fixture = VMFixture::run(
        "MOV R0, 50\n"
        "SUB R0, 8\n");
    REQUIRE(fixture.vm.read_register(0) == 42);
}

TEST_CASE("VirtualMachine - ADD immediate", "[VirtualMachine]") {
    auto fixture = VMFixture::run(
        "MOV R0, 0\n"
        "ADD R0, 1\n");
    REQUIRE(fixture.vm.read_register(0) == 1);
}

TEST_CASE("VirtualMachine - JMP forward skips instruction", "[VirtualMachine]") {
    auto fixture = VMFixture::run(
        "MOV R0, 10\n"
        "JMP skip\n"
        "SUB R0, 5\n"
        "skip:\n"
        "MOV R1, 1\n");
    REQUIRE(fixture.vm.read_register(0) == 10);
    REQUIRE(fixture.vm.read_register(1) == 1);
}

TEST_CASE("VirtualMachine - chained JMP resolves labels", "[VirtualMachine]") {
    auto fixture = VMFixture::run(
        "MOV R0, 10\n"
        "JMP step\n"
        "SUB R0, 3\n"
        "step:\n"
        "SUB R0, 4\n"
        "JMP finish\n"
        "SUB R0, 99\n"
        "finish:\n"
        "MOV R1, R0\n");
    REQUIRE(fixture.vm.read_register(0) == 6);
    REQUIRE(fixture.vm.read_register(1) == 6);
}

TEST_CASE("VirtualMachine - label targets next instruction", "[VirtualMachine]") {
    auto fixture = VMFixture::load(
        "start:\n"
        "MOV R0, 7\n");
    auto run_result = fixture.vm.run();
    REQUIRE(run_result.has_value());
    REQUIRE(fixture.vm.read_register(0) == 7);
}

TEST_CASE("VirtualMachine - unknown label on JMP", "[VirtualMachine]") {
    auto fixture = VMFixture::load("JMP nowhere");
    auto run_result = fixture.vm.run();
    REQUIRE_FALSE(run_result.has_value());
    REQUIRE(run_result.error() == VMError::UnknownLabel);
}

TEST_CASE("VirtualMachine - unsupported opcode on load", "[VirtualMachine]") {
    MemoryEngine::LinearArena arena{1024UZ * 64UZ};
    Lexer lexer("PUSH R1");
    Parser parser(lexer.lex_input(), arena);
    auto ast = parser.parse_program();
    REQUIRE(ast.has_value());

    VirtualMachine virtual_machine;
    auto load_result = virtual_machine.load_program(std::span{ast->data(), ast->size()});
    REQUIRE_FALSE(load_result.has_value());
    REQUIRE(load_result.error() == VMError::UnsupportedOpcode);
}

TEST_CASE("VirtualMachine - reset clears register state", "[VirtualMachine]") {
    auto fixture = VMFixture::run("MOV R3, 99");
    REQUIRE(fixture.vm.read_register(3) == 99);
    REQUIRE(fixture.vm.instruction_pointer() > 0);
    fixture.vm.reset();
    REQUIRE(fixture.vm.read_register(3) == 0);
    REQUIRE(fixture.vm.instruction_pointer() == 0);
    REQUIRE(fixture.vm.stack_pointer() == 0);
    REQUIRE_FALSE(fixture.vm.is_running());
}

TEST_CASE("VirtualMachine - empty program run succeeds", "[VirtualMachine]") {
    VirtualMachine virtual_machine;
    auto run_result = virtual_machine.run();
    REQUIRE(run_result.has_value());
    REQUIRE(virtual_machine.program_size() == 0);
}

TEST_CASE("VirtualMachine - arithmetic chain", "[VirtualMachine]") {
    auto fixture = VMFixture::run(
        "MOV R0, 100\n"
        "SUB R0, 30\n"
        "ADD R0, 5\n"
        "MOV R1, R0\n");
    REQUIRE(fixture.vm.read_register(0) == 75);
    REQUIRE(fixture.vm.read_register(1) == 75);
}
