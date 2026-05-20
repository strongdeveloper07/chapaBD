#pragma once
#include "Table.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace chapadb {

struct DatabaseMeta {
    std::string                          name;
    std::unordered_map<std::string, TableMeta> tables;
};

} 
