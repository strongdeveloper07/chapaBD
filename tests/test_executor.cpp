#include <gtest/gtest.h>
#include "test_helper.h"

using namespace chapadb;
using namespace chapadb::test;

class ExecutorTest : public ::testing::Test {
protected:
    CatalogFixture fix;
    void SetUp()    override { fix.setUp(); }
    void TearDown() override { fix.tearDown(); }
};

// ── DDL: DATABASE ─────────────────────────────────────────────────────────

TEST_F(ExecutorTest, CreateDatabase) {
    auto r = runSql("CREATE DATABASE mydb;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_FALSE(r.message.empty());
}

TEST_F(ExecutorTest, CreateDuplicateDatabaseFails) {
    runSql("CREATE DATABASE mydb;");
    auto r = runSql("CREATE DATABASE mydb;");
    EXPECT_FALSE(r.success);
}

TEST_F(ExecutorTest, DropDatabase) {
    runSql("CREATE DATABASE mydb;");
    auto r = runSql("DROP DATABASE mydb;");
    EXPECT_TRUE(r.success) << r.errorMessage;
}

TEST_F(ExecutorTest, DropNonExistentDatabaseFails) {
    auto r = runSql("DROP DATABASE ghost;");
    EXPECT_FALSE(r.success);
}

TEST_F(ExecutorTest, UseDatabase) {
    runSql("CREATE DATABASE mydb;");
    auto r = runSql("USE mydb;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(Catalog::instance().currentDatabase(), "mydb");
}

TEST_F(ExecutorTest, UseNonExistentDatabaseFails) {
    auto r = runSql("USE ghost;");
    EXPECT_FALSE(r.success);
}

// ── DDL: TABLE ────────────────────────────────────────────────────────────

TEST_F(ExecutorTest, CreateTable) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    auto r = runSql("CREATE TABLE products (id INT, title TEXT, price FLOAT, active BOOL);");
    EXPECT_TRUE(r.success) << r.errorMessage;
}

TEST_F(ExecutorTest, CreateTableNamedUsers) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    auto r = runSql("CREATE TABLE users (id INT, name VARCHAR(50));");
    EXPECT_TRUE(r.success) << r.errorMessage;
}

TEST_F(ExecutorTest, CreateDuplicateTableFails) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE t (id INT);");
    auto r = runSql("CREATE TABLE t (id INT);");
    EXPECT_FALSE(r.success);
}

TEST_F(ExecutorTest, DropTable) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE t (id INT);");
    auto r = runSql("DROP TABLE t;");
    EXPECT_TRUE(r.success) << r.errorMessage;
}

TEST_F(ExecutorTest, DropNonExistentTableFails) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    auto r = runSql("DROP TABLE ghost;");
    EXPECT_FALSE(r.success);
}

TEST_F(ExecutorTest, CreateTableWithoutDatabaseFails) {
    auto r = runSql("CREATE TABLE t (id INT);");
    EXPECT_FALSE(r.success);
}

// ── INSERT ────────────────────────────────────────────────────────────────

TEST_F(ExecutorTest, InsertSingleRow) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE users (id INT, name VARCHAR(50), age INT, active BOOL);");
    auto r = runSql("INSERT INTO users VALUES (1, 'Alice', 25, TRUE);");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.affectedRows, 1);
}

TEST_F(ExecutorTest, InsertMultipleRows) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE t (id INT, name TEXT);");
    auto r = runSql("INSERT INTO t VALUES (1, 'a'), (2, 'b'), (3, 'c');");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.affectedRows, 3);
}

TEST_F(ExecutorTest, InsertIntoUsersTable) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE users (id INT, name VARCHAR(50), age INT, active BOOL);");
    auto r = runSql("INSERT INTO users VALUES (1, 'Alice', 25, TRUE);");
    EXPECT_TRUE(r.success) << r.errorMessage;
}

TEST_F(ExecutorTest, InsertWithoutDatabaseFails) {
    auto r = runSql("INSERT INTO t VALUES (1);");
    EXPECT_FALSE(r.success);
}

// ── SELECT ────────────────────────────────────────────────────────────────

TEST_F(ExecutorTest, SelectAllReturnsInsertedRow) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE users (id INT, name VARCHAR(50), age INT, active BOOL);");
    runSql("INSERT INTO users VALUES (1, 'Alice', 25, TRUE);");

    auto r = runSql("SELECT * FROM users;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    ASSERT_EQ(r.rows.size(), 1u);
    ASSERT_EQ(r.rows[0].values.size(), 4u);
}

TEST_F(ExecutorTest, SelectColumnsReturnsOnlyRequested) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE users (id INT, name VARCHAR(50), age INT);");
    runSql("INSERT INTO users VALUES (1, 'Alice', 25);");

    auto r = runSql("SELECT id, name FROM users;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    ASSERT_EQ(r.columns.size(), 2u);
    EXPECT_EQ(r.columns[0], "id");
    EXPECT_EQ(r.columns[1], "name");
    ASSERT_EQ(r.rows[0].values.size(), 2u);
}

TEST_F(ExecutorTest, SelectFromEmptyTableReturnsZeroRows) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE t (id INT);");

    auto r = runSql("SELECT * FROM t;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.rows.size(), 0u);
}

