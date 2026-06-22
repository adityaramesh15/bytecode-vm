#include <catch2/catch_test_macros.hpp>
#include "LinearArena.hpp"
#include "ArenaAllocator.hpp"
#include "Lexer.hpp"
#include <vector>

using namespace MemoryEngine;

TEST_CASE("Arena Allocator - Standard Vector Integration Pass", "[Memory]") {
    // 1. Initialize a 4KB local arena using type-safe C++23 size literals
    LinearArena arena(1024UZ * 4UZ);
    REQUIRE(arena.bytes_used() == 0UZ);

    {
        // 2. Bind our allocator to the arena instance
        ArenaAllocator<Token> allocator(arena);

        // 3. Instantiate a standard vector powered by our custom memory pool
        std::vector<Token, ArenaAllocator<Token>> token_stream(allocator);

        // Verify the stream begins empty
        REQUIRE(arena.bytes_used() == 0UZ);

        // 4. Append token ensuring coordinate primitives match size_t types cleanly
        token_stream.push_back(Token{TokenType::Opcode, "MOV", 1UZ, 1UZ});
        
        // Assert that memory metrics changed inside our backend engine
        REQUIRE(arena.bytes_used() > 0UZ);
        size_t baseline_memory = arena.bytes_used();

        // 5. Trigger a vector capacity expansion pass
        for (int i = 0; i < 20; ++i) {
            token_stream.push_back(Token{TokenType::Immediate, "42", 2UZ, 5UZ});
        }

        // Verify that internal container resizing drew space from the arena
        REQUIRE(arena.bytes_used() > baseline_memory);
    } 

    // 6. Vector scope ends here.
    SUCCEED("Container dismantled cleanly without memory layer faults.");
}