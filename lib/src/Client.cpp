#include "../include/chapadb/Client.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <stdexcept>
#include <cstring>
#include <sstream>
#include <cctype>
#include <vector>
#include <algorithm>

namespace chapadb {

// ─────────────────────────── Pimpl ───────────────────────────────────

struct Client::Impl {
    int  sockFd    = -1;
    bool connected = false;

    // Протокол
    static void     writeU32BE(int fd, uint32_t v);
    static uint32_t readU32BE(int fd);
    static void     sendAll(int fd, const void* data, size_t n);
    static std::string recvAll(int fd, size_t n);

    // Простой JSON-парсер
    static ClientResult parseResponse(uint8_t status, const std::string& json);
};

// ─────────────────────────── Бинарный I/O ─────────────────────────────

void Client::Impl::writeU32BE(int fd, uint32_t v) {
    uint8_t buf[4] = {
        static_cast<uint8_t>((v >> 24) & 0xFF),
        static_cast<uint8_t>((v >> 16) & 0xFF),
        static_cast<uint8_t>((v >>  8) & 0xFF),
        static_cast<uint8_t>( v        & 0xFF)
    };
    sendAll(fd, buf, 4);
}

uint32_t Client::Impl::readU32BE(int fd) {
    uint8_t buf[4];
    ssize_t n = ::recv(fd, buf, 4, MSG_WAITALL);
    if (n != 4) throw std::runtime_error("Потеряно соединение с сервером");
    return (static_cast<uint32_t>(buf[0]) << 24) |
           (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) <<  8) |
            static_cast<uint32_t>(buf[3]);
}

void Client::Impl::sendAll(int fd, const void* data, size_t n) {
    const char* ptr = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < n) {
        ssize_t s = ::send(fd, ptr + sent, n - sent, MSG_NOSIGNAL);
        if (s <= 0) throw std::runtime_error("Ошибка отправки данных");
        sent += static_cast<size_t>(s);
    }
}

std::string Client::Impl::recvAll(int fd, size_t n) {
    std::string buf(n, '\0');
    size_t received = 0;
    while (received < n) {
        ssize_t r = ::recv(fd, buf.data() + received, n - received, 0);
        if (r <= 0) throw std::runtime_error("Потеряно соединение при получении данных");
        received += static_cast<size_t>(r);
    }
    return buf;
}

// ─────────────────────────── Простой JSON-парсер ─────────────────────

namespace {

/// Пропускает пробелы
static size_t skipWs(const std::string& s, size_t pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
    return pos;
}

/// Парсит JSON-строку начиная с '"'
static std::string parseJsonString(const std::string& s, size_t& pos) {
    if (s[pos] != '"') return {};
    ++pos;
    std::string result;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1 < s.size()) {
            ++pos;
            switch (s[pos]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += s[pos]; break;
            }
        } else {
            result += s[pos];
        }
        ++pos;
    }
    if (pos < s.size()) ++pos; // закрывающая "
    return result;
}

/// Парсит произвольный JSON-примитив или строку как std::string
static std::string parseJsonValue(const std::string& s, size_t& pos) {
    pos = skipWs(s, pos);
    if (pos >= s.size()) return "";

    if (s[pos] == '"') return parseJsonString(s, pos);

    // null / true / false / число
    size_t start = pos;
    while (pos < s.size() && s[pos] != ',' && s[pos] != ']' && s[pos] != '}') ++pos;
    std::string raw = s.substr(start, pos - start);
    // Убираем пробелы
    while (!raw.empty() && std::isspace(static_cast<unsigned char>(raw.back()))) raw.pop_back();
    return raw;
}

} // anonymous namespace

