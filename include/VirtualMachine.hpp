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


struct RegisterFile {
    static constexpr size_t GPR_COUNT = 16UZ;

    alignas(64) std::array<int64_t, GPR_COUNT> gpr{};   // R0 through R15
    size_t ip{0};
    size_t sp{0};

    void reset() noexcept {
        gpr.fill(0);
        ip = 0UZ;
        sp = 0UZ;
    }

    [[nodiscard]] int64_t read_gpr(uint8_t index) const {
        return gpr.at(index);
    }
};

static_assert(alignof(RegisterFile) >= 64);
static_assert(sizeof(RegisterFile{}.gpr) == 128);
static_assert(sizeof(RegisterFile) <= 256);

class VirtualMachine {
    public:
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
            m_cpu.ip = 0UZ;

            while (m_running) {
                if (m_cpu.ip >= m_program.size()) {
                    m_running = false;
                    break;
                }

                auto result = step(m_program[m_cpu.ip]);
                if (!result) {
                    m_running = false;
                    return result;
                }
            }

            return {};
        }

        void reset() noexcept {
            m_cpu.reset();
            m_running = false;
        }

        [[nodiscard]] int64_t read_register(uint8_t index) const {
            return m_cpu.read_gpr(index);
        }

        [[nodiscard]] size_t instruction_pointer() const noexcept { return m_cpu.ip; }
        [[nodiscard]] size_t stack_pointer() const noexcept { return m_cpu.sp; }
        [[nodiscard]] bool is_running() const noexcept { return m_running; }
        [[nodiscard]] size_t program_size() const noexcept { return m_program.size(); }

    private:
        RegisterFile m_cpu; 
        bool m_running{false};

        std::vector<Instruction> m_program;
        std::unordered_map<std::string_view, size_t> m_labels;



        [[nodiscard]] static Instruction make_dest_src_instruction(Opcode opcode, VirtualRegister dest, const Operand& src) {
            Instruction instr{.opcode = opcode, .operands = {}, .operand_count = 2};
            instr.operands[0].value = dest;
            instr.operands[1] = src;
            return instr;
        }

        [[nodiscard]] VMResult<Instruction> lower_node(const AST::ASTNode& node) const {
            if (std::holds_alternative<AST::MovOp>(node)) {
                const auto& op = std::get<AST::MovOp>(node);
                return make_dest_src_instruction(Opcode::MOV, op.dest, op.src);
            }
            if (std::holds_alternative<AST::AddOp>(node)) {
                const auto& op = std::get<AST::AddOp>(node);
                return make_dest_src_instruction(Opcode::ADD, op.dest, op.src);
            }
            if (std::holds_alternative<AST::SubOp>(node)) {
                const auto& op = std::get<AST::SubOp>(node);
                return make_dest_src_instruction(Opcode::SUB, op.dest, op.src);
            }
            if (std::holds_alternative<AST::JmpOp>(node)) {
                const auto& jmp_op = std::get<AST::JmpOp>(node);
                Instruction instr{.opcode = Opcode::JMP, .operands = {}, .operand_count = 1};
                instr.operands[0].value = jmp_op.target;
                return instr;
            }
            return std::unexpected(VMError::UnsupportedOpcode);
        }



        [[nodiscard]] VMResult<uint8_t> register_index(VirtualRegister reg) const noexcept {
            if (reg.index >= RegisterFile::GPR_COUNT) {
                return std::unexpected(VMError::InvalidRegister);
            }
            return reg.index;
        }

        [[nodiscard]] VMResult<uint8_t> register_index(const Operand& op) const noexcept {
            if (!std::holds_alternative<VirtualRegister>(op.value)) {
                return std::unexpected(VMError::InvalidOperand);
            }
            return register_index(std::get<VirtualRegister>(op.value));
        }

        [[nodiscard]] VMResult<int64_t> operand_value(const Operand& op) const {
            return std::visit([this](const auto& value) -> VMResult<int64_t> {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, VirtualRegister>) {
                    auto idx = register_index(value);
                    if (!idx) {
                        return std::unexpected(idx.error());
                    }
                    return m_cpu.read_gpr(*idx);
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    return value;
                } else {
                    return std::unexpected(VMError::InvalidOperand);
                }
            }, op.value);
        }

        [[nodiscard]] VMResult<std::string_view> label_target(const Operand& op) const noexcept {
            if (!std::holds_alternative<std::string_view>(op.value)) {
                return std::unexpected(VMError::InvalidOperand);
            }
            return std::get<std::string_view>(op.value);
        }


        VMResult<void> step(const Instruction& inst) {
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
                return step_result;
            }

            if (inst.opcode != Opcode::JMP) {
                ++m_cpu.ip; 
            }

            return {};
        }

        template <typename ApplyFn>
        VMResult<void> apply_dest_src(const Instruction& inst, ApplyFn&& apply) {
            auto dest = register_index(inst.operands[0]);
            if (!dest) {
                return std::unexpected(dest.error());
            }
            auto src = operand_value(inst.operands[1]);
            if (!src) {
                return std::unexpected(src.error());
            }
            apply(m_cpu.gpr[*dest], *src);
            return {};
        }

        // passing in lambdas for operation behavior
        VMResult<void> exec_mov(const Instruction& inst) {
            return apply_dest_src(inst, [](int64_t& dest, int64_t src) { dest = src; });
        }

        VMResult<void> exec_add(const Instruction& inst) {
            return apply_dest_src(inst, [](int64_t& dest, int64_t src) { dest += src; });
        }

        VMResult<void> exec_sub(const Instruction& inst) {
            return apply_dest_src(inst, [](int64_t& dest, int64_t src) { dest -= src; });
        }

        VMResult<void> exec_jmp(const Instruction& inst) {
            auto target = label_target(inst.operands[0]);
            if (!target) {
                return std::unexpected(target.error());
            }
            auto it = m_labels.find(*target);
            if (it == m_labels.end()) {
                return std::unexpected(VMError::UnknownLabel);
            }
            m_cpu.ip = it->second;
            return {};
        }
};
