#include "Server.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"
#include "../executor/Executor.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#include <thread>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <sstream>

namespace chapadb {

Server::Server(std::string host, int port)
    : m_host(std::move(host)), m_port(port), m_serverFd(-1), m_running(false)
{}

Server::~Server() {
    stop();
}

// ─────────────────────────── Запуск ──────────────────────────────────

void Server::run() {
    m_serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverFd < 0) throw std::runtime_error("Не удалось создать сокет");

    int opt = 1;
    ::setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(m_port));

    if (m_host == "0.0.0.0" || m_host.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        ::inet_pton(AF_INET, m_host.c_str(), &addr.sin_addr);
    }

    if (::bind(m_serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("bind() не удался на порту " + std::to_string(m_port));
    }

    if (::listen(m_serverFd, 64) < 0) {
        throw std::runtime_error("listen() не удался");
    }

    m_running = true;
    std::cout << "chapaBD сервер запущен на " << m_host << ":" << m_port << std::endl;

    while (m_running) {
        sockaddr_in clientAddr{};
        socklen_t   addrLen = sizeof(clientAddr);
        int clientFd = ::accept(m_serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
        if (clientFd < 0) {
            if (m_running) {
                std::cerr << "accept() ошибка: " << strerror(errno) << std::endl;
            }
            continue;
        }

        // Обрабатываем клиента в отдельном потоке
        std::thread([this, clientFd]() {
            handleClient(clientFd);
            ::close(clientFd);
        }).detach();
    }
}

void Server::stop() {
    m_running = false;
    if (m_serverFd >= 0) {
        ::shutdown(m_serverFd, SHUT_RDWR);
        ::close(m_serverFd);
        m_serverFd = -1;
    }
}

// ─────────────────────────── Протокол ────────────────────────────────

uint32_t Server::readU32BE(int fd) {
    uint8_t buf[4];
    ssize_t n = ::recv(fd, buf, 4, MSG_WAITALL);
    if (n != 4) throw std::runtime_error("Соединение разорвано");
    return (static_cast<uint32_t>(buf[0]) << 24) |
           (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) <<  8) |
            static_cast<uint32_t>(buf[3]);
}

void Server::writeU32BE(int fd, uint32_t v) {
    uint8_t buf[4] = {
        static_cast<uint8_t>((v >> 24) & 0xFF),
        static_cast<uint8_t>((v >> 16) & 0xFF),
        static_cast<uint8_t>((v >>  8) & 0xFF),
        static_cast<uint8_t>( v        & 0xFF)
    };
    sendAll(fd, buf, 4);
}

std::string Server::recvAll(int fd, size_t n) {
    std::string buf(n, '\0');
    size_t received = 0;
    while (received < n) {
        ssize_t r = ::recv(fd, buf.data() + received, n - received, 0);
        if (r <= 0) throw std::runtime_error("Соединение разорвано при чтении данных");
        received += static_cast<size_t>(r);
    }
    return buf;
}

void Server::sendAll(int fd, const void* data, size_t n) {
    const char* ptr = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < n) {
        ssize_t s = ::send(fd, ptr + sent, n - sent, MSG_NOSIGNAL);
        if (s <= 0) throw std::runtime_error("Ошибка отправки данных");
        sent += static_cast<size_t>(s);
    }
}

// ─────────────────────────── Обработка клиента ───────────────────────

void Server::handleClient(int clientFd) {
    try {
        while (true) {
            // Читаем длину SQL-запроса
            uint32_t sqlLen;
            try {
                sqlLen = readU32BE(clientFd);
            } catch (...) {
                break; // клиент отключился
            }

            if (sqlLen == 0 || sqlLen > 1024 * 1024) break;

            std::string sql = recvAll(clientFd, sqlLen);
            std::string jsonResp = processQuery(sql);

            // Отправляем ответ: [статус 1 байт][длина 4 байта][JSON]
            bool hasError = (jsonResp.size() > 9 && jsonResp.substr(2, 5) == "error");
            uint8_t status = hasError ? 1 : 0;

            uint8_t statusByte = status;
            sendAll(clientFd, &statusByte, 1);
            writeU32BE(clientFd, static_cast<uint32_t>(jsonResp.size()));
            sendAll(clientFd, jsonResp.data(), jsonResp.size());
        }
    } catch (const std::exception& e) {
        std::cerr << "Ошибка обработки клиента: " << e.what() << std::endl;
    }
}

// ─────────────────────────── Выполнение запроса ──────────────────────

std::string Server::processQuery(const std::string& sql) {
    try {
        Lexer  lexer(sql);
        auto   tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto   ast = parser.parse();

        Executor executor;
        QueryResult result = executor.execute(*ast);

        if (!result.success) {
            return buildErrorJson(result.errorMessage);
        }

        // SELECT возвращает таблицу
        if (!result.columns.empty()) {
            return buildSelectJson(result);
        }
        // INSERT/UPDATE/DELETE возвращают количество строк
        if (result.affectedRows > 0 || result.message.empty()) {
            return buildAffectedJson(result.affectedRows);
        }
        return buildMessageJson(result.message);

    } catch (const std::exception& e) {
        return buildErrorJson(e.what());
    }
}

// ─────────────────────────── JSON-форматирование ─────────────────────

std::string Server::escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string Server::valueToJson(const Value& v) {
    if (std::holds_alternative<std::monostate>(v)) return "null";
    if (std::holds_alternative<int32_t>(v))        return std::to_string(std::get<int32_t>(v));
    if (std::holds_alternative<double>(v)) {
        std::ostringstream oss;
        oss << std::get<double>(v);
        return oss.str();
    }
    if (std::holds_alternative<bool>(v))   return std::get<bool>(v) ? "true" : "false";
    if (std::holds_alternative<std::string>(v)) {
        return "\"" + escapeJson(std::get<std::string>(v)) + "\"";
    }
    return "null";
}

std::string Server::buildSelectJson(const QueryResult& r) {
    std::ostringstream out;
    out << "{\"columns\":[";
    for (size_t i = 0; i < r.columns.size(); ++i) {
        if (i) out << ',';
        out << '"' << escapeJson(r.columns[i]) << '"';
    }
    out << "],\"rows\":[";
    for (size_t i = 0; i < r.rows.size(); ++i) {
        if (i) out << ',';
        out << '[';
        const auto& row = r.rows[i];
        for (size_t j = 0; j < row.values.size(); ++j) {
            if (j) out << ',';
            out << valueToJson(row.values[j]);
        }
        out << ']';
    }
    out << "],\"count\":" << r.rows.size() << '}';
    return out.str();
}

std::string Server::buildAffectedJson(int64_t n) {
    return "{\"affected\":" + std::to_string(n) + "}";
}

std::string Server::buildMessageJson(const std::string& msg) {
    return "{\"message\":\"" + escapeJson(msg) + "\"}";
}

std::string Server::buildErrorJson(const std::string& err) {
    return "{\"error\":\"" + escapeJson(err) + "\"}";
}

} // namespace chapadb
