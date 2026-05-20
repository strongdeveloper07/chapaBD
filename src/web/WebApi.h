#pragma once
#include "HttpServer.h"
#include "../common/Types.h"
#include <string>

namespace chapadb {

/// Регистрирует все REST API маршруты в переданном HttpServer
void registerWebApi(HttpServer& server);

/// Конвертирует QueryResult в JSON
std::string resultToJson(const QueryResult& result);

/// Экранирует строку для JSON
std::string jsonEscape(const std::string& s);

/// Конвертирует Value в JSON-представление
std::string valueToJson(const Value& v);

} // namespace chapadb
