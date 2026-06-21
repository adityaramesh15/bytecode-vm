#include <catch2/catch_test_macros.hpp>
#include "LinearArena.hpp"
#include "ArenaAllocator.hpp"
#include "Lexer.hpp"
#include <vector>

using namespace MemoryEngine;

TEST_CASE("Arena Allocator - Standard Vector Integration Pass", "[Memory]") {
    // 1. Initialize a 4KB local arena
    LinearArena arena(1024 * 4);
    REQUIRE(arena.bytes_used() == 0);

    {
        // 2. Bind our allocator to the arena instance
        ArenaAllocator<Token> allocator(arena);

        // 3. Instantiate a standard vector powered by our custom memory pool
        std::vector<Token, ArenaAllocator<Token>> token_stream(allocator);

        // Verify the stream begins empty
        REQUIRE(arena.bytes_used() == 0);

        // 4. Force elements into the vector to trigger allocation activity
        token_stream.push_back(Token{TokenType::Opcode, "MOV", 1, 1});
        
        // Assert that memory metrics changed inside our backend engine
        REQUIRE(arena.bytes_used() > 0);
        size_t baseline_memory = arena.bytes_used();

        // 5. Trigger a vector capacity expansion pass
        for (int i = 0; i < 20; ++i) {
            token_stream.push_back(Token{TokenType::Immediate, "42", 2, 5});
        }

        // Verify that internal container resizing drew space from the arena
        REQUIRE(arena.bytes_used() > baseline_memory);
    } 

    // 6. Vector scope ends here.
    // Ensure all internal container resources cleared safely without throwing errors.
    SUCCEED("Container dismantled cleanly without memory layer faults.");
}