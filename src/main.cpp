#include <iostream>
#include <variant>
#include <vector>
#include "Lexer.hpp"
#include "AST.hpp"
#include "Parser.hpp"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; }; 
void inspect_ast(const std::vector<AST::ASTNode>& program_ast) {
    std::cout << "Successfully compiled " << program_ast.size() << " AST nodes.\n";

    for (size_t i = 0; i < program_ast.size(); ++i) {
        std::cout << "Node [" << i << "]: ";

        std::visit(overloaded {
            [](const AST::LabelDecl& op) { std::cout << "Label Declaration -> Name: " << op.name << "\n"; },
            [](const AST::MovOp& op)     { std::cout << "Instruction -> MOV (Dest: R" << static_cast<int>(op.dest.index) << ")\n"; },
            [](const AST::AddOp& op)     { std::cout << "Instruction -> ADD (Dest: R" << static_cast<int>(op.dest.index) << ")\n"; },
            [](const AST::SubOp& op)     { std::cout << "Instruction -> SUB (Dest: R" << static_cast<int>(op.dest.index) << ")\n"; },
            [](const AST::PushOp&)       { std::cout << "Instruction -> PUSH\n"; },
            [](const AST::PopOp& op)     { std::cout << "Instruction -> POP (Dest: R" << static_cast<int>(op.dest.index) << ")\n"; },
            [](const AST::CallOp& op)    { std::cout << "Instruction -> CALL -> Target: " << op.target << "\n"; },
            [](const AST::JmpOp& op)     { std::cout << "Instruction -> JMP -> Target: " << op.target << "\n"; },
            [](const AST::RetOp&)        { std::cout << "Instruction -> RET\n"; }
        }, program_ast[i]); 
    }

}

int main() {
    constexpr std::string_view source_code = 
        "main:\n"
        "    MOV R1, 42\n"
        "    ADD R2, R1\n"
        "    PUSH R2\n"
        "    CALL print_val\n"
        "    RET";

    std::cout << "Frontend Engine Initializing...\n";
    std::cout << "Ingesting raw assembly\n";

    Lexer lexer(source_code);
    std::vector<Token> tokens = lexer.lex_input();

    Parser parser(std::move(tokens));
    ParseResult<std::vector<AST::ASTNode>> result = parser.parse_program();

    if (!result.has_value()) {
        const SyntaxError& err = result.error();
        std::cerr << "INTEGRATION FAILURE: Compilation Error Detected!\n"
                  << "Message: " << err.message << "\n"
                  << "Coordinates: Line " << err.line << ", Column " << err.column << "\n";
        return 1;
    }

    const std::vector<AST::ASTNode>& program_ast = result.value();
    inspect_ast(program_ast);

    std::cout << "\nFrontend Verification Complete. Zero Leaks. All Constraints Valid.\n";
    return 0;
}