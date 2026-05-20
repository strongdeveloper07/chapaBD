#pragma once
#include "Table.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace chapadb {

/// Представляет одну базу данных: имя + коллекция таблиц
struct DatabaseMeta {
    std::string                          name;
    std::unordered_map<std::string, TableMeta> tables;
};

} // namespace chapadb
