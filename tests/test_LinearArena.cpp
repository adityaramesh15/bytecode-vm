#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "LinearArena.hpp"
#include <cstdint>
#include <unistd.h>

using namespace MemoryEngine;

TEST_CASE("LinearArena - Structural Instantiation and Reset Mechanics", "[Memory]") {
    const auto system_page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    LinearArena arena(1024);
    
    REQUIRE(arena.capacity() == system_page_size);
    REQUIRE(arena.bytes_used() == 0);
    
    auto view = arena.current_allocations();
    REQUIRE(view.empty());
}

TEST_CASE("LinearArena - Micro-Architectural Alignment Boundaries", "[Memory]") {
    LinearArena arena(64);
    
    auto* char_ptr = arena.allocate<char>('Z');
    REQUIRE(*char_ptr == 'Z');
    REQUIRE(arena.bytes_used() == 1);
    
    auto* int_ptr = arena.allocate<int32_t>(1337);
    REQUIRE(*int_ptr == 1337);
    REQUIRE(reinterpret_cast<uintptr_t>(int_ptr) % 4 == 0);
    REQUIRE(arena.bytes_used() == 8);
    
    auto* double_ptr = arena.allocate<double>(123.456);
    
    REQUIRE_THAT(*double_ptr, Catch::Matchers::WithinRel(123.456, 0.00001));
    REQUIRE(arena.bytes_used() == 16);
    
    auto view = arena.current_allocations();
    REQUIRE(view.size() == 16);
    REQUIRE(view.data() == reinterpret_cast<std::byte*>(char_ptr));
}

TEST_CASE("LinearArena - High-Performance O(1) Cleansing Passes", "[Memory]") {
    LinearArena arena(512);
    
    auto* ptr1 = arena.allocate<uint64_t>(100ULL);
    auto* ptr2 = arena.allocate<uint64_t>(200ULL);
    REQUIRE(ptr1 != nullptr);
    REQUIRE(ptr2 != nullptr);
    REQUIRE(arena.bytes_used() == 16);
    
    arena.reset();
    REQUIRE(arena.bytes_used() == 0);
    
    auto* fresh_ptr = arena.allocate<uint64_t>(300ULL);
    REQUIRE(*fresh_ptr == 300ULL);
    REQUIRE(arena.bytes_used() == 8);
}

TEST_CASE("LinearArena - Defensive Boundary Conditions", "[Memory]") {
    LinearArena arena(8);
    
    REQUIRE_NOTHROW(arena.allocate<uint64_t>(0xFFFFFFFFFFFFFFFFULL));
    
    // Adjusted to overflow the actual page-aligned capacity instead of the requested 8 bytes
    REQUIRE_THROWS_AS(arena.allocate_raw<char>(arena.capacity() + 1UZ), std::bad_alloc);
}

TEST_CASE("LinearArena - R-Value Move Semantics Ownership Propagation", "[Memory]") {
    const auto system_page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    LinearArena source(256);
    
    auto* original_allocation = source.allocate<uint32_t>(777U);
    REQUIRE(source.bytes_used() == 4);
    
    LinearArena destination(std::move(source));
    
    REQUIRE(source.capacity() == 0);
    REQUIRE(source.bytes_used() == 0);
    
    // Adjusted to validate page-aligned capacity persistence post-move
    REQUIRE(destination.capacity() == system_page_size);
    REQUIRE(destination.bytes_used() == 4);
    
    auto view = destination.current_allocations();
    auto* recovered_ptr = reinterpret_cast<uint32_t*>(view.data());
    REQUIRE(*recovered_ptr == 777U);
    REQUIRE(recovered_ptr == original_allocation);
}