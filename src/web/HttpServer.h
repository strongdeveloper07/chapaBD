#pragma once
#include <string>
#include <functional>
#include <map>
#include <thread>
#include <atomic>

namespace chapadb {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
};

struct HttpResponse {
    int         status = 200;
    std::string contentType = "application/json";
    std::string body;
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

/**
 * <summary>
 * Встроенный HTTP/1.1 сервер на POSIX-сокетах.
 * Каждый запрос обрабатывается в отдельном потоке.
 * </summary>
 */
class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    /// Регистрирует обработчик для метода + пути
    void on(const std::string& method, const std::string& path, HttpHandler handler);

    /// Запускает сервер (блокирует поток)
    void listen(const std::string& host, int port);

    /// Запускает в фоновом потоке
    void listenAsync(const std::string& host, int port);

    void stop();

private:
    std::map<std::string, HttpHandler> m_routes; // "METHOD /path" → handler
    std::atomic<bool> m_running{false};
    int m_serverFd{-1};

    void handleClient(int clientFd);
    HttpResponse dispatch(const HttpRequest& req);

    static std::string statusText(int code);
    static HttpRequest parseRequest(const std::string& raw);
};

} // namespace chapadb
