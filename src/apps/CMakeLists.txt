# ======== APPLICATION EXECUTABLES ========
# This defines the executable targets for the project

# Main application executable
add_executable(main 
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../network
    ${MYSQL_INCLUDE_DIR}
    ${MYSQL_INCLUDE_DIR}/jdbc
)

target_link_directories(main PRIVATE ${MYSQL_LIB_DIR})

target_link_libraries(main PRIVATE 
    core_lib
    mysqlcppconn
    ws2_32
)

# Copy DLL tự động cho main
copy_mysql_dlls(main)

# Test initialization executable
add_executable(test_init 
    ${CMAKE_CURRENT_SOURCE_DIR}/test_init.cpp
)

target_include_directories(test_init PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${MYSQL_INCLUDE_DIR}
    ${MYSQL_INCLUDE_DIR}/jdbc
)

target_link_directories(test_init PRIVATE ${MYSQL_LIB_DIR})

target_link_libraries(test_init PRIVATE 
    core_lib
    mysqlcppconn
    ws2_32
)

# Copy DLL tự động cho test_init
copy_mysql_dlls(test_init)
