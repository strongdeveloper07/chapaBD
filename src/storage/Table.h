#pragma once
#include "../common/Types.h"
#include <string>
#include <vector>

namespace chapadb {

/// Метаданные таблицы (схема + имя)
struct TableMeta {
    std::string              name;
    std::vector<ColumnSchema> columns;
};

} // namespace chapadb
