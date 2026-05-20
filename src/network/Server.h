#pragma once
#include "../common/Types.h"
#include <string>
#include <atomic>

namespace chapadb {

/**
 * <summary>
 * TCP-сервер на POSIX-сокетах.
 * Каждое соединение обрабатывается в отдельном std::thread.
 * </summary>
 */
class Server {
public:
    Server(std::string host, int port);
    ~Server();

    /// Запуск сервера
    void run();

    /// Остановка сервера
    void stop();

private:
    std::string         m_host;
    int                 m_port;
    int                 m_serverFd;
    std::atomic<bool>   m_running;

    /// Обрабатывает одно клиентское соединение
    void handleClient(int clientFd);

    std::string processQuery(const std::string& sql);

    // Протокольные хелперы
    static uint32_t readU32BE(int fd);
    static void     writeU32BE(int fd, uint32_t v);
    static std::string recvAll(int fd, size_t n);
    static void     sendAll(int fd, const void* data, size_t n);

    // Форматирование JSON-ответа
    static std::string buildSelectJson(const QueryResult& r);
    static std::string buildAffectedJson(int64_t n);
    static std::string buildMessageJson(const std::string& msg);
    static std::string buildErrorJson(const std::string& err);
    static std::string escapeJson(const std::string& s);
    static std::string valueToJson(const Value& v);
};

} 
