cmake_minimum_required(VERSION 3.14)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

function(add_chapadb_test target_name source_file)
    add_executable(${target_name} ${source_file})
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(${target_name} PRIVATE chapadb_core gtest_main)
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

add_chapadb_test(test_lexer       test_lexer.cpp)
add_chapadb_test(test_parser      test_parser.cpp)
add_chapadb_test(test_executor    test_executor.cpp)
add_chapadb_test(test_persistence test_persistence.cpp)

# Запуск всех тестов одной командой: cmake --build build --target run_tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_lexer test_parser test_executor test_persistence
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
