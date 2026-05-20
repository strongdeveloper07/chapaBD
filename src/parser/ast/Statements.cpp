#include "Statements.h"
#include <stdexcept>
#include <algorithm>

namespace chapadb {

/// Универсальное числовое сравнение через приведение к double
static bool numericCompare(double a, CompareOp op, double b) {
    switch (op) {
        case CompareOp::EQ:  return a == b;
        case CompareOp::NEQ: return a != b;
        case CompareOp::LT:  return a <  b;
        case CompareOp::GT:  return a >  b;
        case CompareOp::LE:  return a <= b;
        case CompareOp::GE:  return a >= b;
    }
    return false;
}

bool compareValues(const Value& a, CompareOp op, const Value& b) {
    // NULL не равен ничему
    if (std::holds_alternative<std::monostate>(a) ||
        std::holds_alternative<std::monostate>(b)) {
        return false;
    }

    // Оба INT
    if (std::holds_alternative<int32_t>(a) && std::holds_alternative<int32_t>(b)) {
        return numericCompare(std::get<int32_t>(a), op, std::get<int32_t>(b));
    }

    // Оба FLOAT или INT+FLOAT
    if ((std::holds_alternative<double>(a) || std::holds_alternative<int32_t>(a)) &&
        (std::holds_alternative<double>(b) || std::holds_alternative<int32_t>(b))) {
        double da = std::holds_alternative<double>(a)
                        ? std::get<double>(a)
                        : static_cast<double>(std::get<int32_t>(a));
        double db = std::holds_alternative<double>(b)
                        ? std::get<double>(b)
                        : static_cast<double>(std::get<int32_t>(b));
        return numericCompare(da, op, db);
    }

    // BOOL
    if (std::holds_alternative<bool>(a) && std::holds_alternative<bool>(b)) {
        bool ba = std::get<bool>(a), bb = std::get<bool>(b);
        switch (op) {
            case CompareOp::EQ:  return ba == bb;
            case CompareOp::NEQ: return ba != bb;
            default: return false;
        }
    }

    // STRING
    if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
        const auto& sa = std::get<std::string>(a);
        const auto& sb = std::get<std::string>(b);
        switch (op) {
            case CompareOp::EQ:  return sa == sb;
            case CompareOp::NEQ: return sa != sb;
            case CompareOp::LT:  return sa <  sb;
            case CompareOp::GT:  return sa >  sb;
            case CompareOp::LE:  return sa <= sb;
            case CompareOp::GE:  return sa >= sb;
        }
    }

    return false;
}

bool ComparisonCondition::evaluate(const Row& row, const std::vector<ColumnSchema>& schema) const {
    // Находим индекс столбца по имени
    int idx = -1;
    for (int i = 0; i < static_cast<int>(schema.size()); ++i) {
        if (schema[i].name == name) { idx = i; break; }
    }
    if (idx < 0) {
        throw std::runtime_error("Столбец '" + name + "' не найден в схеме");
    }
    if (idx >= static_cast<int>(row.values.size())) return false;
    return compareValues(row.values[idx], op, value);
}

} // namespace chapadb
