#include "Parser.h"
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace chapadb {

Parser::Parser(std::vector<Token> tokens)
    : m_tokens(std::move(tokens)), m_pos(0)
{}

const Token& Parser::peek() const {
    return m_tokens[m_pos];
}

const Token& Parser::advance() {
    const Token& t = m_tokens[m_pos];
    if (t.type != TokenType::END_OF_FILE) ++m_pos;
    return t;
}

bool Parser::check(TokenType t) const {
    return m_tokens[m_pos].type == t;
}

bool Parser::match(TokenType t) {
    if (check(t)) { advance(); return true; }
    return false;
}

const Token& Parser::expect(TokenType t, const std::string& msg) {
    if (!check(t)) {
        throw std::runtime_error(msg + " (получен: '" + peek().value + "' на строке " +
                                 std::to_string(peek().line) + ")");
    }
    return advance();
}

std::unique_ptr<ASTNode> Parser::parse() {
    auto node = parseStatement();
    match(TokenType::SYM_SEMICOL); 
    return node;
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    switch (peek().type) {
        case TokenType::KW_SELECT: return parseSelect();
        case TokenType::KW_INSERT: return parseInsert();
        case TokenType::KW_UPDATE: return parseUpdate();
        case TokenType::KW_DELETE: return parseDelete();
        case TokenType::KW_CREATE: return parseCreate();
        case TokenType::KW_DROP:   return parseDrop();
        case TokenType::KW_USE:    return parseUse();
        case TokenType::KW_GRANT:  return parseGrant();
        case TokenType::KW_REVOKE: return parseRevoke();
        case TokenType::KW_SHOW:   return parseShowUsers();
        case TokenType::KW_AUTH:   return parseAuth();
        default:
            throw std::runtime_error(
                "Ожидался SQL-оператор, получен '" + peek().value + "' на строке " +
                std::to_string(peek().line));
    }
}

// ─────────────────────────── CREATE ──────────────────────────────────

std::unique_ptr<ASTNode> Parser::parseCreate() {
    expect(TokenType::KW_CREATE, "Ожидался CREATE");

    if (match(TokenType::KW_DATABASE)) {
        auto stmt = std::make_unique<CreateDatabaseStatement>();
        stmt->dbName = parseIdentifier("Ожидалось имя базы данных");
        return stmt;
    }

    if (match(TokenType::KW_TABLE)) {
        auto stmt = std::make_unique<CreateTableStatement>();
        stmt->tableName = parseIdentifier("Ожидалось имя таблицы");
        expect(TokenType::SYM_LPAREN, "Ожидался '(' после имени таблицы");

        do {
            stmt->columns.push_back(parseColumnDef());
        } while (match(TokenType::SYM_COMMA));

        expect(TokenType::SYM_RPAREN, "Ожидался ')' после определения столбцов");
        return stmt;
    }

    if (match(TokenType::KW_USER)) return parseCreateUser();

    throw std::runtime_error("Ожидался DATABASE, TABLE или USER после CREATE");
}

ColumnSchema Parser::parseColumnDef() {
    ColumnSchema col;
    col.name  = parseIdentifier("Ожидалось имя столбца");
    col.param = 0;

    switch (peek().type) {
        case TokenType::KW_INT:     advance(); col.type = DataType::INT;     break;
        case TokenType::KW_FLOAT:   advance(); col.type = DataType::FLOAT;   break;
        case TokenType::KW_BOOL:    advance(); col.type = DataType::BOOL;    break;
        case TokenType::KW_TEXT:    advance(); col.type = DataType::TEXT;    break;
        case TokenType::KW_VARCHAR: {
            advance(); col.type = DataType::VARCHAR;
            // VARCHAR(n)
            if (match(TokenType::SYM_LPAREN)) {
                const Token& n = expect(TokenType::LIT_INTEGER, "Ожидалась длина VARCHAR");
                col.param = static_cast<uint32_t>(std::stoul(n.value));
                expect(TokenType::SYM_RPAREN, "Ожидался ')' после длины VARCHAR");
            }
            break;
        }
        default:
            throw std::runtime_error("Ожидался тип данных для столбца '" + col.name + "'");
    }
    return col;
}

// ─────────────────────────── DROP ────────────────────────────────────

std::unique_ptr<ASTNode> Parser::parseDrop() {
    expect(TokenType::KW_DROP, "Ожидался DROP");

    if (match(TokenType::KW_DATABASE)) {
        auto stmt = std::make_unique<DropDatabaseStatement>();
        stmt->dbName = parseIdentifier("Ожидалось имя базы данных");
        return stmt;
    }

    if (match(TokenType::KW_TABLE)) {
        auto stmt = std::make_unique<DropTableStatement>();
        stmt->tableName = parseIdentifier("Ожидалось имя таблицы");
        return stmt;
    }

    if (match(TokenType::KW_USER)) return parseDropUser();

    throw std::runtime_error("Ожидался DATABASE, TABLE или USER после DROP");
}

// ─────────────────────────── SELECT ──────────────────────────────────

/// Определяет, является ли текущий токен началом агрегатной функции
static bool isAggToken(TokenType t) {
    return t == TokenType::KW_COUNT || t == TokenType::KW_SUM ||
           t == TokenType::KW_AVG   || t == TokenType::KW_MIN ||
           t == TokenType::KW_MAX;
}

SelectItem Parser::parseSelectItem() {
    SelectItem item;

    if (isAggToken(peek().type)) {
        // Агрегатная функция: COUNT(*), SUM(col), AVG(col), MIN(col), MAX(col)
        switch (peek().type) {
            case TokenType::KW_COUNT: item.aggFunc = AggFunc::COUNT; break;
            case TokenType::KW_SUM:   item.aggFunc = AggFunc::SUM;   break;
            case TokenType::KW_AVG:   item.aggFunc = AggFunc::AVG;   break;
            case TokenType::KW_MIN:   item.aggFunc = AggFunc::MIN;   break;
            case TokenType::KW_MAX:   item.aggFunc = AggFunc::MAX;   break;
            default: break;
        }
        advance();
        expect(TokenType::SYM_LPAREN, "Ожидался '(' после агрегатной функции");
        if (match(TokenType::SYM_STAR)) {
            item.aggArg = "*";
        } else {
            item.aggArg = parseIdentifier("Ожидался аргумент агрегатной функции");
        }
        expect(TokenType::SYM_RPAREN, "Ожидался ')' после аргумента агрегата");
    } else {
        item.colName = parseIdentifier("Ожидалось имя столбца");
        item.aggFunc = AggFunc::NONE;
    }
    return item;
}

std::unique_ptr<ASTNode> Parser::parseSelect() {
    expect(TokenType::KW_SELECT, "Ожидался SELECT");
    auto stmt = std::make_unique<SelectStatement>();
    stmt->selectAll = false;
    bool hasAgg = false;

    if (match(TokenType::SYM_STAR)) {
        stmt->selectAll = true;
    } else {
        // Проверяем есть ли агрегатные функции
        if (isAggToken(peek().type)) hasAgg = true;
        stmt->items.push_back(parseSelectItem());
        while (match(TokenType::SYM_COMMA)) {
            if (isAggToken(peek().type)) hasAgg = true;
            stmt->items.push_back(parseSelectItem());
        }
        if (!hasAgg) {
            for (const auto& item : stmt->items) {
                stmt->columns.push_back(item.colName);
            }
        }
    }

    expect(TokenType::KW_FROM, "Ожидался FROM");
    stmt->tableName = parseIdentifier("Ожидалось имя таблицы");

    // INNER JOIN
    while (check(TokenType::KW_INNER) || check(TokenType::KW_JOIN)) {
        match(TokenType::KW_INNER);
        expect(TokenType::KW_JOIN, "Ожидался JOIN");
        JoinClause jc;
        jc.tableName = parseIdentifier("Ожидалось имя таблицы JOIN");
        expect(TokenType::KW_ON, "Ожидался ON после имени таблицы JOIN");
        std::string lhs = parseIdentifier("Ожидался столбец ON");
        expect(TokenType::OP_EQ, "Ожидался '=' в условии ON");
        std::string rhs = parseIdentifier("Ожидался столбец ON");
        jc.leftCol  = lhs;
        jc.rightCol = rhs;
        stmt->joins.push_back(std::move(jc));
    }

    if (match(TokenType::KW_WHERE)) {
        stmt->where = parseWhere();
    }

    // GROUP BY
    if (check(TokenType::KW_GROUP)) {
        advance();
        expect(TokenType::KW_BY, "Ожидался BY после GROUP");
        stmt->groupBy.push_back(parseIdentifier("Ожидалось имя столбца GROUP BY"));
        while (match(TokenType::SYM_COMMA)) {
            stmt->groupBy.push_back(parseIdentifier("Ожидалось имя столбца"));
        }
    }

    // HAVING
    if (match(TokenType::KW_HAVING)) {
        stmt->having = parseWhere();
    }

    // ORDER BY
    if (check(TokenType::KW_ORDER)) {
        advance();
        expect(TokenType::KW_BY, "Ожидался BY после ORDER");
        do {
            OrderByClause ob;
            ob.column = parseIdentifier("Ожидалось имя столбца ORDER BY");
            ob.ascending = true;
            if (match(TokenType::KW_DESC)) ob.ascending = false;
            else match(TokenType::KW_ASC);
            stmt->orderBy.push_back(std::move(ob));
        } while (match(TokenType::SYM_COMMA));
    }

    // LIMIT [OFFSET]
    if (match(TokenType::KW_LIMIT)) {
        const Token& n = expect(TokenType::LIT_INTEGER, "Ожидалось число после LIMIT");
        stmt->limitCount = std::stoll(n.value);
        if (match(TokenType::KW_OFFSET)) {
            const Token& off = expect(TokenType::LIT_INTEGER, "Ожидалось число после OFFSET");
            stmt->limitOffset = std::stoll(off.value);
        }
    }

    return stmt;
}

// ─────────────────────────── INSERT ──────────────────────────────────

std::unique_ptr<ASTNode> Parser::parseInsert() {
    expect(TokenType::KW_INSERT, "Ожидался INSERT");
    expect(TokenType::KW_INTO,   "Ожидался INTO");

    auto stmt = std::make_unique<InsertStatement>();
    stmt->tableName = parseIdentifier("Ожидалось имя таблицы");

    // Список столбцов
    if (match(TokenType::SYM_LPAREN)) {
        stmt->columns.push_back(parseIdentifier("Ожидалось имя столбца"));
        while (match(TokenType::SYM_COMMA)) {
            stmt->columns.push_back(parseIdentifier("Ожидалось имя столбца"));
        }
        expect(TokenType::SYM_RPAREN, "Ожидался ')'");
    }

    expect(TokenType::KW_VALUES, "Ожидался VALUES");

    // Одна или несколько групп значений
    do {
        expect(TokenType::SYM_LPAREN, "Ожидался '('");
        std::vector<Value> row;
        row.push_back(parseValue());
        while (match(TokenType::SYM_COMMA)) {
            row.push_back(parseValue());
        }
        expect(TokenType::SYM_RPAREN, "Ожидался ')'");
        stmt->values.push_back(std::move(row));
    } while (match(TokenType::SYM_COMMA));

    return stmt;
}

// ─────────────────────────── UPDATE ──────────────────────────────────

std::unique_ptr<ASTNode> Parser::parseUpdate() {
    expect(TokenType::KW_UPDATE, "Ожидался UPDATE");

    auto stmt = std::make_unique<UpdateStatement>();
    stmt->tableName = parseIdentifier("Ожидалось имя таблицы");

    expect(TokenType::KW_SET, "Ожидался SET");

    // column = value, ...
    do {
        std::string col = parseIdentifier("Ожидалось имя столбца");
        expect(TokenType::OP_EQ, "Ожидался '='");
        Value val = parseValue();
        stmt->assignments.emplace_back(std::move(col), std::move(val));
    } while (match(TokenType::SYM_COMMA));

    if (match(TokenType::KW_WHERE)) {
        stmt->where = parseWhere();
    }
    return stmt;
}

// ─────────────────────────── DELETE ──────────────────────────────────

std::unique_ptr<ASTNode> Parser::parseDelete() {
    expect(TokenType::KW_DELETE, "Ожидался DELETE");
    expect(TokenType::KW_FROM,   "Ожидался FROM");

    auto stmt = std::make_unique<DeleteStatement>();
    stmt->tableName = parseIdentifier("Ожидалось имя таблицы");

    if (match(TokenType::KW_WHERE)) {
        stmt->where = parseWhere();
    }
    return stmt;
}

// ─────────────────────────── USE ─────────────────────────────────────

std::unique_ptr<ASTNode> Parser::parseUse() {
    expect(TokenType::KW_USE, "Ожидался USE");
    auto stmt = std::make_unique<UseDatabaseStatement>();
    stmt->dbName = parseIdentifier("Ожидалось имя базы данных");
    return stmt;
}

// ─────────────────────────── WHERE ───────────────────────────────────

std::unique_ptr<ConditionNode> Parser::parseWhere() {
    return parseOr();
}

std::unique_ptr<ConditionNode> Parser::parseOr() {
    auto left = parseAnd();
    while (match(TokenType::KW_OR)) {
        auto right = parseAnd();
        auto node  = std::make_unique<OrCondition>();
        node->left  = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<ConditionNode> Parser::parseAnd() {
    auto left = parseNot();
    while (match(TokenType::KW_AND)) {
        auto right = parseNot();
        auto node  = std::make_unique<AndCondition>();
        node->left  = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<ConditionNode> Parser::parseNot() {
    if (match(TokenType::KW_NOT)) {
        auto node  = std::make_unique<NotCondition>();
        node->inner = parsePrimary();
        return node;
    }
    return parsePrimary();
}

std::unique_ptr<ConditionNode> Parser::parsePrimary() {
    if (match(TokenType::SYM_LPAREN)) {
        auto cond = parseOr();
        expect(TokenType::SYM_RPAREN, "Ожидался ')' после условия");
        return cond;
    }

    std::string col = parseIdentifier("Ожидалось имя столбца в условии");
    CompareOp   op  = parseCompareOp();
    Value       val = parseValue();

    auto node  = std::make_unique<ComparisonCondition>();
    node->name  = col;
    node->op    = op;
    node->value = std::move(val);
    return node;
}

CompareOp Parser::parseCompareOp() {
    switch (peek().type) {
        case TokenType::OP_EQ:  advance(); return CompareOp::EQ;
        case TokenType::OP_NEQ: advance(); return CompareOp::NEQ;
        case TokenType::OP_LT:  advance(); return CompareOp::LT;
        case TokenType::OP_GT:  advance(); return CompareOp::GT;
        case TokenType::OP_LE:  advance(); return CompareOp::LE;
        case TokenType::OP_GE:  advance(); return CompareOp::GE;
        default:
            throw std::runtime_error(
                "Ожидался оператор сравнения, получен '" + peek().value + "'");
    }
}

// ─────────────────────────── Управление пользователями ───────────────

std::unique_ptr<ASTNode> Parser::parseCreateUser() {
    auto stmt = std::make_unique<CreateUserStatement>();
    stmt->username = parseIdentifier("Ожидалось имя пользователя");
    if (match(TokenType::KW_WITH)) {
        match(TokenType::KW_PASSWORD); 
        stmt->password = expect(TokenType::LIT_STRING, "Ожидался пароль в кавычках").value;
    }
    if (match(TokenType::KW_ROLE)) {
        switch (peek().type) {
            case TokenType::KW_ADMIN:     advance(); stmt->role = "ADMIN"; break;
            case TokenType::KW_READWRITE: advance(); stmt->role = "READWRITE"; break;
            case TokenType::KW_READONLY:  advance(); stmt->role = "READONLY"; break;
            default: stmt->role = "READWRITE";
        }
    } else {
        stmt->role = "READWRITE";
    }
    return stmt;
}

std::unique_ptr<ASTNode> Parser::parseDropUser() {
    auto stmt = std::make_unique<DropUserStatement>();
    stmt->username = parseIdentifier("Ожидалось имя пользователя");
    return stmt;
}

std::unique_ptr<ASTNode> Parser::parseShowUsers() {
    expect(TokenType::KW_SHOW, "Ожидался SHOW");
    expect(TokenType::KW_USERS, "Ожидался USERS");
    return std::make_unique<ShowUsersStatement>();
}

std::unique_ptr<ASTNode> Parser::parseAuth() {
    expect(TokenType::KW_AUTH, "Ожидался AUTH");
    auto stmt = std::make_unique<AuthStatement>();
    stmt->username = expect(TokenType::IDENTIFIER, "Ожидалось имя пользователя").value;
    stmt->password = expect(TokenType::LIT_STRING, "Ожидался пароль в кавычках").value;
    return stmt;
}

/// Разбирает список привилегий в текущей позиции парсера
void parsePrivList(Parser& p, bool& all, bool& sel, bool& ins, bool& upd, bool& del) {
    if (p.check(TokenType::KW_ALL)) {
        p.advance(); all = sel = ins = upd = del = true;
        return;
    }
    auto try_match = [&](TokenType t, bool& flag) {
        if (p.check(t)) { p.advance(); flag = true; p.match(TokenType::SYM_COMMA); }
    };
    try_match(TokenType::KW_SELECT, sel);
    try_match(TokenType::KW_INSERT, ins);
    try_match(TokenType::KW_UPDATE, upd);
    try_match(TokenType::KW_DELETE, del);
}

std::unique_ptr<ASTNode> Parser::parseGrant() {
    expect(TokenType::KW_GRANT, "Ожидался GRANT");
    auto stmt = std::make_unique<GrantStatement>();
    parsePrivList(*this, stmt->all, stmt->sel, stmt->ins, stmt->upd, stmt->del);
    expect(TokenType::KW_ON, "Ожидался ON после привилегий");
    stmt->tableName = check(TokenType::SYM_STAR)
        ? (advance(), std::string("*"))
        : parseIdentifier("Ожидалось имя таблицы");
    expect(TokenType::KW_TO, "Ожидался TO");
    stmt->username = parseIdentifier("Ожидалось имя пользователя");
    return stmt;
}

std::unique_ptr<ASTNode> Parser::parseRevoke() {
    expect(TokenType::KW_REVOKE, "Ожидался REVOKE");
    auto stmt = std::make_unique<RevokeStatement>();
    parsePrivList(*this, stmt->all, stmt->sel, stmt->ins, stmt->upd, stmt->del);
    expect(TokenType::KW_ON, "Ожидался ON после привилегий");
    stmt->tableName = check(TokenType::SYM_STAR)
        ? (advance(), std::string("*"))
        : parseIdentifier("Ожидалось имя таблицы");
    expect(TokenType::KW_FROM, "Ожидался FROM");
    stmt->username = parseIdentifier("Ожидалось имя пользователя");
    return stmt;
}

std::string Parser::parseIdentifier(const std::string& errMsg) {
    TokenType t = peek().type;
    if (t == TokenType::IDENTIFIER   ||
        t == TokenType::KW_USERS     || t == TokenType::KW_USER      ||
        t == TokenType::KW_ALL       ||
        t == TokenType::KW_MIN       || t == TokenType::KW_MAX       ||
        t == TokenType::KW_SUM       || t == TokenType::KW_AVG       ||
        t == TokenType::KW_COUNT     ||
        t == TokenType::KW_ASC       || t == TokenType::KW_DESC      ||
        t == TokenType::KW_ROLE      || t == TokenType::KW_PASSWORD   ||
        t == TokenType::KW_AUTH      || t == TokenType::KW_ADMIN     ||
        t == TokenType::KW_READONLY  || t == TokenType::KW_READWRITE) {
        return advance().value;
    }
    throw std::runtime_error(errMsg + " (получен: '" + peek().value +
                             "' на строке " + std::to_string(peek().line) + ")");
}

Value Parser::parseValue() {
    switch (peek().type) {
        case TokenType::LIT_INTEGER: {
            int32_t v = static_cast<int32_t>(std::stoi(advance().value));
            return Value{v};
        }
        case TokenType::LIT_FLOAT: {
            double v = std::stod(advance().value);
            return Value{v};
        }
        case TokenType::LIT_STRING: {
            return Value{advance().value};
        }
        case TokenType::KW_TRUE:  advance(); return Value{true};
        case TokenType::KW_FALSE: advance(); return Value{false};
        case TokenType::KW_NULL:  advance(); return Value{std::monostate{}};
        default:
            throw std::runtime_error(
                "Ожидался литерал значения, получен '" + peek().value + "'");
    }
}

} 
