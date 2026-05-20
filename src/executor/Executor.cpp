#include "Executor.h"
#include "../catalog/Catalog.h"
#include "../auth/AuthManager.h"
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>
#include <map>

namespace chapadb {

Executor::Executor() = default;

QueryResult Executor::execute(ASTNode& node) {
    m_result = QueryResult{};
    m_result.success      = true;
    m_result.affectedRows = 0;
    try {
        node.accept(*this);
    } catch (const std::exception& e) {
        m_result.success      = false;
        m_result.errorMessage = e.what();
    }
    return m_result;
}

const std::string& Executor::requireDb() {
    if (!Catalog::instance().hasCurrentDatabase()) {
        throw std::runtime_error("Не выбрана база данных. Используйте USE <имя>");
    }
    return Catalog::instance().currentDatabase();
}

TableMeta Executor::requireTable(const std::string& dbName, const std::string& tableName) {
    auto& storage = Catalog::instance().storage();
    auto tables   = storage.loadSchema(dbName);
    for (auto& t : tables) {
        if (t.name == tableName) return t;
    }
    throw std::runtime_error("Таблица '" + tableName + "' не найдена в базе '" + dbName + "'");
}

bool Executor::matchesWhere(const Row& row, const TableMeta& meta,
                             const std::unique_ptr<ConditionNode>& where) {
    if (!where) return true;
    return where->evaluate(row, meta.columns);
}

// ─────────────────────────── DDL ─────────────────────────────────────

void Executor::visit(CreateDatabaseStatement& stmt) {
    Catalog::instance().storage().createDatabase(stmt.dbName);
    m_result.message = "База данных '" + stmt.dbName + "' создана";
}

void Executor::visit(DropDatabaseStatement& stmt) {
    auto& cat = Catalog::instance();
    cat.storage().dropDatabase(stmt.dbName);
    // Сбрасываем активную БД, если удалили её
    if (cat.currentDatabase() == stmt.dbName) {
        cat.setCurrentDatabase("");
    }
    m_result.message = "База данных '" + stmt.dbName + "' удалена";
}

void Executor::visit(UseDatabaseStatement& stmt) {
    auto& storage = Catalog::instance().storage();
    if (!storage.databaseExists(stmt.dbName)) {
        throw std::runtime_error("База данных '" + stmt.dbName + "' не существует");
    }
    Catalog::instance().setCurrentDatabase(stmt.dbName);
    m_result.message = "Активная база данных: '" + stmt.dbName + "'";
}

void Executor::visit(CreateTableStatement& stmt) {
    const std::string& dbName = requireDb();
    auto& storage = Catalog::instance().storage();

    if (storage.tableExists(dbName, stmt.tableName)) {
        throw std::runtime_error("Таблица '" + stmt.tableName + "' уже существует");
    }

    auto tables = storage.loadSchema(dbName);
    TableMeta meta;
    meta.name    = stmt.tableName;
    meta.columns = stmt.columns;
    tables.push_back(std::move(meta));
    storage.saveSchema(dbName, tables);

    m_result.message = "Таблица '" + stmt.tableName + "' создана";
}

void Executor::visit(DropTableStatement& stmt) {
    const std::string& dbName = requireDb();
    auto& storage = Catalog::instance().storage();

    auto tables = storage.loadSchema(dbName);
    auto it = std::remove_if(tables.begin(), tables.end(),
                              [&](const TableMeta& t){ return t.name == stmt.tableName; });
    if (it == tables.end()) {
        throw std::runtime_error("Таблица '" + stmt.tableName + "' не найдена");
    }
    tables.erase(it, tables.end());
    storage.saveSchema(dbName, tables);

    // Удаляем файл данных, если есть
    std::string path = "data/" + dbName + "/" + stmt.tableName + ".dat";
    // Хранилище само управляет путями; здесь просто сохраняем схему
    m_result.message = "Таблица '" + stmt.tableName + "' удалена";
}

// ─────────────────────────── Вспомогательные функции SELECT ──────────

/// Конвертирует Value в double для агрегатных вычислений
static double toDouble(const Value& v) {
    if (std::holds_alternative<int32_t>(v))  return static_cast<double>(std::get<int32_t>(v));
    if (std::holds_alternative<double>(v))   return std::get<double>(v);
    if (std::holds_alternative<bool>(v))     return std::get<bool>(v) ? 1.0 : 0.0;
    return 0.0;
}

/// Сравнивает два Value для сортировки
static int compareForSort(const Value& a, const Value& b) {
    if (std::holds_alternative<std::monostate>(a)) return std::holds_alternative<std::monostate>(b) ? 0 : -1;
    if (std::holds_alternative<std::monostate>(b)) return 1;
    if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
        const auto& sa = std::get<std::string>(a);
        const auto& sb = std::get<std::string>(b);
        if (sa < sb) return -1;
        if (sa > sb) return  1;
        return 0;
    }
    double da = toDouble(a), db = toDouble(b);
    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}

