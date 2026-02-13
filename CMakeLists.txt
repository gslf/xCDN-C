cmake_minimum_required(VERSION 3.10)
project(xcdn C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Library sources
set(XCDN_SOURCES
    src/error.c
    src/ast.c
    src/lexer.c
    src/parser.c
    src/ser.c
)

set(XCDN_HEADERS
    src/xcdn.h
    src/error.h
    src/ast.h
    src/lexer.h
    src/parser.h
    src/ser.h
)

# Static library
add_library(xcdn STATIC ${XCDN_SOURCES})
target_include_directories(xcdn PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Tests
enable_testing()

add_executable(test_lexer tests/test_lexer.c)
target_link_libraries(test_lexer xcdn)
add_test(NAME test_lexer COMMAND test_lexer)

add_executable(test_parser tests/test_parser.c)
target_link_libraries(test_parser xcdn)
add_test(NAME test_parser COMMAND test_parser)

add_executable(test_ser tests/test_ser.c)
target_link_libraries(test_ser xcdn)
add_test(NAME test_ser COMMAND test_ser)

add_executable(test_basic tests/test_basic.c)
target_link_libraries(test_basic xcdn)
add_test(NAME test_basic COMMAND test_basic)

# Examples
add_executable(example_roundtrip examples/roundtrip.c)
target_link_libraries(example_roundtrip xcdn)

add_executable(example_fields_access examples/fields_access.c)
target_link_libraries(example_fields_access xcdn)

add_executable(example_dict_like examples/dict_like_operations.c)
target_link_libraries(example_dict_like xcdn)
