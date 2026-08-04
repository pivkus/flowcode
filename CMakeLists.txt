cmake_minimum_required(VERSION 3.25)

project(flowcode)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(BUILD_SHARED_LIBS OFF)

# Qt: moc/uic/rcc run automatically on sources that need them
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(APPLE AND EXISTS /opt/homebrew/opt/qt)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/qt)
endif()
find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(BUILD_CURL_EXE OFF)
set(CURL_USE_LIBPSL OFF)

add_subdirectory(vendor/curl)

# reproc: cross-platform process library. Enable the C++ wrapper (reproc++).
set(REPROC++ ON CACHE BOOL "" FORCE)
add_subdirectory(vendor/reproc)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/json)

add_executable(flowcode src/main.cpp src/core/SessionHandle.cpp src/core/Backend.cpp src/core/Tools.cpp src/ui/MainWindow.cpp src/Bridge.cpp src/ui/SessionWidget.cpp src/core/Session.cpp)
target_link_libraries(flowcode PRIVATE CURL::libcurl nlohmann_json Qt6::Widgets reproc++)
