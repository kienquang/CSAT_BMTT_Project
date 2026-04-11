# ======== CORE LIBRARY - Encryption & Database ========
# This library contains core functionality for encryption and database operations

set(CORE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/Base64.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Blowfish.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseHelper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EnvConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/masking.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PasswordHasher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SecureRandom.h
)

set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Base64.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Blowfish.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseHelper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EnvConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/masking.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PasswordHasher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SecureRandom.cpp
)

set(ARGON2_ROOT ${CMAKE_SOURCE_DIR}/third_party/argon2)
set(ARGON2_SOURCES
    ${ARGON2_ROOT}/src/argon2.c
    ${ARGON2_ROOT}/src/core.c
    ${ARGON2_ROOT}/src/encoding.c
    ${ARGON2_ROOT}/src/ref.c
    ${ARGON2_ROOT}/src/thread.c
    ${ARGON2_ROOT}/src/blake2/blake2b.c
)

add_library(argon2_lib STATIC ${ARGON2_SOURCES})

target_include_directories(argon2_lib PUBLIC
    ${ARGON2_ROOT}/include
    ${ARGON2_ROOT}/src
)

# Create static library for core functionality
add_library(core_lib STATIC ${CORE_SOURCES} ${CORE_HEADERS})

# Set include directories for core library
target_include_directories(core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MYSQL_INCLUDE_DIR}
    ${MYSQL_INCLUDE_DIR}/jdbc
    ${ARGON2_ROOT}/include
)

# Link MySQL libraries
target_link_directories(core_lib PRIVATE ${MYSQL_LIB_DIR})
target_link_libraries(core_lib PUBLIC mysqlcppconn argon2_lib)

if(WIN32)
    target_link_libraries(core_lib PUBLIC bcrypt)
endif()

