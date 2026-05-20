#pragma once
#include <memory>

namespace chapadb {

// Предварительные объявления всех Statement-классов
struct CreateDatabaseStatement;
struct DropDatabaseStatement;
struct UseDatabaseStatement;
struct CreateTableStatement;
struct DropTableStatement;
struct SelectStatement;
struct InsertStatement;
struct UpdateStatement;
struct DeleteStatement;
struct CreateUserStatement;
struct DropUserStatement;
struct ShowUsersStatement;
struct AuthStatement;
struct GrantStatement;
struct RevokeStatement;

/**
 * <summary>
 * Чистый абстрактный интерфейс Visitor для обхода AST.
 * Каждый конкретный посетитель реализует все методы visit().
 * </summary>
 */
struct ASTVisitor {
    virtual ~ASTVisitor() = default;
    virtual void visit(CreateDatabaseStatement& stmt) = 0;
    virtual void visit(DropDatabaseStatement&   stmt) = 0;
    virtual void visit(UseDatabaseStatement&    stmt) = 0;
    virtual void visit(CreateTableStatement&    stmt) = 0;
    virtual void visit(DropTableStatement&      stmt) = 0;
    virtual void visit(SelectStatement&         stmt) = 0;
    virtual void visit(InsertStatement&         stmt) = 0;
    virtual void visit(UpdateStatement&         stmt) = 0;
    virtual void visit(DeleteStatement&         stmt) = 0;
    virtual void visit(CreateUserStatement&     stmt) = 0;
    virtual void visit(DropUserStatement&       stmt) = 0;
    virtual void visit(ShowUsersStatement&      stmt) = 0;
    virtual void visit(AuthStatement&           stmt) = 0;
    virtual void visit(GrantStatement&          stmt) = 0;
    virtual void visit(RevokeStatement&         stmt) = 0;
};

/// Базовый класс для всех узлов AST
struct ASTNode {
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
};

} // namespace chapadb
