#pragma once
#include "AST.hpp"
#include "BytecodeFormat.hpp"
#include "Instruction.hpp"
#include "VMTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

/*
Bytecode (32 bits) Design:

[ header (4 bytes) ] [ ext word 0? ] [ ext word 1? ]
         ↑
    IP points here at fetch time

Operand kind	    Extension	        Contents
Register            none                Index already in reg_a / reg_b
Immediate           1 word (usually)    int32_t value, sign-extended to int64_t at decode
Immediate (wide)    2 words             Low 32 bits + high 32 bits when value ∉ [INT32_MIN, INT32_MAX]
LabelOffset         1 word              Absolute byte offset from code segment base


 31      28 27      24 23      20 19      16 15      12 11           0
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────────┐
│  opcode  │ op0 kind │ op1 kind │  reg_a   │  reg_b   │   reserved   │
│  4 bits  │  4 bits  │  4 bits  │  4 bits  │  4 bits  │   12 bits    │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────────┘

Jump Targets are absolute byte offset (matches MMU virtual addresses)
*/

class BytecodeEmitter {
public:
    struct EmitResult {
        std::vector<std::byte> code;
    };

    [[nodiscard]] VMResult<EmitResult> emit(std::span<const AST::ASTNode> ast) const {
        const auto labels = compute_label_offsets(ast);
        if (!labels) {
            return std::unexpected(labels.error());
        }

        EmitResult result;
        const auto status = emit_all_nodes(ast, *labels, result.code);
        if (!status) {
            return std::unexpected(status.error());
        }

        return result;
    }

private:
    static constexpr uint8_t GPR_COUNT = 16U;

    using LabelOffsetTable = std::unordered_map<std::string_view, size_t>;

    struct SourceOperandEncoding {
        Bytecode::OperandKind kind{Bytecode::OperandKind::None};
        uint8_t register_index{Bytecode::UNUSED_REG};
    };

    [[nodiscard]] static VMResult<void> validate_gpr(uint8_t index) noexcept {
        if (index >= GPR_COUNT) {
            return std::unexpected(VMError::InvalidRegister);
        }
        return {};
    }

    [[nodiscard]] static VMResult<SourceOperandEncoding> encode_source_operand(const Operand& src) {
        if (std::holds_alternative<VirtualRegister>(src.value)) {
            const uint8_t reg = std::get<VirtualRegister>(src.value).index;
            if (const auto valid = validate_gpr(reg); !valid) {
                return std::unexpected(valid.error());
            }
            return SourceOperandEncoding{Bytecode::OperandKind::Register, reg};
        }

        if (std::holds_alternative<int64_t>(src.value)) {
            if (!Bytecode::fits_int32(std::get<int64_t>(src.value))) {
                return std::unexpected(VMError::InvalidOperand);
            }
            return SourceOperandEncoding{Bytecode::OperandKind::Immediate, Bytecode::UNUSED_REG};
        }

        return std::unexpected(VMError::InvalidOperand);
    }

    [[nodiscard]] static VMResult<size_t> byte_size_from_header(uint32_t header) {
        return Bytecode::instruction_size_bytes(header);
    }

    [[nodiscard]] VMResult<LabelOffsetTable> compute_label_offsets(std::span<const AST::ASTNode> ast) const {
        LabelOffsetTable labels;
        size_t byte_offset = 0UZ;

        for (const AST::ASTNode& node : ast) {
            if (std::holds_alternative<AST::LabelDecl>(node)) {
                labels[std::get<AST::LabelDecl>(node).name] = byte_offset;
                continue;
            }

            const auto size = instruction_byte_size(node);
            if (!size) {
                return std::unexpected(size.error());
            }
            byte_offset += *size;
        }

        return labels;
    }

    [[nodiscard]] VMResult<void> emit_all_nodes(
        std::span<const AST::ASTNode> ast,
        const LabelOffsetTable& labels,
        std::vector<std::byte>& out) const
    {
        for (const AST::ASTNode& node : ast) {
            const auto status = emit_node(node, labels, out);
            if (!status) {
                return status;
            }
        }
        return {};
    }

    [[nodiscard]] VMResult<size_t> instruction_byte_size(const AST::ASTNode& node) const {
        if (std::holds_alternative<AST::LabelDecl>(node)) {
            return 0UZ;
        }

        if (std::holds_alternative<AST::MovOp>(node)) {
            const auto& ast_op = std::get<AST::MovOp>(node);
            return byte_size_dest_src(Opcode::MOV, ast_op.dest, ast_op.src);
        }
        if (std::holds_alternative<AST::AddOp>(node)) {
            const auto& ast_op = std::get<AST::AddOp>(node);
            return byte_size_dest_src(Opcode::ADD, ast_op.dest, ast_op.src);
        }
        if (std::holds_alternative<AST::SubOp>(node)) {
            const auto& ast_op = std::get<AST::SubOp>(node);
            return byte_size_dest_src(Opcode::SUB, ast_op.dest, ast_op.src);
        }
        if (std::holds_alternative<AST::PushOp>(node)) {
            return byte_size_push(std::get<AST::PushOp>(node).src);
        }
        if (std::holds_alternative<AST::PopOp>(node)) {
            if (const auto valid = validate_gpr(std::get<AST::PopOp>(node).dest.index); !valid) {
                return std::unexpected(valid.error());
            }
            return byte_size_from_header(Bytecode::header_unary_reg(Opcode::POP, std::get<AST::PopOp>(node).dest.index));
        }
        if (std::holds_alternative<AST::JmpOp>(node)) {
            return byte_size_from_header(Bytecode::header_label_branch(Opcode::JMP));
        }
        if (std::holds_alternative<AST::CallOp>(node)) {
            return byte_size_from_header(Bytecode::header_label_branch(Opcode::CALL));
        }
        if (std::holds_alternative<AST::RetOp>(node)) {
            return byte_size_from_header(Bytecode::header_nullary(Opcode::RET));
        }

        return std::unexpected(VMError::UnsupportedOpcode);
    }