ClientResult Client::Impl::parseResponse(uint8_t status, const std::string& json) {
    ClientResult res;
    res.success      = (status == 0);
    res.affectedRows = 0;

    size_t pos = skipWs(json, 0);
    if (pos >= json.size() || json[pos] != '{') {
        res.success      = false;
        res.errorMessage = "Неверный формат ответа сервера";
        return res;
    }
    ++pos; // {

    while (pos < json.size()) {
        pos = skipWs(json, pos);
        if (pos >= json.size() || json[pos] == '}') break;
        if (json[pos] == ',') { ++pos; continue; }

        // Ключ
        std::string key = parseJsonString(json, pos);
        pos = skipWs(json, pos);
        if (pos < json.size() && json[pos] == ':') ++pos;
        pos = skipWs(json, pos);

        if (key == "error") {
            res.errorMessage = parseJsonString(json, pos);
            res.success      = false;
        } else if (key == "message") {
            res.message = parseJsonString(json, pos);
        } else if (key == "affected") {
            std::string v = parseJsonValue(json, pos);
            if (!v.empty()) res.affectedRows = std::stoll(v);
        } else if (key == "count") {
            parseJsonValue(json, pos); // пропускаем — используем rows.size()
        } else if (key == "columns") {
            // ["col1","col2",...]
            if (pos < json.size() && json[pos] == '[') {
                ++pos;
                while (pos < json.size() && json[pos] != ']') {
                    pos = skipWs(json, pos);
                    if (json[pos] == ',') { ++pos; continue; }
                    if (json[pos] == ']') break;
                    res.columns.push_back(parseJsonString(json, pos));
                }
                if (pos < json.size()) ++pos; // ]
            }
        } else if (key == "rows") {
            // [[v1,v2],[v3,v4],...]
            if (pos < json.size() && json[pos] == '[') {
                ++pos;
                while (pos < json.size() && json[pos] != ']') {
                    pos = skipWs(json, pos);
                    if (json[pos] == ',') { ++pos; continue; }
                    if (json[pos] == ']') break;

                    if (json[pos] == '[') {
                        ++pos;
                        ClientRow row;
                        while (pos < json.size() && json[pos] != ']') {
                            pos = skipWs(json, pos);
                            if (json[pos] == ',') { ++pos; continue; }
                            if (json[pos] == ']') break;
                            row.values.push_back(parseJsonValue(json, pos));
                        }
                        if (pos < json.size()) ++pos; // ]
                        res.rows.push_back(std::move(row));
                    } else {
                        ++pos;
                    }
                }
                if (pos < json.size()) ++pos; // ]
            }
        } else {
            // Пропускаем неизвестный ключ
            parseJsonValue(json, pos);
        }
    }

    if (!res.success && res.errorMessage.empty()) {
        res.errorMessage = "Неизвестная ошибка сервера";
    }
    return res;
}

// ─────────────────────────── Client API ──────────────────────────────

Client::Client() : m_impl(std::make_unique<Impl>()) {}
Client::~Client() { disconnect(); }

bool Client::connect(const std::string& host, int port) {
    m_impl->sockFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_impl->sockFd < 0) return false;

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(port);
    if (::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) {
        ::close(m_impl->sockFd);
        m_impl->sockFd = -1;
        return false;
    }

    bool ok = (::connect(m_impl->sockFd, res->ai_addr, res->ai_addrlen) == 0);
    ::freeaddrinfo(res);

    if (!ok) {
        ::close(m_impl->sockFd);
        m_impl->sockFd = -1;
        return false;
    }

    m_impl->connected = true;
    return true;
}

void Client::disconnect() {
    if (m_impl->sockFd >= 0) {
        ::close(m_impl->sockFd);
        m_impl->sockFd    = -1;
        m_impl->connected = false;
    }
}

bool Client::isConnected() const {
    return m_impl->connected;
}

ClientResult Client::execute(const std::string& sql) {
    if (!m_impl->connected) {
        return ClientResult{false, "Нет соединения с сервером", {}, {}, 0, ""};
    }

    try {
        // Отправляем: [uint32_t длина][sql]
        Impl::writeU32BE(m_impl->sockFd, static_cast<uint32_t>(sql.size()));
        Impl::sendAll(m_impl->sockFd, sql.data(), sql.size());

        // Получаем: [uint8_t статус][uint32_t длина][json]
        uint8_t statusByte = 0;
        ssize_t n = ::recv(m_impl->sockFd, &statusByte, 1, MSG_WAITALL);
        if (n != 1) throw std::runtime_error("Сервер разорвал соединение");

        uint32_t    jsonLen  = Impl::readU32BE(m_impl->sockFd);
        std::string jsonData = Impl::recvAll(m_impl->sockFd, jsonLen);

        return Impl::parseResponse(statusByte, jsonData);

    } catch (const std::exception& e) {
        m_impl->connected = false;
        return ClientResult{false, std::string("Ошибка соединения: ") + e.what(), {}, {}, 0, ""};
    }
}

} // namespace chapadb
