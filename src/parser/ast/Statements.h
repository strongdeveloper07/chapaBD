#pragma once
#include "ASTNode.h"
#include "../../common/Types.h"
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace chapadb {

// ─────────────────────────── Условия WHERE ───────────────────────────

enum class CompareOp { EQ, NEQ, LT, GT, LE, GE };

/// Базовый узел условия
struct ConditionNode {
    virtual ~ConditionNode() = default;
    virtual bool evaluate(const Row& row, const std::vector<ColumnSchema>& schema) const = 0;
};

/// Простое сравнение: column op value
struct ComparisonCondition : ConditionNode {
    std::string name;
    CompareOp   op;
    Value       value;

    bool evaluate(const Row& row, const std::vector<ColumnSchema>& schema) const override;
};

struct AndCondition : ConditionNode {
    std::unique_ptr<ConditionNode> left;
    std::unique_ptr<ConditionNode> right;

    bool evaluate(const Row& row, const std::vector<ColumnSchema>& schema) const override {
        return left->evaluate(row, schema) && right->evaluate(row, schema);
    }
};

struct OrCondition : ConditionNode {
    std::unique_ptr<ConditionNode> left;
    std::unique_ptr<ConditionNode> right;

    bool evaluate(const Row& row, const std::vector<ColumnSchema>& schema) const override {
        return left->evaluate(row, schema) || right->evaluate(row, schema);
    }
};

struct NotCondition : ConditionNode {
    std::unique_ptr<ConditionNode> inner;

    bool evaluate(const Row& row, const std::vector<ColumnSchema>& schema) const override {
        return !inner->evaluate(row, schema);
    }
};

// ─────────────────────────── DDL ─────────────────────────────────────

struct CreateDatabaseStatement : ASTNode {
    std::string dbName;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct DropDatabaseStatement : ASTNode {
    std::string dbName;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct UseDatabaseStatement : ASTNode {
    std::string dbName;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct CreateTableStatement : ASTNode {
    std::string              tableName;
    std::vector<ColumnSchema> columns;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct DropTableStatement : ASTNode {
    std::string tableName;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

// ─────────────────────────── DML ─────────────────────────────────────

enum class AggFunc { NONE, COUNT, SUM, AVG, MIN, MAX };

/// Один элемент проекции SELECT: столбец или агрегатная функция
struct SelectItem {
    std::string colName;               // обычный столбец (aggFunc == NONE)
    AggFunc     aggFunc = AggFunc::NONE;
    std::string aggArg;                // аргумент агрегата (имя столбца или *)
};

/// Одна клауза ORDER BY
struct OrderByClause {
    std::string column;
    bool ascending = true;
};

/// Один INNER JOIN
struct JoinClause {
    std::string tableName;
    std::string leftCol;    // столбец из основной таблицы
    std::string rightCol;   // столбец из присоединяемой таблицы
};

struct SelectStatement : ASTNode {
    std::string              tableName;
    std::vector<std::string> columns;   // пустой вектор = SELECT * (совместимость)
    bool                     selectAll; // true если SELECT *
    std::unique_ptr<ConditionNode> where;

    // Расширенная проекция (Post-MVP)
    std::vector<SelectItem>      items;    // заполняется если есть агрегаты
    std::vector<JoinClause>      joins;
    std::vector<std::string>     groupBy;
    std::unique_ptr<ConditionNode> having;
    std::vector<OrderByClause>   orderBy;
    int64_t limitCount  = -1;
    int64_t limitOffset = 0;

    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct InsertStatement : ASTNode {
    std::string                   tableName;
    std::vector<std::string>      columns;
    std::vector<std::vector<Value>> values; // несколько строк VALUES

    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct UpdateStatement : ASTNode {
    std::string                         tableName;
    std::vector<std::pair<std::string, Value>> assignments;
    std::unique_ptr<ConditionNode>      where;

    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct DeleteStatement : ASTNode {
    std::string                    tableName;
    std::unique_ptr<ConditionNode> where;

    void accept(ASTVisitor& v) override { v.visit(*this); }
};

// ─────────────────────────── Управление пользователями ───────────────

struct CreateUserStatement : ASTNode {
    std::string username;
    std::string password;
    std::string role; // "ADMIN", "READWRITE", "READONLY"
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct DropUserStatement : ASTNode {
    std::string username;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct ShowUsersStatement : ASTNode {
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct AuthStatement : ASTNode {
    std::string username;
    std::string password;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct GrantStatement : ASTNode {
    std::string username;
    std::string tableName; // "*" = все таблицы
    bool all = false;
    bool sel = false, ins = false, upd = false, del = false;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct RevokeStatement : ASTNode {
    std::string username;
    std::string tableName;
    bool all = false;
    bool sel = false, ins = false, upd = false, del = false;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

// ─────────────────────────── Вспомогательное ─────────────────────────

/// Сравнивает два Value через оператор (для ComparisonCondition::evaluate)
bool compareValues(const Value& a, CompareOp op, const Value& b);

} // namespace chapadb
