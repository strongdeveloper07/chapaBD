# Разделяемая клиентская библиотека
add_library(chapadb SHARED
    src/Client.cpp
    include/chapadb/Client.h
)

target_include_directories(chapadb PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(chapadb PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION 1
)