/// Находит индекс столбца по имени
static int findColIndex(const std::vector<ColumnSchema>& cols, const std::string& name) {
    for (int i = 0; i < static_cast<int>(cols.size()); ++i) {
        if (cols[i].name == name) return i;
    }
    return -1;
}

/// Строковое представление агрегата для заголовка
static std::string aggColName(AggFunc f, const std::string& arg) {
    switch (f) {
        case AggFunc::COUNT: return "count(" + arg + ")";
        case AggFunc::SUM:   return "sum(" + arg + ")";
        case AggFunc::AVG:   return "avg(" + arg + ")";
        case AggFunc::MIN:   return "min(" + arg + ")";
        case AggFunc::MAX:   return "max(" + arg + ")";
        default:             return arg;
    }
}

/// Вычисляет агрегат по набору строк
static Value calcAgg(AggFunc f, const std::string& arg,
                     const std::vector<Row>& rows,
                     const std::vector<ColumnSchema>& schema) {
    if (f == AggFunc::COUNT) {
        return Value{static_cast<int32_t>(rows.size())};
    }
    int idx = findColIndex(schema, arg);
    if (idx < 0) throw std::runtime_error("Столбец '" + arg + "' не найден");

    if (rows.empty()) return Value{std::monostate{}};

    if (f == AggFunc::MIN) {
        Value minVal = rows[0].values[idx];
        for (size_t i = 1; i < rows.size(); ++i) {
            if (compareForSort(rows[i].values[idx], minVal) < 0) minVal = rows[i].values[idx];
        }
        return minVal;
    }
    if (f == AggFunc::MAX) {
        Value maxVal = rows[0].values[idx];
        for (size_t i = 1; i < rows.size(); ++i) {
            if (compareForSort(rows[i].values[idx], maxVal) > 0) maxVal = rows[i].values[idx];
        }
        return maxVal;
    }

    double sum = 0.0;
    for (const auto& row : rows) sum += toDouble(row.values[idx]);
    if (f == AggFunc::SUM) return Value{sum};
    if (f == AggFunc::AVG) return Value{sum / static_cast<double>(rows.size())};

    return Value{std::monostate{}};
}

// ─────────────────────────── SELECT ──────────────────────────────────

