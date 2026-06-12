#pragma once
#include <vector>
#include <span>
#include <charconv> 
#include <optional>
#include <expected> 
#include <variant>
#include "Lexer.hpp"
#include "AST.hpp"

class Parser {
    public:
        explicit Parser(std::vector<Token> tokens) noexcept : m_tokens(std::move(tokens)) {}
        ParseResult<std::vector<AST::ASTNode>> parse_program() noexcept {
            std::vector<AST::ASTNode> program_ast;
            
            while (!is_at_end()) {
                auto node_result = parse_next_instruction();
                if (!node_result) {
                    return std::unexpected(node_result.error()); 
                }
                program_ast.push_back(std::move(*node_result));
            }
            
            return program_ast;
        }


    private:
        std::vector<Token> m_tokens; 
        size_t m_cursor{0};

        [[nodiscard]] bool is_at_end() const noexcept {
            return m_cursor >= m_tokens.size() || m_tokens[m_cursor].token == TokenType::EndOfFile;
        }


        [[nodiscard]] Token peek() const noexcept {
            if (is_at_end()) {
                return m_tokens.back();
            }
            return m_tokens[m_cursor];
        }

        [[nodiscard]] Token peek_ahead(size_t offset) const noexcept {
            if (m_cursor + offset >= m_tokens.size()) {
                return m_tokens.back();
            }
            return m_tokens[m_cursor + offset];
        }

        void advance() noexcept {
            if (!is_at_end()) {
                m_cursor++;
            }
        }

        // for monadic evaluation via std::variant
        ParseResult<Token> consume(TokenType expected, std::string_view error_message) noexcept {
            Token current = peek();
            if (current.token != expected) {
                return std::unexpected(SyntaxError{
                    .message = error_message,
                    .line = current.line,
                    .column = current.column
                });
            }
            advance();
            return current;
        }

        // converting register into VirtualRegister
        ParseResult<VirtualRegister> extract_register(Token tok) noexcept {
            if (tok.token != TokenType::Register) {
                return std::unexpected(SyntaxError{"Expected a valid register identifier", tok.line, tok.column});
            }
            
            std::string_view num_part = tok.lexeme.substr(1);
            uint8_t index = 0;
            auto [ptr, ec] = std::from_chars(num_part.data(), num_part.data() + num_part.size(), index);
            
            if (ec != std::errc{}) {
                return std::unexpected(SyntaxError{"Failed to parse register numeric index", tok.line, tok.column});
            }
            return VirtualRegister{index};
        }

        // converting matched Immediate token into signed 64 bit scalar
        ParseResult<int64_t> extract_immediate(Token tok) noexcept {
            if (tok.token != TokenType::Immediate) {
                return std::unexpected(SyntaxError{"Expected an integer immediate value", tok.line, tok.column});
            }
            int64_t value = 0;
            auto [ptr, ec] = std::from_chars(tok.lexeme.data(), tok.lexeme.data() + tok.lexeme.size(), value);
            
            if (ec != std::errc{}) {
                return std::unexpected(SyntaxError{"Immediate literal value out of bounds for int64_t", tok.line, tok.column});
            }
            return value;
        }
        


        ParseResult<Operand> parse_operand() noexcept {
            Token current = peek();
            if (current.token == TokenType::Register) {
                auto reg_res = extract_register(current);
                if (!reg_res) return std::unexpected(reg_res.error());
                advance();
                std::variant<VirtualRegister, int64_t, std::string_view> value{std::in_place_type<VirtualRegister>, *reg_res};
                return Operand{value};
            }
            if (current.token == TokenType::Immediate) {
                auto imm_res = extract_immediate(current);
                if (!imm_res) return std::unexpected(imm_res.error());
                advance();
                std::variant<VirtualRegister, int64_t, std::string_view> value{std::in_place_type<int64_t>, *imm_res};
                return Operand{value};
            }
            if (current.token == TokenType::Identifier) {
                advance();
                std::variant<VirtualRegister, int64_t, std::string_view> value{std::in_place_type<std::string_view>, current.lexeme};
                return Operand{value};
            }
            return std::unexpected(SyntaxError{
                .message = "Expected a valid operand (Register, Immediate literal, or Jump target identifier)",
                .line = current.line,
                .column = current.column
            });
        }


        ParseResult<AST::ASTNode> parse_mov_op() noexcept {
        auto dest_tok = consume(TokenType::Register, "MOV instruction requires a destination register as its first operand");
        if (!dest_tok) return std::unexpected(dest_tok.error());

        auto dest_reg = extract_register(*dest_tok);
        if (!dest_reg) return std::unexpected(dest_reg.error());

        if (!consume(TokenType::Comma, "Expected a ',' separator between destination and source operands")) {
            return std::unexpected(SyntaxError{"Expected ',' separating register and source operand", peek().line, peek().column});
        }

        auto src_op = parse_operand();
        if (!src_op) return std::unexpected(src_op.error());

        AST::ASTNode node{std::in_place_type<AST::MovOp>, AST::MovOp{.dest = *dest_reg, .src = *src_op}};
        return node;
    }

