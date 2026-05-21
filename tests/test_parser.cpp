#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "parser/ast/Statements.h"
#include "common/Types.h"
#include <stdexcept>

using namespace chapadb;

// ── Вспомогательные функции ───────────────────────────────────────────────

static std::unique_ptr<ASTNode> parse(const std::string& sql) {
    Lexer  lex(sql);
    auto   tokens = lex.tokenize();
    Parser p(std::move(tokens));
    return p.parse();
}

template<typename T>
static T* as(std::unique_ptr<ASTNode>& node) {
    auto* ptr = dynamic_cast<T*>(node.get());
    return ptr;
}

// ── DDL: DATABASE ─────────────────────────────────────────────────────────

TEST(ParserTest, CreateDatabase) {
    auto ast = parse("CREATE DATABASE mydb;");
    auto* stmt = dynamic_cast<CreateDatabaseStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->dbName, "mydb");
}

TEST(ParserTest, DropDatabase) {
    auto ast = parse("DROP DATABASE mydb;");
    auto* stmt = dynamic_cast<DropDatabaseStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->dbName, "mydb");
}

TEST(ParserTest, UseDatabase) {
    auto ast = parse("USE mydb;");
    auto* stmt = dynamic_cast<UseDatabaseStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->dbName, "mydb");
}

// ── DDL: TABLE ────────────────────────────────────────────────────────────

TEST(ParserTest, CreateTableAllTypes) {
    auto ast = parse("CREATE TABLE t (a INT, b FLOAT, c BOOL, d TEXT, e VARCHAR(100));");
    auto* stmt = dynamic_cast<CreateTableStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->tableName, "t");
    ASSERT_EQ(stmt->columns.size(), 5u);
    EXPECT_EQ(stmt->columns[0].type, DataType::INT);
    EXPECT_EQ(stmt->columns[1].type, DataType::FLOAT);
    EXPECT_EQ(stmt->columns[2].type, DataType::BOOL);
    EXPECT_EQ(stmt->columns[3].type, DataType::TEXT);
    EXPECT_EQ(stmt->columns[4].type, DataType::VARCHAR);
    EXPECT_EQ(stmt->columns[4].param, 100u);
}

TEST(ParserTest, DropTable) {
    auto ast = parse("DROP TABLE employees;");
    auto* stmt = dynamic_cast<DropTableStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->tableName, "employees");
}

// КЛЮЧЕВОЙ: 'users' — зарезервированное слово, парсер обязан принять его как имя таблицы
TEST(ParserTest, CreateTableNamedUsers) {
    EXPECT_NO_THROW({
        auto ast = parse("CREATE TABLE users (id INT, name VARCHAR(50));");
        auto* stmt = dynamic_cast<CreateTableStatement*>(ast.get());
        ASSERT_NE(stmt, nullptr);
        EXPECT_EQ(stmt->tableName, "users");
    });
}

// ── SELECT ────────────────────────────────────────────────────────────────

TEST(ParserTest, SelectStar) {
    auto ast = parse("SELECT * FROM employees;");
    auto* stmt = dynamic_cast<SelectStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->selectAll);
    EXPECT_EQ(stmt->tableName, "employees");
    EXPECT_EQ(stmt->where, nullptr);
}

TEST(ParserTest, SelectColumns) {
    auto ast = parse("SELECT id, name FROM employees;");
    auto* stmt = dynamic_cast<SelectStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->selectAll);
    ASSERT_EQ(stmt->columns.size(), 2u);
    EXPECT_EQ(stmt->columns[0], "id");
    EXPECT_EQ(stmt->columns[1], "name");
}

TEST(ParserTest, SelectWithWhereEquals) {
    auto ast = parse("SELECT * FROM t WHERE id = 1;");
    auto* stmt = dynamic_cast<SelectStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->where, nullptr);
}

TEST(ParserTest, SelectWithWhereAndOr) {
    auto ast = parse("SELECT * FROM t WHERE a = 1 AND b = 2 OR c = 3;");
    auto* stmt = dynamic_cast<SelectStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->where, nullptr);
}

TEST(ParserTest, SelectFromUsersTable) {
    EXPECT_NO_THROW({
        auto ast = parse("SELECT * FROM users WHERE id = 1;");
        auto* stmt = dynamic_cast<SelectStatement*>(ast.get());
        ASSERT_NE(stmt, nullptr);
        EXPECT_EQ(stmt->tableName, "users");
    });
}

// ── INSERT ────────────────────────────────────────────────────────────────

TEST(ParserTest, InsertSingleRow) {
    auto ast = parse("INSERT INTO t VALUES (1, 'Alice', 25, TRUE);");
    auto* stmt = dynamic_cast<InsertStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->tableName, "t");
    ASSERT_EQ(stmt->values.size(), 1u);
    ASSERT_EQ(stmt->values[0].size(), 4u);
}

