# CLI клиент
add_executable(chapadb_cli
    main.cpp
)

target_link_libraries(chapadb_cli PRIVATE chapadb)
