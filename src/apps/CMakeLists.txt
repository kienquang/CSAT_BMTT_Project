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
