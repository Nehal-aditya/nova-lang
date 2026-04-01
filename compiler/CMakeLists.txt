cmake_minimum_required(VERSION 3.20)
project(nova_compiler_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Wall -Wextra -Wpedantic -g -fsanitize=address)
    add_link_options(-fsanitize=address)
else()
    add_compile_options(-O2 -Wall)
endif()

add_subdirectory(lexer)
add_subdirectory(parser)
