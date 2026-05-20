#pragma once
#include "../lexer/Token.h"
#include "ast/ASTNode.h"
#include "ast/Statements.h"
#include <vector>
#include <memory>

namespace chapadb {

/**
 * <summary>
 * Рекурсивно-нисходящий парсер SQL.
 * Принимает поток токенов от Lexer и строит AST.
 * </summary>
 */
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    /// Разбирает один SQL-оператор и возвращает его AST-узел
    std::unique_ptr<ASTNode> parse();

    // Публичные методы для вспомогательных функций парсинга
    const Token& peek() const;
    const Token& advance();
    bool         check(TokenType t) const;
    bool         match(TokenType t);

private:
    std::vector<Token> m_tokens;
    size_t             m_pos;
    const Token& expect(TokenType t, const std::string& msg);

    // Парсинг операторов
    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<ASTNode> parseCreate();
    std::unique_ptr<ASTNode> parseDrop();
    std::unique_ptr<ASTNode> parseSelect();
    std::unique_ptr<ASTNode> parseInsert();
    std::unique_ptr<ASTNode> parseUpdate();
    std::unique_ptr<ASTNode> parseDelete();
    std::unique_ptr<ASTNode> parseUse();
    std::unique_ptr<ASTNode> parseCreateUser();
    std::unique_ptr<ASTNode> parseDropUser();
    std::unique_ptr<ASTNode> parseShowUsers();
    std::unique_ptr<ASTNode> parseAuth();
    std::unique_ptr<ASTNode> parseGrant();
    std::unique_ptr<ASTNode> parseRevoke();

    // Парсинг определения столбца в CREATE TABLE
    ColumnSchema parseColumnDef();

    // Парсинг WHERE
    std::unique_ptr<ConditionNode> parseWhere();
    std::unique_ptr<ConditionNode> parseOr();
    std::unique_ptr<ConditionNode> parseAnd();
    std::unique_ptr<ConditionNode> parseNot();
    std::unique_ptr<ConditionNode> parsePrimary();

    // Парсинг элементов проекции SELECT
    SelectItem parseSelectItem();

    // Парсинг одного значения (литерал)
    Value parseValue();

    CompareOp parseCompareOp();
};

} // namespace chapadb
