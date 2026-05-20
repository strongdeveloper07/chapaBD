#pragma once
#include <string>
#include <cstdint>

namespace chapadb {

enum class TokenType {
    // Ключевые слова
    KW_SELECT, KW_FROM, KW_WHERE,
    KW_INSERT, KW_INTO, KW_VALUES,
    KW_UPDATE, KW_SET,
    KW_DELETE,
    KW_CREATE, KW_DROP,
    KW_TABLE, KW_DATABASE,
    KW_USE,
    // Типы данных
    KW_INT, KW_FLOAT, KW_BOOL, KW_TEXT, KW_VARCHAR,
    // Логические операторы
    KW_AND, KW_OR, KW_NOT,
    // Литералы
    LIT_INTEGER, LIT_FLOAT, LIT_STRING,
    KW_TRUE, KW_FALSE, KW_NULL,
    // Операторы сравнения
    OP_EQ,  // =
    OP_NEQ, // !=
    OP_LT,  // <
    OP_GT,  // >
    OP_LE,  // <=
    OP_GE,  // >=
    // Символы
    SYM_LPAREN,  // (
    SYM_RPAREN,  // )
    SYM_COMMA,   // ,
    SYM_SEMICOL, // ;
    SYM_STAR,    // *
    // Расширенные клаузы SELECT
    KW_ORDER, KW_BY, KW_ASC, KW_DESC,
    KW_LIMIT, KW_OFFSET,
    KW_GROUP, KW_HAVING,
    // Агрегатные функции
    KW_COUNT, KW_SUM, KW_AVG, KW_MIN, KW_MAX,
    // JOIN
    KW_JOIN, KW_INNER, KW_ON,
    // Управление пользователями
    KW_USER, KW_PASSWORD, KW_WITH, KW_ROLE,
    KW_GRANT, KW_REVOKE, KW_TO, KW_ALL,
    KW_SHOW, KW_USERS, KW_AUTH,
    KW_READONLY, KW_READWRITE, KW_ADMIN,
    // Идентификатор
    IDENTIFIER,
    // Конец потока
    END_OF_FILE
};

struct Token {
    TokenType   type;
    std::string value;   // текстовое представление токена
    int         line;
    int         column;
};

} 
