file(GLOB_RECURSE FLOWCODE_APP_SOURCES CONFIGURE_DEPENDS
    "include/*.h" 
    "include/*.hpp" 
    "src/*.cpp"
    "assets/*.qrc"
)

add_executable(flowcode-app ${FLOWCODE_APP_SOURCES})

if(APPLE AND EXISTS /opt/homebrew/opt/qt)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/qt)
endif()
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

set_target_properties(flowcode-app PROPERTIES AUTOMOC ON AUTORCC ON)

target_include_directories(flowcode-app PRIVATE include)

target_link_libraries(flowcode-app PRIVATE flowcode-harness Qt6::Widgets Qt6::Svg)

# Temporary development location; packaging will supply the prompt at release time.
target_compile_definitions(flowcode-app PRIVATE
    FLOWCODE_SYSTEM_PROMPT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/system-prompt.md"
)
