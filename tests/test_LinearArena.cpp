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
    
    auto char_ptr = arena.allocate<char>('Z');
    REQUIRE(*char_ptr == 'Z');
    REQUIRE(arena.bytes_used() == 1);
    
    // Satisfies modernize-use-auto by removing type duplication
    auto int_ptr = arena.allocate<int32_t>(1337);
    REQUIRE(*int_ptr == 1337);
    REQUIRE(reinterpret_cast<uintptr_t>(int_ptr) % 4 == 0);
    REQUIRE(arena.bytes_used() == 8);
    
    // Replaced 3.14159 with an arbitrary double to satisfy modernize-use-std-numbers
    auto double_ptr = arena.allocate<double>(123.456);
    
    // Catch2 v3 Idiomatic approach: Use REQUIRE_THAT with the WithinRel matcher
    REQUIRE_THAT(*double_ptr, Catch::Matchers::WithinRel(123.456, 0.00001));
    REQUIRE(arena.bytes_used() == 16);
    
    auto view = arena.current_allocations();
    REQUIRE(view.size() == 16);
    REQUIRE(view.data() == reinterpret_cast<std::byte*>(char_ptr));
}

TEST_CASE("LinearArena - High-Performance O(1) Cleansing Passes", "[Memory]") {
    LinearArena arena(512);
    
    // Suffix 'ull' eliminates sign-conversion compiler warnings cleanly
    auto ptr1 = arena.allocate<uint64_t>(100ull);
    auto ptr2 = arena.allocate<uint64_t>(200ull);
    REQUIRE(ptr1 != nullptr);
    REQUIRE(ptr2 != nullptr);
    REQUIRE(arena.bytes_used() == 16);
    
    arena.reset();
    REQUIRE(arena.bytes_used() == 0);
    
    auto fresh_ptr = arena.allocate<uint64_t>(300ull);
    REQUIRE(*fresh_ptr == 300ull);
    REQUIRE(arena.bytes_used() == 8);
}

TEST_CASE("LinearArena - Defensive Boundary Conditions", "[Memory]") {
    LinearArena arena(8);
    
    REQUIRE_NOTHROW(arena.allocate<uint64_t>(0xFFFFFFFFFFFFFFFFull));
    REQUIRE_THROWS_AS(arena.allocate<char>('A'), std::bad_alloc);
}

TEST_CASE("LinearArena - R-Value Move Semantics Ownership Propagation", "[Memory]") {
    LinearArena source(256);
    // Suffix 'u' ensures an exact match with uint32_t
    auto original_allocation = source.allocate<uint32_t>(777u);
    REQUIRE(source.bytes_used() == 4);
    
    LinearArena destination(std::move(source));
    
    REQUIRE(source.capacity() == 0);
    REQUIRE(source.bytes_used() == 0);
    
    REQUIRE(destination.capacity() == 256);
    REQUIRE(destination.bytes_used() == 4);
    
    auto view = destination.current_allocations();
    auto recovered_ptr = reinterpret_cast<uint32_t*>(view.data());
    REQUIRE(*recovered_ptr == 777u);
    REQUIRE(recovered_ptr == original_allocation);
}