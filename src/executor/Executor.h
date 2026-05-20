#pragma once
#include "../parser/ast/ASTNode.h"
#include "../parser/ast/Statements.h"
#include "../common/Types.h"
#include "../storage/Table.h"

namespace chapadb {

/**
 * <summary>
 * Executor реализует паттерн Visitor: посещает каждый узел AST
 * и выполняет соответствующую операцию над хранилищем.
 * Результат запроса сохраняется в m_result.
 * </summary>
 */
class Executor : public ASTVisitor {
public:
    Executor();

    QueryResult execute(ASTNode& node);

    // ── ASTVisitor ─────────────────────────────────────────────────────
    void visit(CreateDatabaseStatement& stmt) override;
    void visit(DropDatabaseStatement&   stmt) override;
    void visit(UseDatabaseStatement&    stmt) override;
    void visit(CreateTableStatement&    stmt) override;
    void visit(DropTableStatement&      stmt) override;
    void visit(SelectStatement&         stmt) override;
    void visit(InsertStatement&         stmt) override;
    void visit(UpdateStatement&         stmt) override;
    void visit(DeleteStatement&         stmt) override;
    void visit(CreateUserStatement&     stmt) override;
    void visit(DropUserStatement&       stmt) override;
    void visit(ShowUsersStatement&      stmt) override;
    void visit(AuthStatement&           stmt) override;
    void visit(GrantStatement&          stmt) override;
    void visit(RevokeStatement&         stmt) override;

private:
    QueryResult m_result;

    /// Возвращает метаданные таблицы или бросает исключение
    TableMeta requireTable(const std::string& dbName, const std::string& tableName);

    /// Проверяет, что активная БД установлена
    const std::string& requireDb();

    /// Применяет WHERE-фильтр к строке
    bool matchesWhere(const Row& row, const TableMeta& meta,
                      const std::unique_ptr<ConditionNode>& where);
};

}
