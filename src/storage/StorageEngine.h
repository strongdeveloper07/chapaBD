#pragma once
#include "Database.h"
#include "../common/Types.h"
#include <string>
#include <vector>
#include <functional>

namespace chapadb {

/**
 * <summary>
 * Движок хранения: работает с бинарными файлами на диске.
 * Поддерживает каталог баз данных, схемы таблиц и данные строк.
 * </summary>
 */
class StorageEngine {
public:
    explicit StorageEngine(std::string dataDir);

    // ── Каталог баз данных ─────────────────────────────────────────────
    std::vector<std::string> listDatabases();
    bool                     databaseExists(const std::string& name);
    void                     createDatabase(const std::string& name);
    void                     dropDatabase(const std::string& name);

    // ── Схема ─────────────────────────────────────────────────────────
    std::vector<TableMeta>   loadSchema(const std::string& dbName);
    void                     saveSchema(const std::string& dbName,
                                        const std::vector<TableMeta>& tables);
    bool                     tableExists(const std::string& dbName, const std::string& tableName);

    // ── Данные строк ───────────────────────────────────────────────────
    std::vector<Row>         readRows(const std::string& dbName,
                                      const TableMeta&   table);

    void                     appendRows(const std::string&       dbName,
                                        const TableMeta&          table,
                                        const std::vector<Row>&   rows);

    /// Перезаписывает файл: строки с predicate(row)==true → помечаются как deleted
    int64_t                  deleteRows(const std::string&                     dbName,
                                        const TableMeta&                        table,
                                        std::function<bool(const Row&)>         predicate);

    /// Перезаписывает строки: для совпавших применяет updater(row)
    int64_t                  updateRows(const std::string&                     dbName,
                                        const TableMeta&                        table,
                                        std::function<bool(const Row&)>         predicate,
                                        std::function<void(Row&)>               updater);

private:
    std::string m_dataDir;

    std::string dbDir(const std::string& dbName) const;
    std::string schemaPath(const std::string& dbName) const;
    std::string tablePath(const std::string& dbName, const std::string& tableName) const;
    std::string catalogPath() const;

    // Сериализация / десериализация значений
    void        writeValue(std::ostream& os, const Value& val, DataType type);
    Value       readValue(std::istream& is, DataType type);

    // Вспомогательные функции бинарного I/O
    static void     writeU8 (std::ostream& os, uint8_t  v);
    static void     writeU32(std::ostream& os, uint32_t v);
    static void     writeU64(std::ostream& os, uint64_t v);
    static uint8_t  readU8  (std::istream& is);
    static uint32_t readU32 (std::istream& is);
    static uint64_t readU64 (std::istream& is);
    static void     writeStr(std::ostream& os, const std::string& s);
    static std::string readStr(std::istream& is);
};

} 
