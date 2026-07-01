#pragma once
#include "Instruction.hpp"
#include <cstddef>
#include <cstdint>

/*
Bytecode word layout (little-endian):

[ header (4 bytes) ] [ ext word 0? ] [ ext word 1? ]
         ↑
    IP points here at fetch time

 31      28 27      24 23      20 19      16 15      12 11           0
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────────┐
│  opcode  │ op0 kind │ op1 kind │  reg_a   │  reg_b   │   reserved   │
│  4 bits  │  4 bits  │  4 bits  │  4 bits  │  4 bits  │   12 bits    │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────────┘

Jump targets are absolute byte offsets from the code segment base.
*/

namespace Bytecode {

enum class OperandKind : uint8_t {
    None = 0,
    Register = 1,
    Immediate = 2,
    LabelOffset = 3
};

inline constexpr uint32_t WORD_SIZE = 4U;
inline constexpr uint8_t UNUSED_REG = 0xFU;

inline constexpr uint32_t OPCODE_SHIFT = 28U;
inline constexpr uint32_t OP0_KIND_SHIFT = 24U;
inline constexpr uint32_t OP1_KIND_SHIFT = 20U;
inline constexpr uint32_t REG_A_SHIFT = 16U;
inline constexpr uint32_t REG_B_SHIFT = 12U;

inline constexpr uint32_t OPCODE_MASK = 0xFU;
inline constexpr uint32_t OP_KIND_MASK = 0xFU;
inline constexpr uint32_t REG_MASK = 0xFU;

inline constexpr uint32_t FILE_MAGIC = 0x42434D56U;  // "BCMV"
inline constexpr uint32_t FILE_VERSION = 1U;

struct DecodedInstruction {
    Opcode opcode{};
    OperandKind op0_kind{OperandKind::None};
    OperandKind op1_kind{OperandKind::None};
    uint8_t reg_a{UNUSED_REG};
    uint8_t reg_b{UNUSED_REG};
    int64_t ext0{0};
    int64_t ext1{0};
    uint32_t size_bytes{WORD_SIZE};
};

[[nodiscard]] constexpr uint32_t pack_header(
    Opcode opcode,
    OperandKind op0,
    OperandKind op1,
    uint8_t reg_a = UNUSED_REG,
    uint8_t reg_b = UNUSED_REG) noexcept
{
    return (static_cast<uint32_t>(opcode) << OPCODE_SHIFT)
         | (static_cast<uint32_t>(op0) << OP0_KIND_SHIFT)
         | (static_cast<uint32_t>(op1) << OP1_KIND_SHIFT)
         | (static_cast<uint32_t>(reg_a) << REG_A_SHIFT)
         | (static_cast<uint32_t>(reg_b) << REG_B_SHIFT);
}

[[nodiscard]] constexpr Opcode decode_opcode(uint32_t header) noexcept {
    return static_cast<Opcode>((header >> OPCODE_SHIFT) & OPCODE_MASK);
}

[[nodiscard]] constexpr OperandKind decode_op0_kind(uint32_t header) noexcept {
    return static_cast<OperandKind>((header >> OP0_KIND_SHIFT) & OP_KIND_MASK);
}

[[nodiscard]] constexpr OperandKind decode_op1_kind(uint32_t header) noexcept {
    return static_cast<OperandKind>((header >> OP1_KIND_SHIFT) & OP_KIND_MASK);
}

[[nodiscard]] constexpr uint8_t decode_reg_a(uint32_t header) noexcept {
    return static_cast<uint8_t>((header >> REG_A_SHIFT) & REG_MASK);
}

[[nodiscard]] constexpr uint8_t decode_reg_b(uint32_t header) noexcept {
    return static_cast<uint8_t>((header >> REG_B_SHIFT) & REG_MASK);
}

[[nodiscard]] constexpr uint32_t extension_word_count(OperandKind kind) noexcept {
    switch (kind) {
        case OperandKind::None:
        case OperandKind::Register:
            return 0U;
        case OperandKind::Immediate:
        case OperandKind::LabelOffset:
            return 1U;
    }
    return 0U;
}

[[nodiscard]] constexpr uint32_t instruction_size_bytes(uint32_t header) noexcept {
    const uint32_t extension_words =
        extension_word_count(decode_op0_kind(header))
      + extension_word_count(decode_op1_kind(header));
    return WORD_SIZE + (extension_words * WORD_SIZE);
}

[[nodiscard]] constexpr int64_t sign_extend_immediate(uint32_t raw) noexcept {
    return static_cast<int64_t>(static_cast<int32_t>(raw));
}

[[nodiscard]] constexpr bool fits_int32(int64_t value) noexcept {
    return value >= INT32_MIN && value <= INT32_MAX;
}

[[nodiscard]] constexpr uint32_t encode_immediate_word(int64_t value) noexcept {
    return static_cast<uint32_t>(static_cast<int32_t>(value));
}

[[nodiscard]] constexpr uint32_t encode_label_offset(size_t byte_offset) noexcept {
    return static_cast<uint32_t>(byte_offset);
}

[[nodiscard]] constexpr uint32_t read_u32_le(const std::byte* ptr) noexcept {
    return static_cast<uint32_t>(ptr[0])
         | (static_cast<uint32_t>(ptr[1]) << 8U)
         | (static_cast<uint32_t>(ptr[2]) << 16U)
         | (static_cast<uint32_t>(ptr[3]) << 24U);
}

constexpr void write_u32_le(std::byte* ptr, uint32_t value) noexcept {
    ptr[0] = static_cast<std::byte>(value & 0xFFU);
    ptr[1] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    ptr[2] = static_cast<std::byte>((value >> 16U) & 0xFFU);
    ptr[3] = static_cast<std::byte>((value >> 24U) & 0xFFU);
}

[[nodiscard]] constexpr bool decode_at(
    const std::byte* base,
    size_t byte_offset,
    size_t buffer_size,
    DecodedInstruction& out) noexcept
{
    if (byte_offset + WORD_SIZE > buffer_size) {
        return false;
    }

    const uint32_t header = read_u32_le(base + byte_offset);
    out.opcode = decode_opcode(header);
    out.op0_kind = decode_op0_kind(header);
    out.op1_kind = decode_op1_kind(header);
    out.reg_a = decode_reg_a(header);
    out.reg_b = decode_reg_b(header);
    out.size_bytes = instruction_size_bytes(header);

    if (byte_offset + out.size_bytes > buffer_size) {
        return false;
    }

    size_t cursor = byte_offset + WORD_SIZE;

    if (extension_word_count(out.op0_kind) == 1U) {
        const uint32_t raw = read_u32_le(base + cursor);
        cursor += WORD_SIZE;
        out.ext0 = (out.op0_kind == OperandKind::Immediate)
            ? sign_extend_immediate(raw)
            : static_cast<int64_t>(raw);
    }

    if (extension_word_count(out.op1_kind) == 1U) {
        const uint32_t raw = read_u32_le(base + cursor);
        out.ext1 = (out.op1_kind == OperandKind::Immediate)
            ? sign_extend_immediate(raw)
            : static_cast<int64_t>(raw);
    }

    return true;
}

[[nodiscard]] constexpr uint32_t header_dest_src(
    Opcode opcode,
    uint8_t dest,
    OperandKind src_kind,
    uint8_t src_reg = UNUSED_REG) noexcept
{
    return pack_header(opcode, OperandKind::Register, src_kind, dest, src_reg);
}

[[nodiscard]] constexpr uint32_t header_unary_reg(Opcode opcode, uint8_t reg) noexcept {
    return pack_header(opcode, OperandKind::Register, OperandKind::None, reg, UNUSED_REG);
}

[[nodiscard]] constexpr uint32_t header_push_immediate() noexcept {
    return pack_header(Opcode::PUSH, OperandKind::Immediate, OperandKind::None);
}

[[nodiscard]] constexpr uint32_t header_nullary(Opcode opcode) noexcept {
    return pack_header(opcode, OperandKind::None, OperandKind::None);
}

[[nodiscard]] constexpr uint32_t header_label_branch(Opcode opcode) noexcept {
    return pack_header(opcode, OperandKind::LabelOffset, OperandKind::None);
}

static_assert(pack_header(Opcode::MOV, OperandKind::Register, OperandKind::Immediate, 1, UNUSED_REG) == 0x0121F000U);

}  // namespace Bytecode