    ParseResult<AST::ASTNode> parse_add_op() noexcept {
        auto dest_tok = consume(TokenType::Register, "ADD instruction requires a destination register as its first operand");
        if (!dest_tok) return std::unexpected(dest_tok.error());

        auto dest_reg = extract_register(*dest_tok);
        if (!dest_reg) return std::unexpected(dest_reg.error());

        if (!consume(TokenType::Comma, "Expected a ',' separator between destination and source operands")) {
            return std::unexpected(SyntaxError{"Expected ',' separating register and source operand", peek().line, peek().column});
        }

        auto src_op = parse_operand();
        if (!src_op) return std::unexpected(src_op.error());

        AST::ASTNode node{std::in_place_type<AST::AddOp>, AST::AddOp{.dest = *dest_reg, .src = *src_op}};
        return node;
    }

    ParseResult<AST::ASTNode> parse_sub_op() noexcept {
        auto dest_tok = consume(TokenType::Register, "SUB instruction requires a destination register as its first operand");
        if (!dest_tok) return std::unexpected(dest_tok.error());

        auto dest_reg = extract_register(*dest_tok);
        if (!dest_reg) return std::unexpected(dest_reg.error());

        if (!consume(TokenType::Comma, "Expected a ',' separator between destination and source operands")) {
            return std::unexpected(SyntaxError{"Expected ',' separating register and source operand", peek().line, peek().column});
        }

        auto src_op = parse_operand();
        if (!src_op) return std::unexpected(src_op.error());

        AST::ASTNode node{std::in_place_type<AST::SubOp>, AST::SubOp{.dest = *dest_reg, .src = *src_op}};
        return node;
    }

    ParseResult<AST::ASTNode> parse_jmp_op() noexcept {
        auto target_tok = consume(TokenType::Identifier, "JMP instruction requires an identifier label target name");
        if (!target_tok) return std::unexpected(target_tok.error());
        AST::ASTNode node{std::in_place_type<AST::JmpOp>, AST::JmpOp{.target = target_tok->lexeme}};
        return node;
    }

    ParseResult<AST::ASTNode> parse_push_op() noexcept {
        auto src_op = parse_operand();
        if (!src_op) return std::unexpected(src_op.error());
        AST::ASTNode node{std::in_place_type<AST::PushOp>, AST::PushOp{.src = *src_op}};
        return node;
    }

    ParseResult<AST::ASTNode> parse_pop_op() noexcept {
        auto dest_tok = consume(TokenType::Register, "POP instruction requires a destination register operand");
        if (!dest_tok) return std::unexpected(dest_tok.error());

        auto dest_reg = extract_register(*dest_tok);
        if (!dest_reg) return std::unexpected(dest_reg.error());

        AST::ASTNode node{std::in_place_type<AST::PopOp>, AST::PopOp{.dest = *dest_reg}};
        return node;
    }

    ParseResult<AST::ASTNode> parse_call_op() noexcept {
        auto target_tok = consume(TokenType::Identifier, "CALL instruction requires an identifier function label name");
        if (!target_tok) return std::unexpected(target_tok.error());
        AST::ASTNode node{std::in_place_type<AST::CallOp>, AST::CallOp{.target = target_tok->lexeme}};
        return node;
    }

    ParseResult<AST::ASTNode> parse_next_instruction() noexcept {
        Token current = peek();

        if (current.token == TokenType::Label) {
            advance();
            std::string_view label_name = current.lexeme;
            if (!label_name.empty() && label_name.back() == ':') {
                label_name.remove_suffix(1);
            }
            AST::ASTNode node{std::in_place_type<AST::LabelDecl>, AST::LabelDecl{.name = label_name}};
            return node;
        }

        if (current.token == TokenType::Opcode) {
            advance();
            std::string_view opcode_lexeme = current.lexeme;

            if (opcode_lexeme == "MOV")  return parse_mov_op();
            if (opcode_lexeme == "ADD")  return parse_add_op();
            if (opcode_lexeme == "SUB")  return parse_sub_op();
            if (opcode_lexeme == "JMP")  return parse_jmp_op();
            if (opcode_lexeme == "PUSH") return parse_push_op();
            if (opcode_lexeme == "POP")  return parse_pop_op();
            if (opcode_lexeme == "CALL") return parse_call_op();
            if (opcode_lexeme == "RET") {
                AST::ASTNode node{std::in_place_type<AST::RetOp>, AST::RetOp{}};
                return node;
            }
        }

        return std::unexpected(SyntaxError{
            .message = "Invalid or unrecognized instruction syntax. Expected an instruction operation or a label declaration.",
            .line = current.line,
            .column = current.column
        });
    }


}; 