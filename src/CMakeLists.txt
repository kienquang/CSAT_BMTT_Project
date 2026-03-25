# ======== SOURCE SUBDIRECTORIES ========
# This organizes source files by category

add_subdirectory(core)
add_subdirectory(network)
add_subdirectory(apps)

# Build UI subdirectory if Qt is available
if(BUILD_QT_GUI)
    add_subdirectory(ui)
endif()
