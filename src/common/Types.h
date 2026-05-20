#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace chapadb {

enum class DataType : uint8_t { INT = 0, FLOAT = 1, BOOL = 2, TEXT = 3, VARCHAR = 4 };

struct ColumnSchema {
    std::string name;
    DataType    type;
    uint32_t    param; // для VARCHAR — максимальная длина
};


using Value = std::variant<std::monostate, int32_t, double, bool, std::string>;

struct Row {
    std::vector<Value> values;
};

struct QueryResult {
    bool                     success;
    std::string              errorMessage;
    std::vector<std::string> columns;
    std::vector<Row>         rows;
    int64_t                  affectedRows;
    std::string              message;
};

} 
