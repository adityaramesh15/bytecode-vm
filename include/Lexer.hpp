#pragma once
#include <string_view>
#include <vector> 
#include <cctype>
#include <charconv>
#include <algorithm>

enum class TokenType {
    Opcode,     // operations like MOVE, ADD, SUB, JMP, PUSH, POP, CALL, RET
    Register,   // R0 through R15    
    Immediate,  // Integer Literals
    Label,      // Identifiers ending with a colon or target markers 
    Comma,      // comma char 
    Identifier, // Non-opcode names (jump targets)
    EndOfFile   // Sentinel for completion of File
};

struct Token {
    TokenType token; 
    std::string_view lexeme;
    size_t line;
    size_t column;
};



class Lexer {
    public:
       constexpr Lexer(std::string_view sv = {}) noexcept : m_str_view(sv), m_line(1), m_column(1) {}
       template <typename Allocator = std::allocator<Token>> 
       std::vector<Token, Allocator> lex_input(Allocator alloc = Allocator()) noexcept {
            std::vector<Token, Allocator> tokens(alloc);

            while (true) {
                skip_whitespace_and_comments();
                if (m_str_view.empty()) {
                    tokens.push_back(Token{TokenType::EndOfFile, {}, m_line, m_column});
                    break;
                }
                tokens.push_back(emit_next_token());
            }

            return tokens;
        }

    
    private:
        std::string_view m_str_view;
        size_t m_line;
        size_t m_column;

        constexpr char peek() const noexcept {
            if (m_str_view.empty()) { return '\0'; }
            return m_str_view[0];
        }

        constexpr void advance (size_t n = 1) noexcept {
            n = std::min(n, m_str_view.size());     
            m_str_view.remove_prefix(n);
            m_column += n;
        }

        void skip_whitespace_and_comments() noexcept {
            while (!m_str_view.empty()) {
                char c = peek();

                if (c == ' '  || c == '\t' || c == '\r') {
                    advance(1);
                } else if (c == '\n') {
                    m_str_view.remove_prefix(1);
                    m_line++;
                    m_column = 1;
                } else if (c == ';') {
                    while (!m_str_view.empty() && peek() != '\n') {
                        advance(1);
                    }

                    if (!m_str_view.empty() && peek() == '\n') {
                        m_str_view.remove_prefix(1); 
                        m_line++;
                        m_column = 1;
                    }
                } else {
                    break;
                }

            }
        }

        constexpr Token emit_next_token() noexcept {
            char c = peek();
            size_t token_start_line = m_line;
            size_t token_start_col = m_column;

            if (c == ',') {
                std::string_view lexeme = m_str_view.substr(0, 1);
                advance(1);
                return Token{TokenType::Comma, lexeme, token_start_line, token_start_col};
            } 

            if (std::isdigit(c) || (c == '-' && std::isdigit(m_str_view.size() > 1 ? m_str_view[1] : '\0'))) {
                size_t length = 0;
                if (peek() == '-') length++;

                while (length < m_str_view.size() && std::isdigit(m_str_view[length])) {
                    length++;
                }
                
                std::string_view lexeme = m_str_view.substr(0, length);
                advance(length);
                return Token{TokenType::Immediate, lexeme, token_start_line, token_start_col};
            }

            if (std::isalpha(c) || c == '_') {
                size_t length = 0;
                while (length < m_str_view.size() && (std::isalnum(m_str_view[length]) || m_str_view[length] == '_')) {
                    length++;
                }

                std::string_view lexeme = m_str_view.substr(0, length);
                advance(length);

                if (peek() == ':') {
                    lexeme = std::string_view(lexeme.data(), length + 1);
                    advance(1); 
                    return Token{TokenType::Label, lexeme, token_start_line, token_start_col};
                }

                TokenType derived_type = classify_word(lexeme);
                return Token{derived_type, lexeme, token_start_line, token_start_col};
            }

            // eror rhandling in case faulty word shows up to prev infinite loop
            std::string_view faulty_lexeme = m_str_view.substr(0, 1);
            advance(1);
            return Token{TokenType::Identifier, faulty_lexeme, token_start_line, token_start_col};

            
        }

        constexpr TokenType classify_word(std::string_view word) const noexcept {
            if (word == "MOV" || word == "ADD" || word == "SUB" || 
                word == "JMP" || word == "PUSH" || word == "POP" || 
                word == "CALL" || word == "RET") {
                return TokenType::Opcode;
            }

            if (word.size() >= 2 && word[0] == 'R') {
                std::string_view num_part = word.substr(1);
                
                bool is_valid_digits = true;
                for (char ch : num_part) {
                    if (ch < '0' || ch > '9') is_valid_digits = false;
                }
                
                if (is_valid_digits && !num_part.empty()) {
                    int reg_val = 0;
                    auto [ptr, ec] = std::from_chars(num_part.data(), num_part.data() + num_part.size(), reg_val);
                    
                    if (ec == std::errc{} && reg_val >= 0 && reg_val <= 15) {
                        return TokenType::Register;
                    }
                }


            }

            return TokenType::Identifier;
        }

}; 