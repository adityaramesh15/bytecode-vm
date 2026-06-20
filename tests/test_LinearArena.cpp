#include <catch2/catch_test_macros.hpp>
// Include the explicit floating-point matchers header required by Catch2 v3
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "LinearArena.hpp"
#include <cstdint>

using namespace MemoryEngine;

TEST_CASE("LinearArena - Structural Instantiation and Reset Mechanics", "[Memory]") {
    LinearArena arena(1024);
    REQUIRE(arena.capacity() == 1024);
    REQUIRE(arena.bytes_used() == 0);
    
    auto view = arena.current_allocations();
    REQUIRE(view.empty());
}

TEST_CASE("LinearArena - Micro-Architectural Alignment Boundaries", "[Memory]") {
    LinearArena arena(64);
    
    char* char_ptr = arena.allocate<char>('Z');
    REQUIRE(*char_ptr == 'Z');
    REQUIRE(arena.bytes_used() == 1);
    
    int32_t* int_ptr = arena.allocate<int32_t>(1337);
    REQUIRE(*int_ptr == 1337);
    REQUIRE(reinterpret_cast<uintptr_t>(int_ptr) % 4 == 0);
    REQUIRE(arena.bytes_used() == 8);
    
    double* double_ptr = arena.allocate<double>(3.14159);
    
    // Catch2 v3 Idiomatic approach: Use REQUIRE_THAT with the WithinRel matcher
    REQUIRE_THAT(*double_ptr, Catch::Matchers::WithinRel(3.14159, 0.00001));
    REQUIRE(arena.bytes_used() == 16);
    
    auto view = arena.current_allocations();
    REQUIRE(view.size() == 16);
    REQUIRE(view.data() == reinterpret_cast<std::byte*>(char_ptr));
}

TEST_CASE("LinearArena - High-Performance O(1) Cleansing Passes", "[Memory]") {
    LinearArena arena(512);
    
    // Capturing nodiscard return values defensively inside the unit tests 
    // to prove allocations succeed while satisfying the -Werror constraint
    auto* ptr1 = arena.allocate<uint64_t>(100);
    auto* ptr2 = arena.allocate<uint64_t>(200);
    REQUIRE(ptr1 != nullptr);
    REQUIRE(ptr2 != nullptr);
    REQUIRE(arena.bytes_used() == 16);
    
    arena.reset();
    REQUIRE(arena.bytes_used() == 0);
    
    uint64_t* fresh_ptr = arena.allocate<uint64_t>(300);
    REQUIRE(*fresh_ptr == 300);
    REQUIRE(arena.bytes_used() == 8);
}

TEST_CASE("LinearArena - Defensive Boundary Conditions", "[Memory]") {
    LinearArena arena(8);
    
    REQUIRE_NOTHROW(arena.allocate<uint64_t>(0xFFFFFFFFFFFFFFFF));
    REQUIRE_THROWS_AS(arena.allocate<char>('A'), std::bad_alloc);
}

TEST_CASE("LinearArena - R-Value Move Semantics Ownership Propagation", "[Memory]") {
    LinearArena source(256);
    uint32_t* original_allocation = source.allocate<uint32_t>(777);
    REQUIRE(source.bytes_used() == 4);
    
    LinearArena destination(std::move(source));
    
    REQUIRE(source.capacity() == 0);
    REQUIRE(source.bytes_used() == 0);
    
    REQUIRE(destination.capacity() == 256);
    REQUIRE(destination.bytes_used() == 4);
    
    auto view = destination.current_allocations();
    uint32_t* recovered_ptr = reinterpret_cast<uint32_t*>(view.data());
    REQUIRE(*recovered_ptr == 777);
    REQUIRE(recovered_ptr == original_allocation);
}