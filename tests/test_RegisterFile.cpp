#include <catch2/catch_test_macros.hpp>
#include "VirtualMachine.hpp"
#include "LinearArena.hpp"
#include "VirtualMemory.hpp"

TEST_CASE("RegisterFile - layout fits L1 cache footprint", "[RegisterFile]") {
    static_assert(alignof(RegisterFile) >= 64);
    static_assert(sizeof(RegisterFile{}.gpr) == 128);
    static_assert(sizeof(RegisterFile) <= 256);
    REQUIRE(RegisterFile::GPR_COUNT == 16);
}

TEST_CASE("RegisterFile - reset clears gpr ip and sp", "[RegisterFile]") {
    RegisterFile cpu{};
    cpu.gpr[0] = 42;
    cpu.gpr[15] = 99;
    cpu.ip = 7;
    cpu.sp = 3;
    cpu.reset();
    REQUIRE(cpu.read_gpr(0) == 0);
    REQUIRE(cpu.read_gpr(15) == 0);
    REQUIRE(cpu.ip == 0);
    REQUIRE(cpu.sp == 0);
}

TEST_CASE("RegisterFile - all 16 gpr slots are addressable", "[RegisterFile]") {
    RegisterFile cpu{};
    for (uint8_t reg_idx = 0; reg_idx < RegisterFile::GPR_COUNT; ++reg_idx) {
        cpu.gpr[reg_idx] = static_cast<int64_t>(reg_idx) * 10;
        REQUIRE(cpu.read_gpr(reg_idx) == static_cast<int64_t>(reg_idx) * 10);
    }
}

TEST_CASE("RegisterFile - out of range gpr read throws", "[RegisterFile]") {
    RegisterFile cpu{};
    REQUIRE_THROWS_AS(cpu.read_gpr(16), std::out_of_range);
}

TEST_CASE("VirtualMachine - reset clears ip and sp", "[RegisterFile]") {
    MemoryEngine::LinearArena arena{1024UZ * 64UZ};
    MemoryEngine::MemoryManagementUnit mmu{arena};
    VirtualMachine virtual_machine{mmu, arena};
    virtual_machine.reset();
    REQUIRE(virtual_machine.instruction_pointer() == 0);
    REQUIRE(virtual_machine.stack_pointer() == 0);
}