void Executor::visit(SelectStatement& stmt) {
    const std::string& dbName = requireDb();
    TableMeta meta = requireTable(dbName, stmt.tableName);

    // Читаем строки основной таблицы
    auto allRows = Catalog::instance().storage().readRows(dbName, meta);

    // JOIN: соединяем с дополнительными таблицами (nested loop)
    std::vector<ColumnSchema> joinedSchema = meta.columns;
    std::vector<Row> joinedRows;

    if (stmt.joins.empty()) {
        joinedRows = allRows;
    } else {
        // Одна итерация JOIN (поддерживаем один JOIN)
        for (const auto& jc : stmt.joins) {
            TableMeta jMeta = requireTable(dbName, jc.tableName);
            auto jRows = Catalog::instance().storage().readRows(dbName, jMeta);

            int leftIdx  = findColIndex(joinedSchema, jc.leftCol);
            int rightIdx = findColIndex(jMeta.columns, jc.rightCol);
            if (leftIdx < 0)  throw std::runtime_error("Столбец ON не найден: " + jc.leftCol);
            if (rightIdx < 0) throw std::runtime_error("Столбец ON не найден: " + jc.rightCol);

            std::vector<Row> result;
            for (const auto& lRow : (joinedRows.empty() ? allRows : joinedRows)) {
                for (const auto& rRow : jRows) {
                    if (compareForSort(lRow.values[leftIdx], rRow.values[rightIdx]) == 0) {
                        Row combined;
                        combined.values = lRow.values;
                        combined.values.insert(combined.values.end(),
                                               rRow.values.begin(), rRow.values.end());
                        result.push_back(std::move(combined));
                    }
                }
            }
            for (const auto& col : jMeta.columns) joinedSchema.push_back(col);
            joinedRows = std::move(result);
        }
    }

    std::vector<Row> filtered;
    TableMeta joinedMeta;
    joinedMeta.columns = joinedSchema;
    for (const auto& row : joinedRows) {
        if (matchesWhere(row, joinedMeta, stmt.where)) filtered.push_back(row);
    }

    // Определяем есть ли агрегатные функции
    bool hasAgg = false;
    for (const auto& item : stmt.items) {
        if (item.aggFunc != AggFunc::NONE) { hasAgg = true; break; }
    }

    if (!hasAgg && stmt.groupBy.empty()) {
        std::vector<int>         colIndexes;
        std::vector<std::string> colNames;

        if (stmt.selectAll) {
            for (int i = 0; i < static_cast<int>(joinedSchema.size()); ++i) {
                colIndexes.push_back(i);
                colNames.push_back(joinedSchema[i].name);
            }
        } else {
            for (const auto& item : stmt.items) {
                int idx = findColIndex(joinedSchema, item.colName);
                if (idx < 0) throw std::runtime_error("Столбец '" + item.colName + "' не найден");
                colIndexes.push_back(idx);
                colNames.push_back(item.colName);
            }
        }

        m_result.columns = colNames;
        for (const auto& row : filtered) {
            Row outRow;
            for (int idx : colIndexes) outRow.values.push_back(row.values[idx]);
            m_result.rows.push_back(std::move(outRow));
        }
    } else if (!stmt.groupBy.empty() || hasAgg) {
        // Путь с агрегатами / GROUP BY
        // Определяем заголовки результата
        std::vector<std::string> outCols;
        for (const auto& item : stmt.items) {
            if (item.aggFunc == AggFunc::NONE)
                outCols.push_back(item.colName);
            else
                outCols.push_back(aggColName(item.aggFunc, item.aggArg));
        }
        m_result.columns = outCols;

        if (stmt.groupBy.empty()) {
            // Агрегат без GROUP BY — одна выходная строка
            Row outRow;
            for (const auto& item : stmt.items) {
                if (item.aggFunc == AggFunc::NONE) {
                    int idx = findColIndex(joinedSchema, item.colName);
                    if (idx < 0) throw std::runtime_error("Столбец '" + item.colName + "' не найден");
                    outRow.values.push_back(filtered.empty() ? Value{std::monostate{}} : filtered[0].values[idx]);
                } else {
                    outRow.values.push_back(calcAgg(item.aggFunc, item.aggArg, filtered, joinedSchema));
                }
            }
            m_result.rows.push_back(std::move(outRow));
        } else {
            // GROUP BY: группируем строки
            std::vector<int> groupIdxs;
            for (const auto& gcol : stmt.groupBy) {
                int idx = findColIndex(joinedSchema, gcol);
                if (idx < 0) throw std::runtime_error("Столбец GROUP BY не найден: " + gcol);
                groupIdxs.push_back(idx);
            }

            // Собираем группы в map (ключ — вектор значений по group-столбцам)
            using GroupKey = std::vector<std::string>;
            std::vector<GroupKey> keyOrder;
            std::map<GroupKey, std::vector<Row>> groups;

            auto makeKey = [&](const Row& row) {
                GroupKey key;
                for (int gi : groupIdxs) {
                    std::visit([&](const auto& v) {
                        using T = std::decay_t<decltype(v)>;
                        if      constexpr (std::is_same_v<T, std::monostate>) key.push_back("NULL");
                        else if constexpr (std::is_same_v<T, bool>)           key.push_back(v ? "1" : "0");
                        else if constexpr (std::is_same_v<T, int32_t>)        key.push_back(std::to_string(v));
                        else if constexpr (std::is_same_v<T, double>)         key.push_back(std::to_string(v));
                        else                                                    key.push_back(std::get<std::string>(row.values[gi]));
                    }, row.values[gi]);
                }
                return key;
            };

            for (const auto& row : filtered) {
                auto key = makeKey(row);
                if (groups.find(key) == groups.end()) keyOrder.push_back(key);
                groups[key].push_back(row);
            }

            for (const auto& key : keyOrder) {
                const auto& groupRows = groups[key];
                Row outRow;
                for (const auto& item : stmt.items) {
                    if (item.aggFunc == AggFunc::NONE) {
                        int idx = findColIndex(joinedSchema, item.colName);
                        if (idx < 0) throw std::runtime_error("Столбец '" + item.colName + "' не найден");
                        outRow.values.push_back(groupRows[0].values[idx]);
                    } else {
                        outRow.values.push_back(calcAgg(item.aggFunc, item.aggArg, groupRows, joinedSchema));
                    }
                }
                m_result.rows.push_back(std::move(outRow));
            }
        }
    }

    // ORDER BY
    if (!stmt.orderBy.empty()) {
        // Строим индексы столбцов сортировки
        std::vector<std::pair<int, bool>> sortKeys; 
        for (const auto& ob : stmt.orderBy) {
            // Ищем в заголовках результата
            int idx = -1;
            for (int i = 0; i < static_cast<int>(m_result.columns.size()); ++i) {
                if (m_result.columns[i] == ob.column) { idx = i; break; }
            }
            if (idx < 0) {
                // Ищем в схеме таблицы (если selectAll)
                idx = findColIndex(joinedSchema, ob.column);
                if (idx < 0) throw std::runtime_error("Столбец ORDER BY не найден: " + ob.column);
            }
            sortKeys.emplace_back(idx, ob.ascending);
        }

        std::stable_sort(m_result.rows.begin(), m_result.rows.end(),
            [&](const Row& a, const Row& b) {
                for (const auto& [idx, asc] : sortKeys) {
                    if (idx >= static_cast<int>(a.values.size())) continue;
                    int cmp = compareForSort(a.values[idx], b.values[idx]);
                    if (cmp != 0) return asc ? cmp < 0 : cmp > 0;
                }
                return false;
            });
    }

    if (stmt.limitCount >= 0) {
        int64_t offset = stmt.limitOffset;
        if (offset > 0) {
            auto skip = m_result.rows.begin() +
                        std::min(offset, static_cast<int64_t>(m_result.rows.size()));
            m_result.rows.erase(m_result.rows.begin(), skip);
        }
        if (stmt.limitCount < static_cast<int64_t>(m_result.rows.size())) {
            m_result.rows.erase(m_result.rows.begin() + stmt.limitCount, m_result.rows.end());
        }
    }

    m_result.affectedRows = static_cast<int64_t>(m_result.rows.size());
}

