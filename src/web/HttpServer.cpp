#include "HttpServer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <cstring>
#include <thread>

namespace chapadb {

HttpServer::HttpServer() = default;

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::on(const std::string& method, const std::string& path, HttpHandler handler) {
    m_routes[method + " " + path] = std::move(handler);
}

std::string HttpServer::statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

HttpRequest HttpServer::parseRequest(const std::string& raw) {
    HttpRequest req;
    std::istringstream ss(raw);
    std::string line;

    if (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line);
        std::string proto;
        ls >> req.method >> req.path >> proto;
    }

    int contentLength = 0;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            while (!val.empty() && val.front() == ' ') val.erase(val.begin());
            req.headers[key] = val;
            if (key == "Content-Length") contentLength = std::stoi(val);
        }
    }

    if (contentLength > 0) {
        req.body.resize(contentLength);
        ss.read(&req.body[0], contentLength);
    }

    return req;
}

HttpResponse HttpServer::dispatch(const HttpRequest& req) {
    // Путь без query string для матчинга
    std::string pathOnly = req.path;
    auto qpos = pathOnly.find('?');
    if (qpos != std::string::npos) pathOnly = pathOnly.substr(0, qpos);

    // Сначала ищем точный маршрут (с полным путём)
    auto it = m_routes.find(req.method + " " + req.path);
    if (it == m_routes.end()) {
        // Потом — по пути без query string
        it = m_routes.find(req.method + " " + pathOnly);
    }
    if (it == m_routes.end()) {
        it = m_routes.find("OPTIONS *");
    }

    if (it != m_routes.end()) {
        try {
            return it->second(req);
        } catch (const std::exception& e) {
            HttpResponse err;
            err.status = 500;
            err.body   = "{\"error\":\"" + std::string(e.what()) + "\"}";
            return err;
        }
    }

    HttpResponse notFound;
    notFound.status = 404;
    notFound.body   = "{\"error\":\"Not found: " + pathOnly + "\"}";
    return notFound;
}

void HttpServer::handleClient(int clientFd) {
    std::string raw;
    char buf[4096];
    ssize_t n;

    while ((n = recv(clientFd, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        raw += buf;

        auto headerEnd = raw.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            int contentLen = 0;
            auto clPos = raw.find("Content-Length: ");
            if (clPos != std::string::npos) {
                auto clEnd = raw.find("\r\n", clPos);
                contentLen = std::stoi(raw.substr(clPos + 16, clEnd - clPos - 16));
            }
            int bodyReceived = static_cast<int>(raw.size()) - static_cast<int>(headerEnd) - 4;
            if (bodyReceived >= contentLen) break;
        }
    }

    if (raw.empty()) {
        close(clientFd);
        return;
    }

    HttpRequest req = parseRequest(raw);
    HttpResponse resp = dispatch(req);

    // Формируем HTTP-ответ
    std::string response;
    response += "HTTP/1.1 " + std::to_string(resp.status) + " " + statusText(resp.status) + "\r\n";
    response += "Content-Type: " + resp.contentType + "; charset=utf-8\r\n";
    response += "Content-Length: " + std::to_string(resp.body.size()) + "\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    response += "Access-Control-Allow-Headers: Content-Type\r\n";
    response += "Connection: close\r\n\r\n";
    response += resp.body;

    send(clientFd, response.c_str(), response.size(), 0);
    close(clientFd);
}

void HttpServer::listen(const std::string& host, int port) {
    m_serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverFd < 0) throw std::runtime_error("HTTP: socket() failed");

    int opt = 1;
    setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = inet_addr(host.c_str());

    if (bind(m_serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("HTTP: bind() failed на порту " + std::to_string(port));

    ::listen(m_serverFd, 16);
    m_running = true;

    std::cout << "Веб-интерфейс доступен на http://" << host << ":" << port << std::endl;

    while (m_running) {
        int clientFd = accept(m_serverFd, nullptr, nullptr);
        if (clientFd < 0) break;
        std::thread([this, clientFd]() {
            handleClient(clientFd);
        }).detach();
    }
}

void HttpServer::listenAsync(const std::string& host, int port) {
    std::thread([this, host, port]() {
        try { listen(host, port); }
        catch (const std::exception& e) {
            std::cerr << "HTTP сервер: " << e.what() << std::endl;
        }
    }).detach();
}

void HttpServer::stop() {
    m_running = false;
    if (m_serverFd >= 0) {
        close(m_serverFd);
        m_serverFd = -1;
    }
}

} 
