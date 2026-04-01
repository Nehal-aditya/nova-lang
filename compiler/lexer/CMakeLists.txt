cmake_minimum_required(VERSION 3.20)
project(nova_lexer C)
set(CMAKE_C_STANDARD 11)

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(nova_lexer STATIC
    src/lexer.c
    src/token.c
)
target_include_directories(nova_lexer PUBLIC include)

add_executable(test_lexer tests/test_lexer.c)
target_link_libraries(test_lexer nova_lexer)
enable_testing()
add_test(NAME lexer_tests COMMAND test_lexer)
