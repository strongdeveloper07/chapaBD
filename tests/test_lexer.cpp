#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "lexer/Token.h"
#include <stdexcept>

using namespace chapadb;

// ── Вспомогательные функции ───────────────────────────────────────────────

static std::vector<Token> tokenize(const std::string& sql) {
    Lexer lex(sql);
    return lex.tokenize();
}

// ── Ключевые слова ────────────────────────────────────────────────────────

TEST(LexerTest, DdlKeywords) {
    auto t = tokenize("CREATE DROP TABLE DATABASE");
    EXPECT_EQ(t[0].type, TokenType::KW_CREATE);
    EXPECT_EQ(t[1].type, TokenType::KW_DROP);
    EXPECT_EQ(t[2].type, TokenType::KW_TABLE);
    EXPECT_EQ(t[3].type, TokenType::KW_DATABASE);
}

TEST(LexerTest, DmlKeywords) {
    auto t = tokenize("SELECT FROM WHERE INSERT INTO VALUES UPDATE SET DELETE");
    EXPECT_EQ(t[0].type, TokenType::KW_SELECT);
    EXPECT_EQ(t[1].type, TokenType::KW_FROM);
    EXPECT_EQ(t[2].type, TokenType::KW_WHERE);
    EXPECT_EQ(t[3].type, TokenType::KW_INSERT);
    EXPECT_EQ(t[4].type, TokenType::KW_INTO);
    EXPECT_EQ(t[5].type, TokenType::KW_VALUES);
    EXPECT_EQ(t[6].type, TokenType::KW_UPDATE);
    EXPECT_EQ(t[7].type, TokenType::KW_SET);
    EXPECT_EQ(t[8].type, TokenType::KW_DELETE);
}

TEST(LexerTest, DataTypeKeywords) {
    auto t = tokenize("INT FLOAT BOOL TEXT VARCHAR");
    EXPECT_EQ(t[0].type, TokenType::KW_INT);
    EXPECT_EQ(t[1].type, TokenType::KW_FLOAT);
    EXPECT_EQ(t[2].type, TokenType::KW_BOOL);
    EXPECT_EQ(t[3].type, TokenType::KW_TEXT);
    EXPECT_EQ(t[4].type, TokenType::KW_VARCHAR);
}

TEST(LexerTest, LogicKeywords) {
    auto t = tokenize("AND OR NOT TRUE FALSE NULL");
    EXPECT_EQ(t[0].type, TokenType::KW_AND);
    EXPECT_EQ(t[1].type, TokenType::KW_OR);
    EXPECT_EQ(t[2].type, TokenType::KW_NOT);
    EXPECT_EQ(t[3].type, TokenType::KW_TRUE);
    EXPECT_EQ(t[4].type, TokenType::KW_FALSE);
    EXPECT_EQ(t[5].type, TokenType::KW_NULL);
}

// ── Нечувствительность к регистру ────────────────────────────────────────

TEST(LexerTest, CaseInsensitiveKeywords) {
    auto t = tokenize("select SELECT Select sElEcT");
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(t[i].type, TokenType::KW_SELECT) << "index " << i;
}

// ── Литералы ─────────────────────────────────────────────────────────────

TEST(LexerTest, IntegerLiteral) {
    auto t = tokenize("42");
    EXPECT_EQ(t[0].type, TokenType::LIT_INTEGER);
    EXPECT_EQ(t[0].value, "42");
}

TEST(LexerTest, FloatLiteral) {
    auto t = tokenize("3.14");
    EXPECT_EQ(t[0].type, TokenType::LIT_FLOAT);
    EXPECT_EQ(t[0].value, "3.14");
}

TEST(LexerTest, StringLiteral) {
    auto t = tokenize("'hello world'");
    EXPECT_EQ(t[0].type, TokenType::LIT_STRING);
    EXPECT_EQ(t[0].value, "hello world");
}

TEST(LexerTest, StringLiteralWithEscapes) {
    auto t = tokenize("'it\\'s a test'");
    EXPECT_EQ(t[0].type, TokenType::LIT_STRING);
    EXPECT_EQ(t[0].value, "it's a test");
}

