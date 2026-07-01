#pragma once
#include "Instruction.hpp"
#include "VMTypes.hpp"
#include "BytecodeFormat.hpp"
#include "BytecodeFile.hpp"
#include "VirtualMemory.hpp"
#include "LinearArena.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>


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
        explicit VirtualMachine(MemoryEngine::MemoryManagementUnit& mmu, MemoryEngine::LinearArena& arena) noexcept
                                : m_mmu(&mmu), m_arena_base(static_cast<std::byte*>(arena.current_allocations().data())){}

        VMResult<void> load_program(std::span<const std::byte> code) {
            if (code.empty()) {
                m_code_size = 0;
                reset();
                return {};
            }

            size_t offset = 0;
            while (offset < code.size()) {
                Bytecode::DecodedInstruction decoded{};
                if (!Bytecode::decode_at(code.data(), offset, code.size(), decoded)) {
                    return std::unexpected(VMError::InvalidBytecode); 
                }
                offset += decoded.size_bytes;
            }
            if (!validate_branch_targets(code)) {
                return std::unexpected(VMError::InvalidBytecode);
            }

            auto loaded = load_into_guest_memory(code);
            if (!loaded) return loaded;

            reset();
            return {}; 
        }

        VMResult<void> load_program_from_file(const char* path) {
            auto code = Bytecode::read_file(path);
            if (!code) {
                return std::unexpected(code.error());
            }
            return load_program(std::span<const std::byte>{*code});
        }

        VMResult<void> run() {
            if (m_code_size == 0) {
                return {};
            }

            m_running = true;
            m_cpu.ip = 0UZ;

            while (m_running) {
                if (m_cpu.ip >= m_code_size) {
                    m_running = false;
                    break;
                }

                auto decoded = fetch_instruction(m_cpu.ip);
                if (!decoded) {
                    m_running = false;
                    return std::unexpected(decoded.error());
                }

                auto result = step(*decoded);
                if (!result) {
                    m_running = false;
                    return result;
                }
            }

            return {};
        }

        void reset() noexcept {
            m_cpu.reset();
            m_stack.clear();
            m_running = false;
        }

        [[nodiscard]] int64_t read_register(uint8_t index) const {
            return m_cpu.read_gpr(index);
        }

        [[nodiscard]] size_t instruction_pointer() const noexcept { return m_cpu.ip; }
        [[nodiscard]] size_t stack_pointer() const noexcept { return m_cpu.sp; }
        [[nodiscard]] bool is_running() const noexcept { return m_running; }
        [[nodiscard]] size_t program_size() const noexcept { return m_code_size; }

    private:
        RegisterFile m_cpu;
        bool m_running{false};

        std::vector<int64_t> m_stack;

        static constexpr uint32_t CODE_BASE_VA = 0x00400000U;
        MemoryEngine::MemoryManagementUnit* m_mmu{nullptr};
        std::byte* m_arena_base{nullptr};
        size_t m_code_size{0};



        [[nodiscard]] static constexpr bool modifies_ip(Opcode opcode) noexcept {
            return opcode == Opcode::JMP || opcode == Opcode::CALL || opcode == Opcode::RET;
        }     



        [[nodiscard]] VMResult<uint8_t> validate_reg(uint8_t index) const noexcept {
            if (index >= RegisterFile::GPR_COUNT || index == Bytecode::UNUSED_REG) {
                return std::unexpected(VMError::InvalidRegister);
            }
            
            return index;
        }

        [[nodiscard]] VMResult<void> validate_branch_targets(std::span<const std::byte> code) const {
            size_t offset = 0;
            while (offset < code.size()) {
                Bytecode::DecodedInstruction decoded{};
                if (!Bytecode::decode_at(code.data(), offset, code.size(), decoded)){
                    return std::unexpected(VMError::InvalidBytecode); 
                }

                if (decoded.opcode == Opcode::JMP || decoded.opcode == Opcode::CALL) {
                    const size_t target = static_cast<size_t>(decoded.ext0);
                    if (target >= code.size()) {
                        return std::unexpected(VMError::InvalidBytecode);
                    }

                    Bytecode::DecodedInstruction at_target{}; 
                    if (!Bytecode::decode_at(code.data(), target, code.size(), at_target)) {
                        return std::unexpected(VMError::InvalidBytecode);
                    }
                }

                offset += decoded.size_bytes;
            }
            return {}; 
        }



        [[nodiscard]] VMResult<int64_t> read_source_operand(const Bytecode::DecodedInstruction& inst) const {
            if (inst.op1_kind == Bytecode::OperandKind::Register) {
                auto idx = validate_reg(inst.reg_b);
                if (!idx) return std::unexpected(idx.error());
                return m_cpu.read_gpr(*idx);
            }
            if (inst.op1_kind == Bytecode::OperandKind::Immediate) {
                return inst.ext1;
            }
            return std::unexpected(VMError::InvalidOperand);
        }

        [[nodiscard]] VMResult<int64_t> read_push_operand(
            const Bytecode::DecodedInstruction& inst) const
        {
            if (inst.op0_kind == Bytecode::OperandKind::Register) {
                auto idx = validate_reg(inst.reg_a);
                if (!idx) return std::unexpected(idx.error());
                return m_cpu.read_gpr(*idx);
            }
            if (inst.op0_kind == Bytecode::OperandKind::Immediate) {
                return inst.ext0;
            }
            return std::unexpected(VMError::InvalidOperand);
        }



        void push_stack(int64_t value) {
            m_stack.push_back(value);
            m_cpu.sp = m_stack.size();
        }

        [[nodiscard]] VMResult<int64_t> pop_stack() {
            if (m_stack.empty()) {
                return std::unexpected(VMError::StackUnderflow);
            }
            int64_t value = m_stack.back();
            m_stack.pop_back();
            m_cpu.sp = m_stack.size();
            return value;
        }


        [[nodiscard]] VMResult<std::byte> read_guest_byte(size_t code_offset) const {
            const uint32_t virt_addr = CODE_BASE_VA + static_cast<uint32_t>(code_offset);
            const auto phys = m_mmu->translate(virt_addr, false);
            if (!phys) {
                return std::unexpected(VMError::InvalidBytecode);
            }
            return m_arena_base[*phys];
        }

        [[nodiscard]] VMResult<void> write_guest_byte(size_t code_offset, std::byte value) {
            const uint32_t virt_addr = CODE_BASE_VA + static_cast<uint32_t>(code_offset);
            const auto phys = m_mmu->translate(virt_addr, true);  // require write during load
            if (!phys) {
                return std::unexpected(VMError::InvalidBytecode);
            }
            m_arena_base[*phys] = value;
            return {};
        }

        [[nodiscard]] VMResult<Bytecode::DecodedInstruction> fetch_instruction(size_t code_offset) const {
            if (code_offset >= m_code_size) {
                return std::unexpected(VMError::InvalidBytecode);
            }
            
            // the max instruction size is 8 bytes (header + one ext word), no wide types yet for ext
            std::byte local_buf[8];
            const size_t remaining = m_code_size - code_offset;
            const size_t window = remaining < sizeof(local_buf) ? remaining : sizeof(local_buf);
            for (size_t byte_idx = 0; byte_idx < window; ++byte_idx) {
                auto byte = read_guest_byte(code_offset + byte_idx);
                if (!byte) {
                    return std::unexpected(byte.error());
                }
                local_buf[byte_idx] = *byte;
            }
            Bytecode::DecodedInstruction decoded{};
            if (!Bytecode::decode_at(local_buf, 0UZ, window, decoded)) {
                return std::unexpected(VMError::InvalidBytecode);
            }

            if (code_offset + decoded.size_bytes > m_code_size) {
                return std::unexpected(VMError::InvalidBytecode);
            }

            return decoded;
        }

        [[nodiscard]] VMResult<void> map_code_page(uint32_t page_va, bool readable, bool writable) {
            const auto status = m_mmu->map_page(page_va, readable, writable);
            if (!status) return std::unexpected(VMError::InvalidBytecode);
            return {};
        }

        
        [[nodiscard]] VMResult<void> load_into_guest_memory(std::span<const std::byte> code) {
            const uint32_t page_size = MemoryEngine::MemoryManagementUnit::PAGE_SIZE;
            const size_t page_count =
                (code.size() + page_size - 1UZ) / page_size;
            
            // map all code pages RW
            for (size_t page = 0; page < page_count; ++page) {
                const uint32_t page_va = CODE_BASE_VA + static_cast<uint32_t>(page * page_size);
                if (auto status = map_code_page(page_va, true, true); !status) {
                    return status;
                }
            }
            
            // write bytecode
            for (size_t byte_idx = 0; byte_idx < code.size(); ++byte_idx) {
                if (auto status = write_guest_byte(byte_idx, code[byte_idx]); !status) {
                    return status;
                }
            }
            
            // W^X — flip to RX (readable, not writable)
            for (size_t page = 0; page < page_count; ++page) {
                const uint32_t page_va =
                    CODE_BASE_VA + static_cast<uint32_t>(page * page_size);
                if (auto status = map_code_page(page_va, true, false); !status) {
                    return status;
                }
            }
            
            m_code_size = code.size();
            return {};
        }




        VMResult<void> step(const Bytecode::DecodedInstruction& inst) {
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
                case Opcode::PUSH:
                    step_result = exec_push(inst);
                    break;
                case Opcode::POP:
                    step_result = exec_pop(inst);
                    break;
                case Opcode::CALL:
                    step_result = exec_call(inst);
                    break;
                case Opcode::RET:
                    step_result = exec_ret(inst);
                    break;
                default:
                    step_result = std::unexpected(VMError::UnsupportedOpcode);
                    break;
            }

            if (!step_result) return step_result;
            if (!modifies_ip(inst.opcode)) m_cpu.ip += inst.size_bytes; 

            return {};
        }

        template <typename ApplyFn>
        VMResult<void> apply_dest_src(const Bytecode::DecodedInstruction& inst, ApplyFn&& apply) {
            auto dest = validate_reg(inst.reg_a); 
            if (!dest) return std::unexpected(dest.error());
    
            auto src = read_source_operand(inst);
            if (!src) return std::unexpected(src.error());
            
            apply(m_cpu.gpr[*dest], *src);
            return {};
        }

        VMResult<void> exec_mov(const Bytecode::DecodedInstruction& inst) {
            return apply_dest_src(inst, [](int64_t& dest, int64_t src) { dest = src; });
        }

        VMResult<void> exec_add(const Bytecode::DecodedInstruction& inst) {
            return apply_dest_src(inst, [](int64_t& dest, int64_t src) { dest += src; });
        }

        VMResult<void> exec_sub(const Bytecode::DecodedInstruction& inst) {
            return apply_dest_src(inst, [](int64_t& dest, int64_t src) { dest -= src; });
        }

        VMResult<void> exec_push(const Bytecode::DecodedInstruction& inst) {
            auto value = read_push_operand(inst);
            if (!value) return std::unexpected(value.error());
            push_stack(*value);
            return {};
        }

        VMResult<void> exec_pop(const Bytecode::DecodedInstruction& inst) {
            auto dest = validate_reg(inst.reg_a);
            if (!dest) return std::unexpected(dest.error());
            
            auto value = pop_stack();
            if (!value) return std::unexpected(value.error());
            m_cpu.gpr[*dest] = *value;
            return {};
        }

        VMResult<void> exec_jmp(const Bytecode::DecodedInstruction& inst) {
            m_cpu.ip = static_cast<size_t>(inst.ext0);
            return {};
        }

        VMResult<void> exec_call(const Bytecode::DecodedInstruction& inst) {
            const size_t return_ip = m_cpu.ip + inst.size_bytes;
            push_stack(static_cast<int64_t>(return_ip));
            m_cpu.ip = static_cast<size_t>(inst.ext0);
            return {};
        }

        VMResult<void> exec_ret(const Bytecode::DecodedInstruction& /*inst*/) {
            auto return_addr = pop_stack();
            if (!return_addr) return std::unexpected(return_addr.error());
            m_cpu.ip = static_cast<size_t>(*return_addr);
            return {};
        }
};
