#include <iostream>
#include <string_view>
#include <vector>
#include "Lexer.hpp"
#include "AST.hpp"
#include "Parser.hpp"
#include "LinearArena.hpp"
#include "ArenaAllocator.hpp"
#include "BytecodeEmitter.hpp"
#include "BytecodeFile.hpp"
#include "VirtualMachine.hpp"
#include "VirtualMemory.hpp"

namespace {

void print_vm_error(VMError err) {
    switch (err) {
        case VMError::InvalidRegister:    std::cerr << "InvalidRegister\n"; break;
        case VMError::InvalidOperand:     std::cerr << "InvalidOperand\n"; break;
        case VMError::UnknownLabel:       std::cerr << "UnknownLabel\n"; break;
        case VMError::UnsupportedOpcode:  std::cerr << "UnsupportedOpcode\n"; break;
        case VMError::StackUnderflow:     std::cerr << "StackUnderflow\n"; break;
        case VMError::InvalidBytecode:    std::cerr << "InvalidBytecode\n"; break;
        default:                          std::cerr << "Unknown VM error\n"; break;
    }
}

}

auto main() -> int {
    try {
        constexpr std::string_view source_code =
            "MOV R1, 42\n"
            "ADD R2, R1\n"
            "PUSH R2\n"
            "CALL add_one\n"
            "POP R3\n"
            "JMP done\n"
            "add_one:\n"
            "ADD R2, R1\n"
            "RET\n"
            "done:\n"
            "MOV R0, 0\n";

        std::cout << "Memory Engine Initializing (64MB Linear Region)...\n";
        constexpr size_t TOTAL_ARENA_CAPACITY = 64UZ * 1024UZ * 1024UZ;

        MemoryEngine::LinearArena arena(TOTAL_ARENA_CAPACITY);
        MemoryEngine::MemoryManagementUnit mmu(arena);

        MemoryEngine::ArenaAllocator<Token> token_allocator(arena);
        MemoryEngine::ArenaAllocator<AST::ASTNode> ast_allocator(arena);

        // --- Frontend: assembly -> AST ---
        std::cout << "Frontend: lex + parse...\n";
        Lexer lexer(source_code);
        auto tokens = lexer.lex_input(token_allocator);

        Parser<MemoryEngine::ArenaAllocator<Token>> parser(std::move(tokens), arena);
        auto parse_result = parser.parse_program(ast_allocator);
        if (!parse_result) {
            const SyntaxError& err = parse_result.error();
            std::cerr << "Compilation error: " << err.message
                      << " (line " << err.line << ", col " << err.column << ")\n";
            return 1;
        }
        const auto& program_ast = *parse_result;
        std::cout << "  -> Parsed " << program_ast.size() << " AST nodes\n";

        // --- Compiler backend: AST -> bytecode ---
        std::cout << "Backend: emit bytecode...\n";
        BytecodeEmitter emitter;
        auto emit_result = emitter.emit(std::span{program_ast.data(), program_ast.size()});
        if (!emit_result) {
            std::cerr << "Emit failed: ";
            print_vm_error(emit_result.error());
            return 1;
        }
        std::cout << "  -> Emitted " << emit_result->code.size() << " bytes\n";

        // --- Persist .bcmv artifact ---
        constexpr const char* kBytecodePath = "program.bcmv";
        if (auto write_status = Bytecode::write_file(kBytecodePath, emit_result->code); !write_status) {
            std::cerr << "Failed to write " << kBytecodePath << "\n";
            return 1;
        }
        std::cout << "  -> Wrote " << kBytecodePath << "\n";

        // --- Runtime: load into guest MMU memory and execute ---
        std::cout << "Runtime: load + execute...\n";
        VirtualMachine virtual_machine(mmu, arena);

        if (auto load_status = virtual_machine.load_program_from_file(kBytecodePath); !load_status) {
            std::cerr << "Load failed: ";
            print_vm_error(load_status.error());
            return 1;
        }

        if (auto run_status = virtual_machine.run(); !run_status) {
            std::cerr << "Execution failed: ";
            print_vm_error(run_status.error());
            return 1;
        }

        // --- Results ---
        std::cout << "\nExecution complete.\n";
        std::cout << "  R1 = " << virtual_machine.read_register(1) << "\n";
        std::cout << "  R2 = " << virtual_machine.read_register(2) << "\n";
        std::cout << "  R3 = " << virtual_machine.read_register(3) << "\n";
        std::cout << "  Final IP = " << virtual_machine.instruction_pointer() << "\n";
        std::cout << "  Program size = " << virtual_machine.program_size() << " bytes\n";

        return 0;

    } catch (const std::exception& exception) {
        std::cerr << "Fatal exception: " << exception.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Fatal unknown exception.\n";
        return 1;
    }
}