TEST(LexerTest, EmptyStringLiteral) {
    auto t = tokenize("''");
    EXPECT_EQ(t[0].type, TokenType::LIT_STRING);
    EXPECT_EQ(t[0].value, "");
}

// ── Операторы сравнения ───────────────────────────────────────────────────

TEST(LexerTest, ComparisonOperators) {
    auto t = tokenize("= != < > <= >=");
    EXPECT_EQ(t[0].type, TokenType::OP_EQ);
    EXPECT_EQ(t[1].type, TokenType::OP_NEQ);
    EXPECT_EQ(t[2].type, TokenType::OP_LT);
    EXPECT_EQ(t[3].type, TokenType::OP_GT);
    EXPECT_EQ(t[4].type, TokenType::OP_LE);
    EXPECT_EQ(t[5].type, TokenType::OP_GE);
}

// ── Символы ──────────────────────────────────────────────────────────────

TEST(LexerTest, Punctuation) {
    auto t = tokenize("( ) , ; *");
    EXPECT_EQ(t[0].type, TokenType::SYM_LPAREN);
    EXPECT_EQ(t[1].type, TokenType::SYM_RPAREN);
    EXPECT_EQ(t[2].type, TokenType::SYM_COMMA);
    EXPECT_EQ(t[3].type, TokenType::SYM_SEMICOL);
    EXPECT_EQ(t[4].type, TokenType::SYM_STAR);
}

// ── Идентификаторы ────────────────────────────────────────────────────────

TEST(LexerTest, Identifier) {
    auto t = tokenize("my_table column123");
    EXPECT_EQ(t[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(t[0].value, "my_table");
    EXPECT_EQ(t[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(t[1].value, "column123");
}

// USERS — зарезервированное слово, должно быть KW_USERS, а не IDENTIFIER
TEST(LexerTest, UsersIsReservedKeyword) {
    auto t = tokenize("users");
    EXPECT_EQ(t[0].type, TokenType::KW_USERS);
}

// ── Комментарии ───────────────────────────────────────────────────────────

TEST(LexerTest, SkipsLineComment) {
    auto t = tokenize("SELECT -- ignored\nFROM");
    EXPECT_EQ(t[0].type, TokenType::KW_SELECT);
    EXPECT_EQ(t[1].type, TokenType::KW_FROM);
    EXPECT_EQ(t[2].type, TokenType::END_OF_FILE);
}

// ── EOF ───────────────────────────────────────────────────────────────────

TEST(LexerTest, AlwaysEndsWithEof) {
    auto t = tokenize("SELECT 1");
    EXPECT_EQ(t.back().type, TokenType::END_OF_FILE);
}

TEST(LexerTest, EmptyInputGivesOnlyEof) {
    auto t = tokenize("");
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].type, TokenType::END_OF_FILE);
}

// ── Ошибки ────────────────────────────────────────────────────────────────

TEST(LexerTest, UnterminatedStringThrows) {
    EXPECT_THROW(tokenize("'unclosed"), std::runtime_error);
}

TEST(LexerTest, UnexpectedCharacterThrows) {
    EXPECT_THROW(tokenize("SELECT @ FROM t"), std::runtime_error);
}

// ── Полный запрос ─────────────────────────────────────────────────────────

TEST(LexerTest, FullInsertQuery) {
    auto t = tokenize("INSERT INTO products VALUES (1, 'Widget', 9.99, TRUE);");
    EXPECT_EQ(t[0].type, TokenType::KW_INSERT);
    EXPECT_EQ(t[1].type, TokenType::KW_INTO);
    EXPECT_EQ(t[2].type, TokenType::IDENTIFIER);  // products
    EXPECT_EQ(t[3].type, TokenType::KW_VALUES);
    EXPECT_EQ(t[4].type, TokenType::SYM_LPAREN);
    EXPECT_EQ(t[5].type, TokenType::LIT_INTEGER); // 1
    EXPECT_EQ(t[6].type, TokenType::SYM_COMMA);
    EXPECT_EQ(t[7].type, TokenType::LIT_STRING);  // Widget
}
