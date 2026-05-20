#include "Lexer.h"
#include <cctype>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>

namespace chapadb {

/// Таблица ключевых слов (в верхнем регистре → тип токена)
static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"SELECT",   TokenType::KW_SELECT},
    {"FROM",     TokenType::KW_FROM},
    {"WHERE",    TokenType::KW_WHERE},
    {"INSERT",   TokenType::KW_INSERT},
    {"INTO",     TokenType::KW_INTO},
    {"VALUES",   TokenType::KW_VALUES},
    {"UPDATE",   TokenType::KW_UPDATE},
    {"SET",      TokenType::KW_SET},
    {"DELETE",   TokenType::KW_DELETE},
    {"CREATE",   TokenType::KW_CREATE},
    {"DROP",     TokenType::KW_DROP},
    {"TABLE",    TokenType::KW_TABLE},
    {"DATABASE", TokenType::KW_DATABASE},
    {"USE",      TokenType::KW_USE},
    {"INT",      TokenType::KW_INT},
    {"INTEGER",  TokenType::KW_INT},
    {"FLOAT",    TokenType::KW_FLOAT},
    {"BOOL",     TokenType::KW_BOOL},
    {"BOOLEAN",  TokenType::KW_BOOL},
    {"TEXT",     TokenType::KW_TEXT},
    {"VARCHAR",  TokenType::KW_VARCHAR},
    {"AND",      TokenType::KW_AND},
    {"OR",       TokenType::KW_OR},
    {"NOT",      TokenType::KW_NOT},
    {"TRUE",     TokenType::KW_TRUE},
    {"FALSE",    TokenType::KW_FALSE},
    {"NULL",     TokenType::KW_NULL},
    // Расширенные клаузы
    {"ORDER",    TokenType::KW_ORDER},
    {"BY",       TokenType::KW_BY},
    {"ASC",      TokenType::KW_ASC},
    {"DESC",     TokenType::KW_DESC},
    {"LIMIT",    TokenType::KW_LIMIT},
    {"OFFSET",   TokenType::KW_OFFSET},
    {"GROUP",    TokenType::KW_GROUP},
    {"HAVING",   TokenType::KW_HAVING},
    {"COUNT",    TokenType::KW_COUNT},
    {"SUM",      TokenType::KW_SUM},
    {"AVG",      TokenType::KW_AVG},
    {"MIN",      TokenType::KW_MIN},
    {"MAX",      TokenType::KW_MAX},
    {"JOIN",      TokenType::KW_JOIN},
    {"INNER",     TokenType::KW_INNER},
    {"ON",        TokenType::KW_ON},
    // Управление пользователями
    {"USER",      TokenType::KW_USER},
    {"PASSWORD",  TokenType::KW_PASSWORD},
    {"WITH",      TokenType::KW_WITH},
    {"ROLE",      TokenType::KW_ROLE},
    {"GRANT",     TokenType::KW_GRANT},
    {"REVOKE",    TokenType::KW_REVOKE},
    {"TO",        TokenType::KW_TO},
    {"ALL",       TokenType::KW_ALL},
    {"SHOW",      TokenType::KW_SHOW},
    {"USERS",     TokenType::KW_USERS},
    {"AUTH",      TokenType::KW_AUTH},
    {"READONLY",  TokenType::KW_READONLY},
    {"READWRITE", TokenType::KW_READWRITE},
    {"ADMIN",     TokenType::KW_ADMIN},
};

Lexer::Lexer(std::string source)
    : m_source(std::move(source)), m_pos(0), m_line(1), m_col(1)
{}

char Lexer::peek() const {
    if (m_pos >= m_source.size()) return '\0';
    return m_source[m_pos];
}

char Lexer::advance() {
    char c = m_source[m_pos++];
    if (c == '\n') { ++m_line; m_col = 1; }
    else           { ++m_col; }
    return c;
}

bool Lexer::isAtEnd() const {
    return m_pos >= m_source.size();
}

void Lexer::skipWhitespace() {
    while (!isAtEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
        advance();
    }
}

void Lexer::skipLineComment() {
    while (!isAtEnd() && peek() != '\n') advance();
}

