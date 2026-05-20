# Статическая библиотека с ядром СУБД
add_library(chapadb_core STATIC
    common/Types.h

    lexer/Token.h
    lexer/Lexer.h
    lexer/Lexer.cpp

    parser/ast/ASTNode.h
    parser/ast/Statements.h
    parser/ast/Statements.cpp
    parser/Parser.h
    parser/Parser.cpp

    executor/Executor.h
    executor/Executor.cpp

    storage/Table.h
    storage/Database.h
    storage/StorageEngine.h
    storage/StorageEngine.cpp

    catalog/Catalog.h
    catalog/Catalog.cpp

    web/HttpServer.h
    web/HttpServer.cpp
    web/WebApi.h
    web/WebApi.cpp

    auth/AuthManager.h
    auth/AuthManager.cpp
)

target_include_directories(chapadb_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Линкуем с Threads для std::thread
find_package(Threads REQUIRED)
target_link_libraries(chapadb_core PUBLIC Threads::Threads)

# Сетевой сервер
add_executable(chapadb_server
    network/Server.h
    network/Server.cpp
    main.cpp
)

target_link_libraries(chapadb_server PRIVATE chapadb_core)
