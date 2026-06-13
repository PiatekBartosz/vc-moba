cmake_minimum_required(VERSION 3.25)
project(practice_tool CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CPM.cmake)

# Third-party: fetched via CPM and marked SYSTEM so raylib's own build warnings
# and its headers are excluded from our strict warning policy below.
CPMAddPackage(
    NAME raylib
    GITHUB_REPOSITORY raysan5/raylib
    GIT_TAG 6.0
    SYSTEM YES
    OPTIONS
        "BUILD_EXAMPLES OFF"
)

# Our warning policy lives in one INTERFACE target, linked only to our code.
add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow;-Wnon-virtual-dtor;-Wold-style-cast;-Wcast-align;-Wunused;-Woverloaded-virtual;-Wdouble-promotion;-Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/WX>
)

add_executable(practice
    src/main.cpp
    src/systems.cpp
    src/input.cpp
    src/render.cpp
)
target_include_directories(practice PRIVATE src)
target_link_libraries(practice PRIVATE raylib project_warnings)
