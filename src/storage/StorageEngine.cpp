#include "StorageEngine.h"
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;

namespace chapadb {

// Магические числа файлов
static constexpr uint32_t MAGIC_CATALOG = 0xCBD1;
static constexpr uint32_t MAGIC_SCHEMA  = 0xCBD2;
static constexpr uint32_t MAGIC_TABLE   = 0xCBD3;

StorageEngine::StorageEngine(std::string dataDir)
    : m_dataDir(std::move(dataDir))
{
    fs::create_directories(m_dataDir);
}

std::string StorageEngine::dbDir(const std::string& dbName) const {
    return m_dataDir + "/" + dbName;
}

std::string StorageEngine::catalogPath() const {
    return m_dataDir + "/catalog.dat";
}

std::string StorageEngine::schemaPath(const std::string& dbName) const {
    return dbDir(dbName) + "/schema.dat";
}

std::string StorageEngine::tablePath(const std::string& dbName, const std::string& tableName) const {
    return dbDir(dbName) + "/" + tableName + ".dat";
}

// ─────────────────────────── Бинарные I/O ────────────────────────────

void StorageEngine::writeU8(std::ostream& os, uint8_t v) {
    os.write(reinterpret_cast<const char*>(&v), 1);
}

void StorageEngine::writeU32(std::ostream& os, uint32_t v) {
    // Little-endian хранение
    os.write(reinterpret_cast<const char*>(&v), 4);
}

void StorageEngine::writeU64(std::ostream& os, uint64_t v) {
    os.write(reinterpret_cast<const char*>(&v), 8);
}

uint8_t StorageEngine::readU8(std::istream& is) {
    uint8_t v = 0;
    is.read(reinterpret_cast<char*>(&v), 1);
    return v;
}

uint32_t StorageEngine::readU32(std::istream& is) {
    uint32_t v = 0;
    is.read(reinterpret_cast<char*>(&v), 4);
    return v;
}

uint64_t StorageEngine::readU64(std::istream& is) {
    uint64_t v = 0;
    is.read(reinterpret_cast<char*>(&v), 8);
    return v;
}

void StorageEngine::writeStr(std::ostream& os, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    writeU32(os, len);
    os.write(s.data(), len);
}

std::string StorageEngine::readStr(std::istream& is) {
    uint32_t len = readU32(is);
    std::string s(len, '\0');
    is.read(s.data(), len);
    return s;
}

// ─────────────────────────── Каталог ─────────────────────────────────

std::vector<std::string> StorageEngine::listDatabases() {
    std::vector<std::string> result;
    std::string path = catalogPath();
    if (!fs::exists(path)) return result;

    std::ifstream f(path, std::ios::binary);
    uint32_t magic = readU32(f);
    if (magic != MAGIC_CATALOG) throw std::runtime_error("Повреждён catalog.dat");

    uint32_t count = readU32(f);
    for (uint32_t i = 0; i < count; ++i) {
        result.push_back(readStr(f));
    }
    return result;
}

bool StorageEngine::databaseExists(const std::string& name) {
    auto dbs = listDatabases();
    return std::find(dbs.begin(), dbs.end(), name) != dbs.end();
}

void StorageEngine::createDatabase(const std::string& name) {
    if (databaseExists(name)) {
        throw std::runtime_error("База данных '" + name + "' уже существует");
    }
    fs::create_directories(dbDir(name));

    auto dbs = listDatabases();
    dbs.push_back(name);

    std::ofstream f(catalogPath(), std::ios::binary | std::ios::trunc);
    writeU32(f, MAGIC_CATALOG);
    writeU32(f, static_cast<uint32_t>(dbs.size()));
    for (const auto& db : dbs) writeStr(f, db);
}

void StorageEngine::dropDatabase(const std::string& name) {
    if (!databaseExists(name)) {
        throw std::runtime_error("База данных '" + name + "' не существует");
    }
    fs::remove_all(dbDir(name));

    auto dbs = listDatabases();
    dbs.erase(std::remove(dbs.begin(), dbs.end(), name), dbs.end());

    std::ofstream f(catalogPath(), std::ios::binary | std::ios::trunc);
    writeU32(f, MAGIC_CATALOG);
    writeU32(f, static_cast<uint32_t>(dbs.size()));
    for (const auto& db : dbs) writeStr(f, db);
}

// ─────────────────────────── Схема ───────────────────────────────────

std::vector<TableMeta> StorageEngine::loadSchema(const std::string& dbName) {
    std::vector<TableMeta> tables;
    std::string path = schemaPath(dbName);
    if (!fs::exists(path)) return tables;

    std::ifstream f(path, std::ios::binary);
    uint32_t magic = readU32(f);
    if (magic != MAGIC_SCHEMA) throw std::runtime_error("Повреждён schema.dat для '" + dbName + "'");

    uint32_t tableCount = readU32(f);
    for (uint32_t t = 0; t < tableCount; ++t) {
        TableMeta meta;
        meta.name = readStr(f);
        uint32_t colCount = readU32(f);
        for (uint32_t c = 0; c < colCount; ++c) {
            ColumnSchema col;
            col.name  = readStr(f);
            col.type  = static_cast<DataType>(readU8(f));
            col.param = readU32(f);
            meta.columns.push_back(std::move(col));
        }
        tables.push_back(std::move(meta));
    }
    return tables;
}

void StorageEngine::saveSchema(const std::string& dbName, const std::vector<TableMeta>& tables) {
    std::ofstream f(schemaPath(dbName), std::ios::binary | std::ios::trunc);
    writeU32(f, MAGIC_SCHEMA);
    writeU32(f, static_cast<uint32_t>(tables.size()));
    for (const auto& t : tables) {
        writeStr(f, t.name);
        writeU32(f, static_cast<uint32_t>(t.columns.size()));
        for (const auto& col : t.columns) {
            writeStr(f, col.name);
            writeU8 (f, static_cast<uint8_t>(col.type));
            writeU32(f, col.param);
        }
    }
}

bool StorageEngine::tableExists(const std::string& dbName, const std::string& tableName) {
    auto tables = loadSchema(dbName);
    for (const auto& t : tables) {
        if (t.name == tableName) return true;
    }
    return false;
}

// ─────────────────────────── Данные строк ────────────────────────────

void StorageEngine::writeValue(std::ostream& os, const Value& val, DataType type) {
    if (std::holds_alternative<std::monostate>(val)) {
        writeU8(os, 1); // isNull
        return;
    }
    writeU8(os, 0); // не NULL

    switch (type) {
        case DataType::INT: {
            int32_t v = std::get<int32_t>(val);
            os.write(reinterpret_cast<const char*>(&v), 4);
            break;
        }
        case DataType::FLOAT: {
            double v = std::get<double>(val);
            os.write(reinterpret_cast<const char*>(&v), 8);
            break;
        }
        case DataType::BOOL: {
            writeU8(os, std::get<bool>(val) ? 1 : 0);
            break;
        }
        case DataType::TEXT:
        case DataType::VARCHAR: {
            writeStr(os, std::get<std::string>(val));
            break;
        }
    }
}

Value StorageEngine::readValue(std::istream& is, DataType type) {
    uint8_t isNull = readU8(is);
    if (isNull) return Value{std::monostate{}};

    switch (type) {
        case DataType::INT: {
            int32_t v = 0;
            is.read(reinterpret_cast<char*>(&v), 4);
            return Value{v};
        }
        case DataType::FLOAT: {
            double v = 0.0;
            is.read(reinterpret_cast<char*>(&v), 8);
            return Value{v};
        }
        case DataType::BOOL: {
            return Value{static_cast<bool>(readU8(is))};
        }
        case DataType::TEXT:
        case DataType::VARCHAR: {
            return Value{readStr(is)};
        }
    }
    return Value{std::monostate{}};
}

std::vector<Row> StorageEngine::readRows(const std::string& dbName, const TableMeta& table) {
    std::vector<Row> rows;
    std::string path = tablePath(dbName, table.name);
    if (!fs::exists(path)) return rows;

    std::ifstream f(path, std::ios::binary);
    uint32_t magic = readU32(f);
    if (magic != MAGIC_TABLE) {
        throw std::runtime_error("Повреждён файл таблицы '" + table.name + "'");
    }

    uint64_t total = readU64(f);
    for (uint64_t i = 0; i < total; ++i) {
        uint8_t deleted = readU8(f);
        Row row;
        for (const auto& col : table.columns) {
            row.values.push_back(readValue(f, col.type));
        }
        if (!deleted) rows.push_back(std::move(row));
    }
    return rows;
}

void StorageEngine::appendRows(const std::string& dbName, const TableMeta& table,
                                const std::vector<Row>& newRows) {
    std::string path = tablePath(dbName, table.name);

    // Считываем все существующие записи (включая помеченные удалёнными)
    struct RawRecord { bool deleted; Row row; };
    std::vector<RawRecord> existing;

    if (fs::exists(path)) {
        std::ifstream f(path, std::ios::binary);
        uint32_t magic = readU32(f);
        if (magic != MAGIC_TABLE) {
            throw std::runtime_error("Повреждён файл таблицы '" + table.name + "'");
        }
        uint64_t total = readU64(f);
        for (uint64_t i = 0; i < total; ++i) {
            uint8_t del = readU8(f);
            Row row;
            for (const auto& col : table.columns) {
                row.values.push_back(readValue(f, col.type));
            }
            existing.push_back({static_cast<bool>(del), std::move(row)});
        }
    }

    // Дописываем новые записи
    uint64_t total = existing.size() + newRows.size();
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    writeU32(f, MAGIC_TABLE);
    writeU64(f, total);

    for (const auto& rec : existing) {
        writeU8(f, rec.deleted ? 1 : 0);
        for (size_t c = 0; c < table.columns.size(); ++c) {
            writeValue(f, rec.row.values[c], table.columns[c].type);
        }
    }
    for (const auto& row : newRows) {
        writeU8(f, 0); // не удалена
        for (size_t c = 0; c < table.columns.size(); ++c) {
            writeValue(f, row.values[c], table.columns[c].type);
        }
    }
}

int64_t StorageEngine::deleteRows(const std::string& dbName, const TableMeta& table,
                                   std::function<bool(const Row&)> predicate) {
    std::string path = tablePath(dbName, table.name);
    if (!fs::exists(path)) return 0;

    // Читаем все записи
    struct RawRecord { bool deleted; Row row; };
    std::vector<RawRecord> records;

    {
        std::ifstream f(path, std::ios::binary);
        uint32_t magic = readU32(f);
        if (magic != MAGIC_TABLE) {
            throw std::runtime_error("Повреждён файл таблицы '" + table.name + "'");
        }
        uint64_t total = readU64(f);
        for (uint64_t i = 0; i < total; ++i) {
            uint8_t del = readU8(f);
            Row row;
            for (const auto& col : table.columns) {
                row.values.push_back(readValue(f, col.type));
            }
            records.push_back({static_cast<bool>(del), std::move(row)});
        }
    }

    int64_t affected = 0;
    for (auto& rec : records) {
        if (!rec.deleted && predicate(rec.row)) {
            rec.deleted = true;
            ++affected;
        }
    }

    // Перезаписываем файл
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    writeU32(f, MAGIC_TABLE);
    writeU64(f, static_cast<uint64_t>(records.size()));
    for (const auto& rec : records) {
        writeU8(f, rec.deleted ? 1 : 0);
        for (size_t c = 0; c < table.columns.size(); ++c) {
            writeValue(f, rec.row.values[c], table.columns[c].type);
        }
    }
    return affected;
}

int64_t StorageEngine::updateRows(const std::string& dbName, const TableMeta& table,
                                   std::function<bool(const Row&)> predicate,
                                   std::function<void(Row&)>       updater) {
    std::string path = tablePath(dbName, table.name);
    if (!fs::exists(path)) return 0;

    struct RawRecord { bool deleted; Row row; };
    std::vector<RawRecord> records;

    {
        std::ifstream f(path, std::ios::binary);
        uint32_t magic = readU32(f);
        if (magic != MAGIC_TABLE) {
            throw std::runtime_error("Повреждён файл таблицы '" + table.name + "'");
        }
        uint64_t total = readU64(f);
        for (uint64_t i = 0; i < total; ++i) {
            uint8_t del = readU8(f);
            Row row;
            for (const auto& col : table.columns) {
                row.values.push_back(readValue(f, col.type));
            }
            records.push_back({static_cast<bool>(del), std::move(row)});
        }
    }

    int64_t affected = 0;
    for (auto& rec : records) {
        if (!rec.deleted && predicate(rec.row)) {
            updater(rec.row);
            ++affected;
        }
    }

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    writeU32(f, MAGIC_TABLE);
    writeU64(f, static_cast<uint64_t>(records.size()));
    for (const auto& rec : records) {
        writeU8(f, rec.deleted ? 1 : 0);
        for (size_t c = 0; c < table.columns.size(); ++c) {
            writeValue(f, rec.row.values[c], table.columns[c].type);
        }
    }
    return affected;
}

} // namespace chapadb
