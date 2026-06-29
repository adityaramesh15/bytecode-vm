#pragma once
#include "Instruction.hpp"
#include "AST.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <unordered_map>
#include <variant>
#include <vector>

enum class VMError : uint8_t {
    InvalidRegister,
    InvalidOperand,
    UnknownLabel,
    UnsupportedOpcode,
    ProgramEnded
};

template <typename T>
using VMResult = std::expected<T, VMError>;

class VirtualMachine {
    public:
        static constexpr size_t REGISTER_COUNT = 16UZ;
        VirtualMachine() = default;

        VMResult<void> load_program(std::span<const AST::ASTNode> ast) {
            m_program.clear();
            m_labels.clear();
            reset();

            for (const AST::ASTNode& node : ast) {
                if (std::holds_alternative<AST::LabelDecl>(node)) {
                    const auto& label = std::get<AST::LabelDecl>(node);
                    m_labels[label.name] = m_program.size();
                    continue;
                }

                auto lowered = lower_node(node);
                if (!lowered) {
                    return std::unexpected(lowered.error());
                }
                m_program.push_back(*lowered);
            }

            return {};
        }

        VMResult<void> run() {
            if (m_program.empty()) {
                return {};
            }

            m_running = true;
            m_ip = 0UZ;

            while (m_running) {
                if (m_ip >= m_program.size()) {
                    m_running = false;
                    break;
                }

                const Instruction& inst = m_program[m_ip];

                VMResult<void> step_result = std::unexpected(VMError::UnsupportedOpcode);

                switch (inst.opcode) {
                    case Opcode::MOV:
                        step_result = exec_mov(inst);
                        break;
                    case Opcode::ADD:
                        step_result = exec_add(inst);
                        break;
                    case Opcode::SUB:
                        step_result = exec_sub(inst);
                        break;
                    case Opcode::JMP:
                        step_result = exec_jmp(inst);
                        break;
                    default:
                        step_result = std::unexpected(VMError::UnsupportedOpcode);
                        break;
                }

                if (!step_result) {
                    m_running = false;
                    return step_result;
                }

                if (inst.opcode != Opcode::JMP) {
                    ++m_ip;
                }
            }

            return {};
        }

        void reset() noexcept {
            m_regs.fill(0);
            m_ip = 0UZ;
            m_running = false;
        }

        [[nodiscard]] int64_t read_register(uint8_t index) const {
            return m_regs.at(index);
        }

        [[nodiscard]] size_t instruction_pointer() const noexcept { return m_ip; }
        [[nodiscard]] bool is_running() const noexcept { return m_running; }
        [[nodiscard]] size_t program_size() const noexcept { return m_program.size(); }

    private:
        std::array<int64_t, REGISTER_COUNT> m_regs{};
        size_t m_ip{0UZ};
        bool m_running{false};

        std::vector<Instruction> m_program;
        std::unordered_map<std::string_view, size_t> m_labels;

        [[nodiscard]] VMResult<uint8_t> validate_register(VirtualRegister reg) const noexcept {
            if (reg.index >= REGISTER_COUNT) {
                return std::unexpected(VMError::InvalidRegister);
            }
            return reg.index;
        }

        [[nodiscard]] VMResult<uint8_t> extract_register_operand(const Operand& op) const noexcept {
            if (!std::holds_alternative<VirtualRegister>(op.value)) {
                return std::unexpected(VMError::InvalidOperand);
            }
            return validate_register(std::get<VirtualRegister>(op.value));
        }

        [[nodiscard]] VMResult<std::string_view> extract_label_operand(const Operand& op) const noexcept {
            if (!std::holds_alternative<std::string_view>(op.value)) {
                return std::unexpected(VMError::InvalidOperand);
            }
            return std::get<std::string_view>(op.value);
        }

        [[nodiscard]] VMResult<Instruction> lower_node(const AST::ASTNode& node) const {
            if (std::holds_alternative<AST::MovOp>(node)) {
                const auto& mov_op = std::get<AST::MovOp>(node);
                Instruction instr{.opcode = Opcode::MOV, .operand_count = 2};
                instr.operands[0].value = mov_op.dest;
                instr.operands[1] = mov_op.src;
                return instr;
            }
            if (std::holds_alternative<AST::AddOp>(node)) {
                const auto& add_op = std::get<AST::AddOp>(node);
                Instruction instr{.opcode = Opcode::ADD, .operand_count = 2};
                instr.operands[0].value = add_op.dest;
                instr.operands[1] = add_op.src;
                return instr;
            }
            if (std::holds_alternative<AST::SubOp>(node)) {
                const auto& sub_op = std::get<AST::SubOp>(node);
                Instruction instr{.opcode = Opcode::SUB, .operand_count = 2};
                instr.operands[0].value = sub_op.dest;
                instr.operands[1] = sub_op.src;
                return instr;
            }
            if (std::holds_alternative<AST::JmpOp>(node)) {
                const auto& jmp_op = std::get<AST::JmpOp>(node);
                Instruction instr{.opcode = Opcode::JMP, .operand_count = 1};
                instr.operands[0].value = jmp_op.target;
                return instr;
            }
            return std::unexpected(VMError::UnsupportedOpcode);
        }

        [[nodiscard]] VMResult<int64_t> resolve_operand(const Operand& op) const {
            return std::visit([this](const auto& value) -> VMResult<int64_t> {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, VirtualRegister>) {
                    auto idx = validate_register(value);
                    if (!idx) {
                        return std::unexpected(idx.error());
                    }
                    return m_regs[*idx];
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    return value;
                } else {
                    return std::unexpected(VMError::InvalidOperand);
                }
            }, op.value);
        }

        VMResult<void> exec_mov(const Instruction& inst) {
            auto dest_idx = extract_register_operand(inst.operands[0]);
            if (!dest_idx) {
                return std::unexpected(dest_idx.error());
            }
            auto src_val = resolve_operand(inst.operands[1]);
            if (!src_val) {
                return std::unexpected(src_val.error());
            }
            m_regs[*dest_idx] = *src_val;
            return {};
        }

        VMResult<void> exec_add(const Instruction& inst) {
            auto dest_idx = extract_register_operand(inst.operands[0]);
            if (!dest_idx) {
                return std::unexpected(dest_idx.error());
            }
            auto src_val = resolve_operand(inst.operands[1]);
            if (!src_val) {
                return std::unexpected(src_val.error());
            }
            m_regs[*dest_idx] += *src_val;
            return {};
        }

        VMResult<void> exec_sub(const Instruction& inst) {
            auto dest_idx = extract_register_operand(inst.operands[0]);
            if (!dest_idx) {
                return std::unexpected(dest_idx.error());
            }
            auto src_val = resolve_operand(inst.operands[1]);
            if (!src_val) {
                return std::unexpected(src_val.error());
            }
            m_regs[*dest_idx] -= *src_val;
            return {};
        }

        VMResult<void> exec_jmp(const Instruction& inst) {
            auto target = extract_label_operand(inst.operands[0]);
            if (!target) {
                return std::unexpected(target.error());
            }
            auto it = m_labels.find(*target);
            if (it == m_labels.end()) {
                return std::unexpected(VMError::UnknownLabel);
            }
            m_ip = it->second;
            return {};
        }
};
