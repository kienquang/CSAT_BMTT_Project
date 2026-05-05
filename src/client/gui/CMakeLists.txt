# ===== Qt UI Library =====

set(UI_SOURCES
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
    MedicalRecordDetailDialog.cpp
    MedicalRecordDetailDialog.h
    EmployeeDialog.cpp
    EmployeeDialog.h
    LoginDialog.cpp
    LoginDialog.h
)

add_library(csat_ui STATIC ${UI_SOURCES})

target_include_directories(csat_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../network
    ${CMAKE_CURRENT_SOURCE_DIR}/../../shared
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(csat_ui PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    core_lib
    network_lib
)

# Qt MOC, UIC, RCC
set_target_properties(csat_ui PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