// ─────────────────────────── INSERT ──────────────────────────────────

void Executor::visit(InsertStatement& stmt) {
    const std::string& dbName = requireDb();
    TableMeta meta = requireTable(dbName, stmt.tableName);

    // Сопоставляем столбцы из INSERT со схемой
    std::vector<int> colMapping; 
    if (!stmt.columns.empty()) {
        for (const auto& name : stmt.columns) {
            bool found = false;
            for (int i = 0; i < static_cast<int>(meta.columns.size()); ++i) {
                if (meta.columns[i].name == name) {
                    colMapping.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error("Столбец '" + name + "' не найден в таблице '" +
                                         stmt.tableName + "'");
            }
        }
    } else {
        for (int i = 0; i < static_cast<int>(meta.columns.size()); ++i) {
            colMapping.push_back(i);
        }
    }

    std::vector<Row> newRows;
    for (const auto& valueRow : stmt.values) {
        if (valueRow.size() != colMapping.size()) {
            throw std::runtime_error("Несоответствие количества значений и столбцов");
        }
        // Создаём строку с NULL во всех столбцах
        Row row;
        row.values.assign(meta.columns.size(), Value{std::monostate{}});

        for (size_t i = 0; i < colMapping.size(); ++i) {
            row.values[colMapping[i]] = valueRow[i];
        }
        newRows.push_back(std::move(row));
    }

    Catalog::instance().storage().appendRows(dbName, meta, newRows);
    m_result.affectedRows = static_cast<int64_t>(newRows.size());
}

// ─────────────────────────── UPDATE ──────────────────────────────────

void Executor::visit(UpdateStatement& stmt) {
    const std::string& dbName = requireDb();
    TableMeta meta = requireTable(dbName, stmt.tableName);

    // Проверяем, что все столбцы из SET существуют
    for (const auto& [colName, _] : stmt.assignments) {
        bool found = false;
        for (const auto& col : meta.columns) {
            if (col.name == colName) { found = true; break; }
        }
        if (!found) {
            throw std::runtime_error("Столбец '" + colName + "' не найден в таблице '" +
                                     stmt.tableName + "'");
        }
    }

    std::vector<std::pair<int, Value>> updates;
    for (const auto& [colName, val] : stmt.assignments) {
        for (int i = 0; i < static_cast<int>(meta.columns.size()); ++i) {
            if (meta.columns[i].name == colName) {
                updates.emplace_back(i, val);
                break;
            }
        }
    }

    const auto& whereRef = stmt.where;
    auto predicate = [&](const Row& row) -> bool {
        return matchesWhere(row, meta, whereRef);
    };

    auto updater = [&](Row& row) {
        for (const auto& [idx, val] : updates) {
            row.values[idx] = val;
        }
    };

    m_result.affectedRows = Catalog::instance().storage().updateRows(dbName, meta, predicate, updater);
}

// ─────────────────────────── DELETE ──────────────────────────────────

void Executor::visit(DeleteStatement& stmt) {
    const std::string& dbName = requireDb();
    TableMeta meta = requireTable(dbName, stmt.tableName);

    const auto& whereRef = stmt.where;
    auto predicate = [&](const Row& row) -> bool {
        return matchesWhere(row, meta, whereRef);
    };

    m_result.affectedRows = Catalog::instance().storage().deleteRows(dbName, meta, predicate);
}

// ─────────────────────────── Управление пользователями ───────────────

void Executor::visit(CreateUserStatement& stmt) {
    UserRole role = UserRole::READWRITE;
    if (stmt.role == "ADMIN")     role = UserRole::ADMIN;
    if (stmt.role == "READONLY")  role = UserRole::READONLY;
    AuthManager::instance().createUser(stmt.username, stmt.password, role);
    m_result.message = "Пользователь '" + stmt.username + "' создан";
}

void Executor::visit(DropUserStatement& stmt) {
    AuthManager::instance().dropUser(stmt.username);
    m_result.message = "Пользователь '" + stmt.username + "' удалён";
}

void Executor::visit(ShowUsersStatement& stmt) {
    auto users = AuthManager::instance().listUsers();
    m_result.columns = {"username", "role"};
    for (const auto& u : users) {
        Row row;
        row.values.push_back(Value{u.name});
        std::string roleStr;
        switch (u.role) {
            case UserRole::ADMIN:     roleStr = "ADMIN";     break;
            case UserRole::READWRITE: roleStr = "READWRITE"; break;
            case UserRole::READONLY:  roleStr = "READONLY";  break;
        }
        row.values.push_back(Value{roleStr});
        m_result.rows.push_back(std::move(row));
    }
    m_result.affectedRows = static_cast<int64_t>(m_result.rows.size());
}

void Executor::visit(AuthStatement& stmt) {
    if (AuthManager::instance().authenticate(stmt.username, stmt.password)) {
        m_result.message = "Аутентификация успешна. Добро пожаловать, " + stmt.username + "!";
    } else {
        throw std::runtime_error("Неверное имя пользователя или пароль");
    }
}

void Executor::visit(GrantStatement& stmt) {
    const std::string& dbName = requireDb();
    AuthManager::instance().grantPrivilege(stmt.username, dbName, stmt.tableName,
                                            stmt.sel, stmt.ins, stmt.upd, stmt.del);
    m_result.message = "Привилегии выданы пользователю '" + stmt.username + "'";
}

void Executor::visit(RevokeStatement& stmt) {
    const std::string& dbName = requireDb();
    AuthManager::instance().revokePrivilege(stmt.username, dbName, stmt.tableName,
                                             stmt.sel, stmt.ins, stmt.upd, stmt.del);
    m_result.message = "Привилегии отозваны у пользователя '" + stmt.username + "'";
}

} 
