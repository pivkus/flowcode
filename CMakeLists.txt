cmake_minimum_required(VERSION 3.25)

project(flowcode)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(BUILD_SHARED_LIBS OFF)

add_subdirectory(flowcode-harness)

option(BUILD_APP "Build the app frontend" ON)
if(BUILD_APP)
  add_subdirectory(flowcode-app)
endif()