cmake_minimum_required(VERSION 3.16)

project(CSAT_BMTT_Project C CXX)

# ======== INSTALL PREFIX CONFIGURATION ========
# Set install prefix to build directory to prevent accidental system-wide installation
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install prefix" FORCE)

# ======== C++ CONFIGURATION ========
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ======== OUTPUT DIRECTORIES ========
# For Visual Studio, add configuration subdirectory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)

# ======== CMAKE OPTIONS ========
option(BUILD_TESTS "Build test executables" ON)
option(BUILD_NETWORK_APPS "Build Server/Client applications" ON)

# ======== MYSQL CONNECTOR CONFIGURATION ========
# Allow user to override MySQL path via CMake command line:
# cmake -DMYSQL_CONNECTOR_PATH="/path/to/mysql" ..
set(MYSQL_CONNECTOR_PATH "C:/mysql-connector-c++-8.3.0-winx64" 
    CACHE PATH "Path to MySQL Connector/C++ installation")

set(MYSQL_INCLUDE_DIR "${MYSQL_CONNECTOR_PATH}/include")
set(MYSQL_LIB_DIR "${MYSQL_CONNECTOR_PATH}/lib64/vs14")
set(MYSQL_DLL_DIR "${MYSQL_CONNECTOR_PATH}/lib64")

# Verify MySQL paths exist
if(NOT EXISTS "${MYSQL_INCLUDE_DIR}")
    message(WARNING "MySQL include directory not found: ${MYSQL_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${MYSQL_LIB_DIR}")
    message(WARNING "MySQL library directory not found: ${MYSQL_LIB_DIR}")
endif()

# ======== DLL COPY FUNCTION ========
function(copy_mysql_dlls TARGET_NAME)
    set(DLL_FILES
        "mysqlcppconn8-2-vs14.dll"
        "mysqlcppconn-9-vs14.dll"
        "libssl-3-x64.dll"
        "libcrypto-3-x64.dll"
    )
    
    foreach(DLL_FILE ${DLL_FILES})
        set(DLL_SOURCE "${MYSQL_DLL_DIR}/${DLL_FILE}")
        set(DLL_DEST "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${DLL_FILE}")
        
        if(EXISTS "${DLL_SOURCE}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL_SOURCE}" "${DLL_DEST}"
                COMMENT "Copy ${DLL_FILE}"
            )
        else()
            message(WARNING "DLL not found: ${DLL_SOURCE}")
        endif()
    endforeach()
endfunction()

# ======== SUBDIRECTORIES ========
add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ======== NETWORK APPLICATIONS (Server/Client) ========
if(BUILD_NETWORK_APPS)
    # ======== SERVER APPLICATION ========
    add_executable(Server 
        ${CMAKE_SOURCE_DIR}/src/network/TCP_Server.cpp
    )

    target_include_directories(Server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network
        ${MYSQL_INCLUDE_DIR}
        ${MYSQL_INCLUDE_DIR}/jdbc
    )

    target_link_directories(Server PRIVATE ${MYSQL_LIB_DIR})

    target_link_libraries(Server PRIVATE
        core_lib
        mysqlcppconn
        ws2_32
    )

    copy_mysql_dlls(Server)

    # ======== CLIENT APPLICATION ========
    add_executable(Client
        ${CMAKE_SOURCE_DIR}/src/network/TCP_Client.cpp
    )

    target_include_directories(Client PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network
        ${MYSQL_INCLUDE_DIR}
        ${MYSQL_INCLUDE_DIR}/jdbc
    )

    target_link_directories(Client PRIVATE ${MYSQL_LIB_DIR})

    target_link_libraries(Client PRIVATE
        core_lib
        mysqlcppconn
        ws2_32
    )

    copy_mysql_dlls(Client)
endif()

# ======== INSTALL RULES ========
install(TARGETS main
        RUNTIME DESTINATION bin)

if(BUILD_NETWORK_APPS)
    install(TARGETS Server Client
            RUNTIME DESTINATION bin)
endif()

if(BUILD_TESTS)
    install(TARGETS TestMasking
            RUNTIME DESTINATION bin/tests)
endif()

# ======== BUILD INFORMATION ========
message(STATUS "")
message(STATUS "========== BUILD CONFIGURATION ==========")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "MySQL Connector: ${MYSQL_CONNECTOR_PATH}")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "Build Tests: ${BUILD_TESTS}")
message(STATUS "Build Network Apps: ${BUILD_NETWORK_APPS}")
message(STATUS "==========================================")
message(STATUS "")
