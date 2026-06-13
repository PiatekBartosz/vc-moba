cmake_minimum_required(VERSION 3.16)
project(practice_tool CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(practice src/main.cpp)

include(cmake/CPM.cmake)
CPMAddPackage("gh:raysan5/raylib#6.0")

find_package(raylib REQUIRED)

target_link_libraries(practice PRIVATE raylib)
