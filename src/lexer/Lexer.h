#pragma once
#include "Token.h"
#include <string>
#include <vector>

namespace chapadb {

class Lexer {
public:
    explicit Lexer(std::string source);

    /// Токенизирует всю строку и возвращает список токенов
    std::vector<Token> tokenize();

private:
    std::string m_source;
    size_t      m_pos;
    int         m_line;
    int         m_col;

    char        peek() const;
    char        advance();
    bool        isAtEnd() const;

    void        skipWhitespace();
    void        skipLineComment(); 

    Token       readNumber();
    Token       readString();      
    Token       readIdentifierOrKeyword();

    Token       makeToken(TokenType t, std::string val) const;
};

} 
