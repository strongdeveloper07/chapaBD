#pragma once
#include "catalog/Catalog.h"
#include "auth/AuthManager.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "executor/Executor.h"
#include "common/Types.h"
#include <filesystem>
#include <random>
#include <string>

namespace chapadb::test {

inline std::string makeTempDir() {
    auto base = std::filesystem::temp_directory_path() / "chapadb_test";
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dis(100000, 999999);
    std::string path = base.string() + "_" + std::to_string(dis(gen));
    std::filesystem::create_directories(path);
    return path;
}

inline QueryResult runSql(const std::string& sql) {
    try {
        Lexer  lex(sql);
        auto   tokens = lex.tokenize();
        Parser p(std::move(tokens));
        auto   ast = p.parse();
        Executor exec;
        return exec.execute(*ast);
    } catch (const std::exception& e) {
        QueryResult r;
        r.success      = false;
        r.errorMessage = e.what();
        return r;
    }
}

// Фикстура для тестов, которым нужен изолированный Catalog
struct CatalogFixture {
    std::string dataDir;

    void setUp() {
        dataDir = makeTempDir();
        Catalog::instance().init(dataDir);
        Catalog::instance().setCurrentDatabase("");
        AuthManager::instance().init(dataDir);
    }

    void tearDown() {
        std::filesystem::remove_all(dataDir);
    }
};

} // namespace chapadb::test
