cmake_minimum_required(VERSION 3.16)
project(FastX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 QUIET)

add_executable(FastX src/main.cpp)

if (SDL2_FOUND)
    target_compile_definitions(FastX PRIVATE FASTX_USE_SDL=1)
    target_include_directories(FastX PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(FastX PRIVATE ${SDL2_LIBRARIES})
else()
    message(WARNING "SDL2 not found. Building headless mode (renders single frame to output.ppm).")
endif()

if (MSVC)
    target_compile_options(FastX PRIVATE /W4 /permissive-)
else()
    target_compile_options(FastX PRIVATE -Wall -Wextra -Wpedantic -O3)
endif()
