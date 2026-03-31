# ======== NETWORK LIBRARY - TCP/IP Communication ========

set(NETWORK_SOURCES
    NetworkClient.h
    NetworkClient.cpp
)

add_library(network_lib STATIC ${NETWORK_SOURCES})

target_include_directories(network_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../shared
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core
)

target_link_libraries(network_lib PUBLIC
    ws2_32
)