TEST_F(ExecutorTest, SelectWhereEquals) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE users (id INT, name VARCHAR(50), age INT, active BOOL);");
    runSql("INSERT INTO users VALUES (1, 'Alice', 25, TRUE);");
    runSql("INSERT INTO users VALUES (2, 'Bob',   30, FALSE);");

    auto r = runSql("SELECT * FROM users WHERE id = 1;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    ASSERT_EQ(r.rows.size(), 1u);
    EXPECT_EQ(std::get<int32_t>(r.rows[0].values[0]), 1);
}

TEST_F(ExecutorTest, SelectWhereGreaterThan) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE users (id INT, name VARCHAR(50), age INT, active BOOL);");
    runSql("INSERT INTO users VALUES (1, 'Alice', 25, TRUE);");
    runSql("INSERT INTO users VALUES (2, 'Bob',   30, FALSE);");
    runSql("INSERT INTO users VALUES (3, 'Carol', 22, TRUE);");

    auto r = runSql("SELECT * FROM users WHERE age > 24;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.rows.size(), 2u);
}

TEST_F(ExecutorTest, SelectWhereLessThanOrEqual) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE t (id INT, val INT);");
    runSql("INSERT INTO t VALUES (1, 10), (2, 20), (3, 30);");

    auto r = runSql("SELECT * FROM t WHERE val <= 20;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.rows.size(), 2u);
}

TEST_F(ExecutorTest, SelectWhereAndCondition) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE users (id INT, name VARCHAR(50), age INT, active BOOL);");
    runSql("INSERT INTO users VALUES (1, 'Alice', 25, TRUE);");
    runSql("INSERT INTO users VALUES (2, 'Bob',   30, FALSE);");
    runSql("INSERT INTO users VALUES (3, 'Carol', 22, TRUE);");

    auto r = runSql("SELECT * FROM users WHERE age > 20 AND active = TRUE;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.rows.size(), 2u);
}

TEST_F(ExecutorTest, SelectWhereOrCondition) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE t (id INT, val INT);");
    runSql("INSERT INTO t VALUES (1, 5), (2, 15), (3, 25);");

    auto r = runSql("SELECT * FROM t WHERE val = 5 OR val = 25;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.rows.size(), 2u);
}

TEST_F(ExecutorTest, SelectFromNonExistentTableFails) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    auto r = runSql("SELECT * FROM ghost;");
    EXPECT_FALSE(r.success);
}

// ── UPDATE ────────────────────────────────────────────────────────────────

TEST_F(ExecutorTest, UpdateWithWhere) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE users (id INT, name VARCHAR(50), age INT, active BOOL);");
    runSql("INSERT INTO users VALUES (1, 'Alice', 25, TRUE);");
    runSql("INSERT INTO users VALUES (2, 'Bob',   30, FALSE);");

    auto upd = runSql("UPDATE users SET age = 31 WHERE id = 2;");
    EXPECT_TRUE(upd.success) << upd.errorMessage;
    EXPECT_EQ(upd.affectedRows, 1);

    auto sel = runSql("SELECT * FROM users WHERE id = 2;");
    ASSERT_EQ(sel.rows.size(), 1u);
    EXPECT_EQ(std::get<int32_t>(sel.rows[0].values[2]), 31);
}

TEST_F(ExecutorTest, UpdateAllRowsWithoutWhere) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE t (id INT, active BOOL);");
    runSql("INSERT INTO t VALUES (1, TRUE), (2, TRUE), (3, TRUE);");

    auto r = runSql("UPDATE t SET active = FALSE;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.affectedRows, 3);
}

TEST_F(ExecutorTest, UpdateNoMatchingRowsReturnsZero) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE t (id INT, val INT);");
    runSql("INSERT INTO t VALUES (1, 10);");

    auto r = runSql("UPDATE t SET val = 99 WHERE id = 999;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.affectedRows, 0);
}

// ── DELETE ────────────────────────────────────────────────────────────────

TEST_F(ExecutorTest, DeleteWithWhere) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE users (id INT, name VARCHAR(50), age INT, active BOOL);");
    runSql("INSERT INTO users VALUES (1, 'Alice', 25, TRUE);");
    runSql("INSERT INTO users VALUES (2, 'Bob',   30, FALSE);");
    runSql("INSERT INTO users VALUES (3, 'Carol', 22, TRUE);");

    auto del = runSql("DELETE FROM users WHERE active = FALSE;");
    EXPECT_TRUE(del.success) << del.errorMessage;
    EXPECT_EQ(del.affectedRows, 1);

    auto sel = runSql("SELECT * FROM users;");
    EXPECT_EQ(sel.rows.size(), 2u);
}

TEST_F(ExecutorTest, DeleteAllRowsWithoutWhere) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE t (id INT);");
    runSql("INSERT INTO t VALUES (1), (2), (3);");

    auto r = runSql("DELETE FROM t;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.affectedRows, 3);

    auto sel = runSql("SELECT * FROM t;");
    EXPECT_EQ(sel.rows.size(), 0u);
}

TEST_F(ExecutorTest, DeleteNoMatchingRowsReturnsZero) {
    runSql("CREATE DATABASE mydb;");
    runSql("USE mydb;");
    runSql("CREATE TABLE t (id INT);");
    runSql("INSERT INTO t VALUES (1);");

    auto r = runSql("DELETE FROM t WHERE id = 999;");
    EXPECT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.affectedRows, 0);
}