Token Lexer::makeToken(TokenType t, std::string val) const {
    return Token{t, std::move(val), m_line, m_col};
}

Token Lexer::readNumber() {
    int    startLine = m_line;
    int    startCol  = m_col;
    std::string buf;
    bool   isFloat   = false;

    while (!isAtEnd() && (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.')) {
        if (peek() == '.') {
            if (isFloat) break; // второй — уже не наш
            isFloat = true;
        }
        buf += advance();
    }

    return Token{isFloat ? TokenType::LIT_FLOAT : TokenType::LIT_INTEGER, buf, startLine, startCol};
}

Token Lexer::readString() {
    int startLine = m_line;
    int startCol  = m_col;
    advance(); // пропускаем открывающую '
    std::string buf;

    while (!isAtEnd() && peek() != '\'') {
        if (peek() == '\\') {
            advance();
            char esc = advance();
            switch (esc) {
                case 'n':  buf += '\n'; break;
                case 't':  buf += '\t'; break;
                case '\'': buf += '\''; break;
                case '\\': buf += '\\'; break;
                default:   buf += esc;  break;
            }
        } else {
            buf += advance();
        }
    }

    if (isAtEnd()) {
        throw std::runtime_error("Незакрытый строковый литерал на строке " + std::to_string(startLine));
    }
    advance(); // закрывающая '
    return Token{TokenType::LIT_STRING, buf, startLine, startCol};
}

Token Lexer::readIdentifierOrKeyword() {
    int startLine = m_line;
    int startCol  = m_col;
    std::string buf;

    while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
        buf += advance();
    }

    // Нормализуем к верхнему регистру для поиска ключевых слов
    std::string upper = buf;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    auto it = KEYWORDS.find(upper);
    if (it != KEYWORDS.end()) {
        return Token{it->second, buf, startLine, startCol};
    }
    return Token{TokenType::IDENTIFIER, buf, startLine, startCol};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespace();
        if (isAtEnd()) break;

        int curLine = m_line;
        int curCol  = m_col;
        char c = peek();

        // Однострочный комментарий
        if (c == '-' && m_pos + 1 < m_source.size() && m_source[m_pos + 1] == '-') {
            skipLineComment();
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(readNumber());
            continue;
        }
        if (c == '\'') {
            tokens.push_back(readString());
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }

        // Однои двухсимвольные операторы и знаки препинания
        advance();
        switch (c) {
            case '=': tokens.push_back({TokenType::OP_EQ,     "=",  curLine, curCol}); break;
            case '<':
                if (!isAtEnd() && peek() == '=') {
                    advance();
                    tokens.push_back({TokenType::OP_LE, "<=", curLine, curCol});
                } else {
                    tokens.push_back({TokenType::OP_LT, "<",  curLine, curCol});
                }
                break;
            case '>':
                if (!isAtEnd() && peek() == '=') {
                    advance();
                    tokens.push_back({TokenType::OP_GE, ">=", curLine, curCol});
                } else {
                    tokens.push_back({TokenType::OP_GT, ">",  curLine, curCol});
                }
                break;
            case '!':
                if (!isAtEnd() && peek() == '=') {
                    advance();
                    tokens.push_back({TokenType::OP_NEQ, "!=", curLine, curCol});
                } else {
                    throw std::runtime_error(
                        "Неожиданный символ '!' на строке " + std::to_string(curLine));
                }
                break;
            case '(': tokens.push_back({TokenType::SYM_LPAREN,  "(", curLine, curCol}); break;
            case ')': tokens.push_back({TokenType::SYM_RPAREN,  ")", curLine, curCol}); break;
            case ',': tokens.push_back({TokenType::SYM_COMMA,   ",", curLine, curCol}); break;
            case ';': tokens.push_back({TokenType::SYM_SEMICOL, ";", curLine, curCol}); break;
            case '*': tokens.push_back({TokenType::SYM_STAR,    "*", curLine, curCol}); break;
            default:
                throw std::runtime_error(
                    std::string("Неожиданный символ '") + c + "' на строке " + std::to_string(curLine));
        }
    }

    tokens.push_back(Token{TokenType::END_OF_FILE, "", m_line, m_col});
    return tokens;
}

} // namespace chapadb
