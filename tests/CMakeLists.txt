# ======== TEST EXECUTABLES ========
# This defines the test targets

# Test for MaskingLogic
add_executable(TestMasking
    ${CMAKE_CURRENT_SOURCE_DIR}/test_masking.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/MaskingLogic.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/MaskingLogic.cpp
)

target_include_directories(TestMasking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
)

# Optional: Additional test executable (commented out by default)
# add_executable(test_all
#     ${CMAKE_CURRENT_SOURCE_DIR}/main_test.cpp
# )
# 
# target_include_directories(test_all PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
#     ${CMAKE_CURRENT_SOURCE_DIR}/../src/network
#     ${MYSQL_INCLUDE_DIR}
#     ${MYSQL_INCLUDE_DIR}/jdbc
# )
# 
# target_link_libraries(test_all PRIVATE
#     core_lib
#     ws2_32
# )
# 
# copy_mysql_dlls(test_all)

# Enable testing
enable_testing()
add_test(NAME MaskingTest COMMAND TestMasking)
