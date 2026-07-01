#pragma once
#include "BytecodeFormat.hpp"
#include "VMTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <span>
#include <vector> 


/*
File Layout: 

Offset  Size  Field
------  ----  -----
0       4     magic   = 0x42434D56 ("BCMV")
4       4     version = 1
8       4     code_size (bytes)
12      N     raw bytecode payload (exactly what BytecodeEmitter produces)

*/

namespace Bytecode {
    inline constexpr uint32_t FILE_HEADER_SIZE = 12U;

    [[nodiscard]] inline VMResult<void> write_file(const char* path, std::span<const std::byte> code) {
        
        std::ofstream output_stream(path, std::ios::binary | std::ios::trunc);
        if (!output_stream) return std::unexpected(VMError::InvalidBytecode);

        std::byte header[FILE_HEADER_SIZE];
    
        write_u32_le(header + 0, FILE_MAGIC);
        write_u32_le(header + 4, FILE_VERSION);
        write_u32_le(header + 8, static_cast<uint32_t>(code.size()));
        
        output_stream.write(reinterpret_cast<const char*>(header), static_cast<std::streamsize>(FILE_HEADER_SIZE));
        if (!output_stream) return std::unexpected(VMError::InvalidBytecode);

        if (!code.empty()) {
            output_stream.write(reinterpret_cast<const char*>(code.data()),
                      static_cast<std::streamsize>(code.size()));
            if (!output_stream) {
                return std::unexpected(VMError::InvalidBytecode);
            }
        }

        return {};
    }


    [[nodiscard]] inline VMResult<std::vector<std::byte>> read_file(const char* path) {
        std::ifstream input_stream(path, std::ios::binary);
        if (!input_stream) return std::unexpected(VMError::InvalidBytecode);

        auto read_u32 = [&input_stream]() -> VMResult<uint32_t> {
            std::byte buf[4];
            input_stream.read(reinterpret_cast<char*>(buf), 4);
            if (!input_stream) {
                return std::unexpected(VMError::InvalidBytecode);
            }
            return read_u32_le(buf);
        };

        const auto magic = read_u32();
        if (!magic || *magic != FILE_MAGIC) return std::unexpected(VMError::InvalidBytecode);

        const auto version = read_u32();
        if (!version || *version != FILE_VERSION) return std::unexpected(VMError::InvalidBytecode);

        const auto code_size = read_u32();
        if (!code_size) {
            return std::unexpected(VMError::InvalidBytecode);
        }


        std::vector<std::byte> code(static_cast<size_t>(*code_size));
        if (*code_size > 0U) {
            input_stream.read(reinterpret_cast<char*>(code.data()),
                    static_cast<std::streamsize>(*code_size));
            if (!input_stream || static_cast<uint32_t>(input_stream.gcount()) != *code_size) {
                return std::unexpected(VMError::InvalidBytecode);
            }
        }

        if (input_stream.peek() != std::ifstream::traits_type::eof()) {
            return std::unexpected(VMError::InvalidBytecode);
        }
        return code;
    }

}