TEST(ParserTest, InsertMultipleRows) {
    auto ast = parse("INSERT INTO t VALUES (1, 'a'), (2, 'b'), (3, 'c');");
    auto* stmt = dynamic_cast<InsertStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->values.size(), 3u);
}

TEST(ParserTest, InsertIntoUsersTable) {
    EXPECT_NO_THROW({
        auto ast = parse("INSERT INTO users VALUES (1, 'Alice', 25, TRUE);");
        auto* stmt = dynamic_cast<InsertStatement*>(ast.get());
        ASSERT_NE(stmt, nullptr);
        EXPECT_EQ(stmt->tableName, "users");
    });
}

TEST(ParserTest, InsertWithColumnList) {
    auto ast = parse("INSERT INTO t (id, name) VALUES (1, 'Bob');");
    auto* stmt = dynamic_cast<InsertStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    ASSERT_EQ(stmt->columns.size(), 2u);
    EXPECT_EQ(stmt->columns[0], "id");
    EXPECT_EQ(stmt->columns[1], "name");
}

TEST(ParserTest, InsertNullValue) {
    auto ast = parse("INSERT INTO t VALUES (1, NULL);");
    auto* stmt = dynamic_cast<InsertStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(stmt->values[0][1]));
}

// ── UPDATE ────────────────────────────────────────────────────────────────

TEST(ParserTest, UpdateWithWhere) {
    auto ast = parse("UPDATE employees SET salary = 60000.0 WHERE id = 1;");
    auto* stmt = dynamic_cast<UpdateStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->tableName, "employees");
    ASSERT_EQ(stmt->assignments.size(), 1u);
    EXPECT_EQ(stmt->assignments[0].first, "salary");
    EXPECT_NE(stmt->where, nullptr);
}

TEST(ParserTest, UpdateMultipleColumns) {
    auto ast = parse("UPDATE t SET a = 1, b = 'x', c = TRUE;");
    auto* stmt = dynamic_cast<UpdateStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->assignments.size(), 3u);
}

TEST(ParserTest, UpdateUsersTable) {
    EXPECT_NO_THROW({
        auto ast = parse("UPDATE users SET age = 30 WHERE id = 1;");
        auto* stmt = dynamic_cast<UpdateStatement*>(ast.get());
        ASSERT_NE(stmt, nullptr);
        EXPECT_EQ(stmt->tableName, "users");
    });
}

// ── DELETE ────────────────────────────────────────────────────────────────

TEST(ParserTest, DeleteWithWhere) {
    auto ast = parse("DELETE FROM employees WHERE id = 1;");
    auto* stmt = dynamic_cast<DeleteStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->tableName, "employees");
    EXPECT_NE(stmt->where, nullptr);
}

TEST(ParserTest, DeleteAllRows) {
    auto ast = parse("DELETE FROM t;");
    auto* stmt = dynamic_cast<DeleteStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->where, nullptr);
}

// ── Значения литералов ────────────────────────────────────────────────────

TEST(ParserTest, IntegerValueParsed) {
    auto ast = parse("INSERT INTO t VALUES (42);");
    auto* stmt = dynamic_cast<InsertStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(std::get<int32_t>(stmt->values[0][0]), 42);
}

TEST(ParserTest, FloatValueParsed) {
    auto ast = parse("INSERT INTO t VALUES (3.14);");
    auto* stmt = dynamic_cast<InsertStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_DOUBLE_EQ(std::get<double>(stmt->values[0][0]), 3.14);
}

TEST(ParserTest, BoolValueParsed) {
    auto ast = parse("INSERT INTO t VALUES (TRUE, FALSE);");
    auto* stmt = dynamic_cast<InsertStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(std::get<bool>(stmt->values[0][0]));
    EXPECT_FALSE(std::get<bool>(stmt->values[0][1]));
}

TEST(ParserTest, StringValueParsed) {
    auto ast = parse("INSERT INTO t VALUES ('hello');");
    auto* stmt = dynamic_cast<InsertStatement*>(ast.get());
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(std::get<std::string>(stmt->values[0][0]), "hello");
}

// ── Ошибки парсинга ───────────────────────────────────────────────────────

TEST(ParserTest, InvalidStatementThrows) {
    EXPECT_THROW(parse("NOT A VALID SQL;"), std::runtime_error);
}

TEST(ParserTest, MissingSemicolonParsesOk) {
    // Парсер не требует обязательный ';' — match(SEMICOL) не бросает исключение
    EXPECT_NO_THROW(parse("SELECT * FROM t"));
}