    [[nodiscard]] VMResult<size_t> byte_size_dest_src(Opcode opcode, VirtualRegister dest, const Operand& src) const {
        if (const auto valid = validate_gpr(dest.index); !valid) {
            return std::unexpected(valid.error());
        }

        const auto encoded = encode_source_operand(src);
        if (!encoded) {
            return std::unexpected(encoded.error());
        }

        const uint32_t header = Bytecode::header_dest_src(
            opcode, dest.index, encoded->kind, encoded->register_index);
        return byte_size_from_header(header);
    }

    [[nodiscard]] VMResult<size_t> byte_size_push(const Operand& src) const {
        const auto encoded = encode_source_operand(src);
        if (!encoded) {
            return std::unexpected(encoded.error());
        }

        const uint32_t header = (encoded->kind == Bytecode::OperandKind::Register)
            ? Bytecode::header_unary_reg(Opcode::PUSH, encoded->register_index)
            : Bytecode::header_push_immediate();
        return byte_size_from_header(header);
    }

    [[nodiscard]] VMResult<void> emit_node(
        const AST::ASTNode& node,
        const LabelOffsetTable& labels,
        std::vector<std::byte>& out) const
    {
        if (std::holds_alternative<AST::LabelDecl>(node)) {
            return {};
        }
        if (std::holds_alternative<AST::MovOp>(node)) {
            const auto& ast_op = std::get<AST::MovOp>(node);
            return emit_dest_src(Opcode::MOV, ast_op.dest, ast_op.src, out);
        }
        if (std::holds_alternative<AST::AddOp>(node)) {
            const auto& ast_op = std::get<AST::AddOp>(node);
            return emit_dest_src(Opcode::ADD, ast_op.dest, ast_op.src, out);
        }
        if (std::holds_alternative<AST::SubOp>(node)) {
            const auto& ast_op = std::get<AST::SubOp>(node);
            return emit_dest_src(Opcode::SUB, ast_op.dest, ast_op.src, out);
        }
        if (std::holds_alternative<AST::PushOp>(node)) {
            return emit_push(std::get<AST::PushOp>(node).src, out);
        }
        if (std::holds_alternative<AST::PopOp>(node)) {
            const auto& ast_op = std::get<AST::PopOp>(node);
            if (const auto valid = validate_gpr(ast_op.dest.index); !valid) {
                return valid;
            }
            append_u32(out, Bytecode::header_unary_reg(Opcode::POP, ast_op.dest.index));
            return {};
        }
        if (std::holds_alternative<AST::JmpOp>(node)) {
            return emit_label_branch(Opcode::JMP, std::get<AST::JmpOp>(node).target, labels, out);
        }
        if (std::holds_alternative<AST::CallOp>(node)) {
            return emit_label_branch(Opcode::CALL, std::get<AST::CallOp>(node).target, labels, out);
        }
        if (std::holds_alternative<AST::RetOp>(node)) {
            append_u32(out, Bytecode::header_nullary(Opcode::RET));
            return {};
        }

        return std::unexpected(VMError::UnsupportedOpcode);
    }

    static void append_u32(std::vector<std::byte>& out, uint32_t word) {
        const size_t old_size = out.size();
        out.resize(old_size + Bytecode::WORD_SIZE);
        Bytecode::write_u32_le(out.data() + old_size, word);
    }

    [[nodiscard]] VMResult<void> emit_dest_src(
        Opcode opcode,
        VirtualRegister dest,
        const Operand& src,
        std::vector<std::byte>& out) const
    {
        if (const auto valid = validate_gpr(dest.index); !valid) {
            return valid;
        }

        const auto encoded = encode_source_operand(src);
        if (!encoded) {
            return std::unexpected(encoded.error());
        }

        append_u32(out, Bytecode::header_dest_src(opcode, dest.index, encoded->kind, encoded->register_index));

        if (encoded->kind == Bytecode::OperandKind::Immediate) {
            append_u32(out, Bytecode::encode_immediate_word(std::get<int64_t>(src.value)));
        }

        return {};
    }

    [[nodiscard]] VMResult<void> emit_push(const Operand& src, std::vector<std::byte>& out) const {
        const auto encoded = encode_source_operand(src);
        if (!encoded) {
            return std::unexpected(encoded.error());
        }

        if (encoded->kind == Bytecode::OperandKind::Register) {
            append_u32(out, Bytecode::header_unary_reg(Opcode::PUSH, encoded->register_index));
            return {};
        }

        append_u32(out, Bytecode::header_push_immediate());
        append_u32(out, Bytecode::encode_immediate_word(std::get<int64_t>(src.value)));
        return {};
    }

    [[nodiscard]] VMResult<void> emit_label_branch(
        Opcode opcode,
        std::string_view target,
        const LabelOffsetTable& labels,
        std::vector<std::byte>& out) const
    {
        const auto offset = resolve_label_byte_offset(labels, target);
        if (!offset) {
            return std::unexpected(offset.error());
        }

        append_u32(out, Bytecode::header_label_branch(opcode));
        append_u32(out, Bytecode::encode_label_offset(*offset));
        return {};
    }

    [[nodiscard]] static VMResult<size_t> resolve_label_byte_offset(
        const LabelOffsetTable& labels,
        std::string_view name)
    {
        const auto label_entry = labels.find(name);
        if (label_entry == labels.end()) {
            return std::unexpected(VMError::UnknownLabel);
        }
        return label_entry->second;
    }
};
