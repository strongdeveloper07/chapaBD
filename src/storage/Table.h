#pragma once
#include "../common/Types.h"
#include <string>
#include <vector>

namespace chapadb {

struct TableMeta {
    std::string              name;
    std::vector<ColumnSchema> columns;
};

} 
