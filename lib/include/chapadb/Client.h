#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace chapadb {

// Упрощённые типы для клиентской библиотеки 

struct ClientRow {
    std::vector<std::string> values; // все значения как строки
};

struct ClientResult {
    bool                     success;
    std::string              errorMessage;
    std::vector<std::string> columns;
    std::vector<ClientRow>   rows;
    int64_t                  affectedRows;
    std::string              message;
};

/**
 * <summary>
 * Публичный TCP-клиент chapaBD.
 * Использует pimpl для скрытия деталей реализации.
 * </summary>
 */
class Client {
public:
    Client();
    ~Client();

    bool connect(const std::string& host, int port);
    void disconnect();
    bool isConnected() const;

    ClientResult execute(const std::string& sql);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} 
