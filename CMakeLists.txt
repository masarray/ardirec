cmake_minimum_required(VERSION 3.24)

project(ardirec VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ARDIREC_BUILD_DESKTOP "Build the Qt Quick desktop application" ON)
option(ARDIREC_BUILD_TESTS "Build unit tests" ON)
option(ARDIREC_ENABLE_SANITIZERS "Enable ASan/UBSan on supported compilers" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ProjectWarnings)
include(Sanitizers)

add_subdirectory(core)
add_subdirectory(cli)

if(ARDIREC_BUILD_DESKTOP)
    find_package(Qt6 6.8 QUIET COMPONENTS Quick Qml QuickControls2)
    if(Qt6_FOUND)
        add_subdirectory(apps/desktop)
    else()
        message(WARNING "Qt 6.8+ not found; ardirec desktop target will be skipped.")
    endif()
endif()

if(ARDIREC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
