#include "catalog/Catalog.h"
#include "auth/AuthManager.h"
#include "network/Server.h"
#include "web/HttpServer.h"
#include "web/WebApi.h"
#include <iostream>
#include <string>
#include <csignal>
#include <cstdlib>

namespace {
    chapadb::Server*     g_server    = nullptr;
    chapadb::HttpServer* g_webServer = nullptr;
}

static void signalHandler(int /*sig*/) {
    std::cout << "\nОстановка сервера..." << std::endl;
    if (g_webServer) g_webServer->stop();
    if (g_server)    g_server->stop();
}

int main(int argc, char* argv[]) {
    std::string host     = "0.0.0.0";
    int         port     = 5432;
    std::string dataDir  = "data";
    int         webPort  = -1; // -1 = auto (port + 1000)

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--host"     && i + 1 < argc) host    = argv[++i];
        else if (arg == "--port"     && i + 1 < argc) port    = std::stoi(argv[++i]);
        else if (arg == "--data"     && i + 1 < argc) dataDir = argv[++i];
        else if (arg == "--web-port" && i + 1 < argc) webPort = std::stoi(argv[++i]);
        else if (arg == "--no-web")                   webPort = 0;
    }
    if (webPort == -1) webPort = port + 1000;

    chapadb::Catalog::instance().init(dataDir);
    chapadb::AuthManager::instance().init(dataDir);

    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Запускаем веб-интерфейс в фоне (если не отключён)
    chapadb::HttpServer webServer;
    if (webPort > 0) {
        chapadb::registerWebApi(webServer);
        webServer.listenAsync(host, webPort);
        g_webServer = &webServer;
    }

    chapadb::Server server(host, port);
    g_server = &server;

    try {
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Ошибка сервера: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
