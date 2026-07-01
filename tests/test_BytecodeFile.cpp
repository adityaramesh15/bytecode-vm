#include <catch2/catch_test_macros.hpp>
#include "BytecodeFile.hpp"
#include "BytecodeEmitter.hpp"
#include "Parser.hpp"
#include "Lexer.hpp"
#include "LinearArena.hpp"
#include <fstream>
#include <vector>

namespace {

constexpr const char* kTestPath = "test_bytecode_file.bcmv";

[[nodiscard]] std::vector<std::byte> emit_source(
    std::string_view source,
    MemoryEngine::LinearArena& arena)
{
    Lexer lexer(source);
    Parser parser(lexer.lex_input(), arena);
    const auto ast = parser.parse_program();
    REQUIRE(ast.has_value());

    BytecodeEmitter emitter;
    const auto emitted = emitter.emit(std::span{ast->data(), ast->size()});
    REQUIRE(emitted.has_value());
    return emitted->code;
}

}  // namespace

TEST_CASE("BytecodeFile - write and read round trip", "[BytecodeFile]") {
    MemoryEngine::LinearArena arena{1024UZ * 64UZ};
    const std::vector<std::byte> original = emit_source("MOV R1, 42", arena);

    REQUIRE(Bytecode::write_file(kTestPath, original).has_value());

    const auto loaded = Bytecode::read_file(kTestPath);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->size() == original.size());
    REQUIRE(*loaded == original);
}

TEST_CASE("BytecodeFile - empty payload round trip", "[BytecodeFile]") {
    const std::vector<std::byte> empty;
    REQUIRE(Bytecode::write_file(kTestPath, empty).has_value());

    const auto loaded = Bytecode::read_file(kTestPath);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->empty());
}

TEST_CASE("BytecodeFile - rejects invalid magic", "[BytecodeFile]") {
    const std::byte bad_file[] = {
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    };

    std::ofstream output_stream(kTestPath, std::ios::binary | std::ios::trunc);
    REQUIRE(output_stream);
    output_stream.write(reinterpret_cast<const char*>(bad_file), static_cast<std::streamsize>(sizeof(bad_file)));
    REQUIRE(output_stream);

    const auto loaded = Bytecode::read_file(kTestPath);
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(loaded.error() == VMError::InvalidBytecode);
}

TEST_CASE("BytecodeFile - rejects trailing garbage", "[BytecodeFile]") {
    MemoryEngine::LinearArena arena{1024UZ * 64UZ};
    std::vector<std::byte> payload = emit_source("MOV R0, 1", arena);

    std::byte header[Bytecode::FILE_HEADER_SIZE];
    Bytecode::write_u32_le(header + 0, Bytecode::FILE_MAGIC);
    Bytecode::write_u32_le(header + 4, Bytecode::FILE_VERSION);
    Bytecode::write_u32_le(header + 8, static_cast<uint32_t>(payload.size()));

    std::ofstream output_stream(kTestPath, std::ios::binary | std::ios::trunc);
    REQUIRE(output_stream);
    output_stream.write(reinterpret_cast<const char*>(header), static_cast<std::streamsize>(Bytecode::FILE_HEADER_SIZE));
    output_stream.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    output_stream.put(static_cast<char>(0xFF));
    REQUIRE(output_stream);

    const auto loaded = Bytecode::read_file(kTestPath);
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(loaded.error() == VMError::InvalidBytecode);
}

TEST_CASE("BytecodeFile - rejects missing file", "[BytecodeFile]") {
    const auto loaded = Bytecode::read_file("nonexistent_file.bcmv");
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(loaded.error() == VMError::InvalidBytecode);
}
