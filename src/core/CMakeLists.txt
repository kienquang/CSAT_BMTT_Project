# ======== CORE LIBRARY - Encryption & Database ========
# This library contains core functionality for encryption and database operations

set(CORE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/Blowfish.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseHelper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EncryptionConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EnvConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MaskingLogic.h
)

set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Blowfish.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseHelper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EncryptionConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EnvConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MaskingLogic.cpp
)

# Create static library for core functionality
add_library(core_lib STATIC ${CORE_SOURCES} ${CORE_HEADERS})

# Set include directories for core library
target_include_directories(core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MYSQL_INCLUDE_DIR}
    ${MYSQL_INCLUDE_DIR}/jdbc
)

# Link MySQL libraries
target_link_directories(core_lib PRIVATE ${MYSQL_LIB_DIR})
target_link_libraries(core_lib PUBLIC mysqlcppconn)

