#include <gtest/gtest.h>
#include "test_helper.h"

using namespace chapadb;
using namespace chapadb::test;

// Фикстура с двумя "сессиями": session1 — создаём данные, session2 — проверяем
class PersistenceTest : public ::testing::Test {
protected:
    std::string dataDir;

    void SetUp() override {
        dataDir = makeTempDir();
    }

    void TearDown() override {
        std::filesystem::remove_all(dataDir);
    }

    // Имитирует запуск сервера: инициализирует Catalog с нужной директорией
    void openSession() {
        Catalog::instance().init(dataDir);
        Catalog::instance().setCurrentDatabase("");
        AuthManager::instance().init(dataDir);
    }
};

// ── Критерий 1: данные сохраняются между запусками ───────────────────────

TEST_F(PersistenceTest, InsertedRowsSurviveRestart) {
    // Сессия 1: создаём БД, таблицу, вставляем строки
    openSession();
    ASSERT_TRUE(runSql("CREATE DATABASE db1;").success);
    ASSERT_TRUE(runSql("USE db1;").success);
    ASSERT_TRUE(runSql("CREATE TABLE accounts (id INT, owner TEXT, balance FLOAT);").success);
    ASSERT_EQ(runSql("INSERT INTO accounts VALUES (1, 'Alice', 1000.0);").affectedRows, 1);
    ASSERT_EQ(runSql("INSERT INTO accounts VALUES (2, 'Bob',   2500.5);").affectedRows, 1);

    // Сессия 2: переинициализируем Catalog с той же директорией
    openSession();
    ASSERT_TRUE(runSql("USE db1;").success);

    auto r = runSql("SELECT * FROM accounts;");
    ASSERT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.rows.size(), 2u);
}

TEST_F(PersistenceTest, TableSchemaSurvivesRestart) {
    openSession();
    ASSERT_TRUE(runSql("CREATE DATABASE db1;").success);
    ASSERT_TRUE(runSql("USE db1;").success);
    ASSERT_TRUE(runSql("CREATE TABLE events (id INT, title VARCHAR(100), active BOOL);").success);

    openSession();
    ASSERT_TRUE(runSql("USE db1;").success);

    // Схема должна быть на месте — INSERT должен работать
    auto r = runSql("INSERT INTO events VALUES (1, 'Meeting', TRUE);");
    EXPECT_TRUE(r.success) << r.errorMessage;
}

TEST_F(PersistenceTest, DatabaseListSurvivesRestart) {
    openSession();
    ASSERT_TRUE(runSql("CREATE DATABASE alpha;").success);
    ASSERT_TRUE(runSql("CREATE DATABASE beta;").success);

    openSession();
    // USE обеих БД должен работать
    EXPECT_TRUE(runSql("USE alpha;").success);
    EXPECT_TRUE(runSql("USE beta;").success);
}

TEST_F(PersistenceTest, UpdatedValuesSurviveRestart) {
    openSession();
    ASSERT_TRUE(runSql("CREATE DATABASE db1;").success);
    ASSERT_TRUE(runSql("USE db1;").success);
    ASSERT_TRUE(runSql("CREATE TABLE counters (id INT, val INT);").success);
    runSql("INSERT INTO counters VALUES (1, 10);");
    runSql("UPDATE counters SET val = 99 WHERE id = 1;");

    openSession();
    ASSERT_TRUE(runSql("USE db1;").success);

    auto r = runSql("SELECT * FROM counters;");
    ASSERT_TRUE(r.success) << r.errorMessage;
    ASSERT_EQ(r.rows.size(), 1u);
    EXPECT_EQ(std::get<int32_t>(r.rows[0].values[1]), 99);
}

TEST_F(PersistenceTest, DeletedRowsNotRestoredAfterRestart) {
    openSession();
    ASSERT_TRUE(runSql("CREATE DATABASE db1;").success);
    ASSERT_TRUE(runSql("USE db1;").success);
    ASSERT_TRUE(runSql("CREATE TABLE items (id INT, name TEXT);").success);
    runSql("INSERT INTO items VALUES (1, 'keep');");
    runSql("INSERT INTO items VALUES (2, 'remove');");
    runSql("DELETE FROM items WHERE id = 2;");

    openSession();
    ASSERT_TRUE(runSql("USE db1;").success);

    auto r = runSql("SELECT * FROM items;");
    ASSERT_TRUE(r.success) << r.errorMessage;
    EXPECT_EQ(r.rows.size(), 1u);
    EXPECT_EQ(std::get<int32_t>(r.rows[0].values[0]), 1);
}

TEST_F(PersistenceTest, DroppedTableNotAccessibleAfterRestart) {
    openSession();
    ASSERT_TRUE(runSql("CREATE DATABASE db1;").success);
    ASSERT_TRUE(runSql("USE db1;").success);
    ASSERT_TRUE(runSql("CREATE TABLE temp (id INT);").success);
    runSql("INSERT INTO temp VALUES (1);");
    ASSERT_TRUE(runSql("DROP TABLE temp;").success);

    openSession();
    ASSERT_TRUE(runSql("USE db1;").success);

    auto r = runSql("SELECT * FROM temp;");
    EXPECT_FALSE(r.success);  // таблица удалена
}

TEST_F(PersistenceTest, MultipleTablesAllSurviveRestart) {
    openSession();
    ASSERT_TRUE(runSql("CREATE DATABASE db1;").success);
    ASSERT_TRUE(runSql("USE db1;").success);
    runSql("CREATE TABLE t1 (id INT, val INT);");
    runSql("CREATE TABLE t2 (id INT, name TEXT);");
    runSql("INSERT INTO t1 VALUES (1, 42);");
    runSql("INSERT INTO t2 VALUES (1, 'hello');");

    openSession();
    ASSERT_TRUE(runSql("USE db1;").success);

    auto r1 = runSql("SELECT * FROM t1;");
    ASSERT_TRUE(r1.success) << r1.errorMessage;
    EXPECT_EQ(r1.rows.size(), 1u);

    auto r2 = runSql("SELECT * FROM t2;");
    ASSERT_TRUE(r2.success) << r2.errorMessage;
    EXPECT_EQ(r2.rows.size(), 1u);
}
