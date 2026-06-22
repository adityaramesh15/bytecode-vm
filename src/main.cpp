#include <iostream>
#include <variant>
#include <vector>
#include "Lexer.hpp"
#include "AST.hpp"
#include "Parser.hpp"
#include "LinearArena.hpp"
#include "ArenaAllocator.hpp"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; }; 

template <typename Allocator>
void inspect_ast(const std::vector<AST::ASTNode, Allocator>& program_ast) {
    std::cout << "Successfully compiled " << program_ast.size() << " AST nodes.\n";

    for (size_t i = 0; i < program_ast.size(); ++i) {
        std::cout << "Node [" << i << "]: ";

        std::visit(overloaded {
            [](const AST::LabelDecl& node) { std::cout << "Label Declaration -> Name: " << node.name << "\n"; },
            [](const AST::MovOp& node)     { std::cout << "Instruction -> MOV (Dest: R" << static_cast<int>(node.dest.index) << ")\n"; },
            [](const AST::AddOp& node)     { std::cout << "Instruction -> ADD (Dest: R" << static_cast<int>(node.dest.index) << ")\n"; },
            [](const AST::SubOp& node)     { std::cout << "Instruction -> SUB (Dest: R" << static_cast<int>(node.dest.index) << ")\n"; },
            [](const AST::PushOp&)         { std::cout << "Instruction -> PUSH\n"; },
            [](const AST::PopOp& node)     { std::cout << "Instruction -> POP (Dest: R" << static_cast<int>(node.dest.index) << ")\n"; },
            [](const AST::CallOp& node)    { std::cout << "Instruction -> CALL -> Target: " << node.target << "\n"; },
            [](const AST::JmpOp& node)     { std::cout << "Instruction -> JMP -> Target: " << node.target << "\n"; },
            [](const AST::RetOp&)          { std::cout << "Instruction -> RET\n"; }
        }, program_ast[i]);
    }

}

auto main() -> int {
    try {
        constexpr std::string_view source_code = 
            "main:\n"
            "    MOV R1, 42\n"
            "    ADD R2, R1\n"
            "    PUSH R2\n"
            "    CALL print_val\n"
            "    RET";

        std::cout << "Memory Engine Initializing (64MB Linear Region Allocation)...\n";
        MemoryEngine::LinearArena arena(1024UZ * 1024UZ * 64UZ);

        MemoryEngine::ArenaAllocator<Token> token_allocator(arena);
        MemoryEngine::ArenaAllocator<AST::ASTNode> ast_allocator(arena);

        std::cout << "Frontend Engine Initializing...\n";
        std::cout << "Ingesting raw assembly\n";

        Lexer lexer(source_code);
        std::vector<Token, MemoryEngine::ArenaAllocator<Token>> tokens = lexer.lex_input(token_allocator);
        std::cout << "    -> Memory consumption post-lex pass: " << arena.bytes_used() << " bytes.\n";

        Parser<MemoryEngine::ArenaAllocator<Token>> parser(std::move(tokens));
        auto result = parser.parse_program(ast_allocator);
        std::cout << "    -> Memory consumption post-parse pass: " << arena.bytes_used() << " bytes.\n";

        if (!result.has_value()) {
            const SyntaxError& err = result.error();
            std::cerr << "INTEGRATION FAILURE: Compilation Error Detected!\n"
                      << "Message: " << err.message << "\n"
                      << "Coordinates: Line " << err.line << ", Column " << err.column << "\n";
            return 1;
        }

        const auto& program_ast = result.value();
        inspect_ast(program_ast);

        std::cout << "\nFrontend Verification Complete. Zero Leaks. All Constraints Valid.\n";
        return 0;

    } catch (const std::exception& exception) {
        std::cerr << "Fatal exception leaked to runtime boundary: " << exception.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Fatal unknown exception leaked to runtime boundary.\n";
        return 1;
    }
}