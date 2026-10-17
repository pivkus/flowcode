
add_library(flowcode-harness STATIC 
    src/FlowcodeHarness.cpp
    src/IoWorker.cpp
    src/Tools.cpp
    src/tools/BashTool.cpp
    src/Session.cpp
    src/NetworkWorker.cpp
    src/SessionHandle.cpp
    src/ExecutorWorker.cpp
)

set_target_properties(flowcode-harness PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

target_include_directories(flowcode-harness PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
    $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/json)

# Enable the C++ wrapper (reproc++).
set(REPROC++ ON CACHE BOOL "" FORCE)
add_subdirectory(vendor/reproc)

set(BUILD_CURL_EXE OFF)
set(CURL_USE_LIBPSL OFF)
add_subdirectory(vendor/curl)

target_link_libraries(flowcode-harness
    PUBLIC nlohmann_json
    PRIVATE reproc++ CURL::libcurl
)
