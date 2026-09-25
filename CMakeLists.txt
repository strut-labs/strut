cmake_minimum_required(VERSION 3.20)
project(strut VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STRUT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

enable_testing()

add_library(strut_compiler STATIC
    src/cli.cpp
    src/source.cpp
    src/token.cpp
    src/lexer.cpp
)
target_include_directories(strut_compiler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jsonic/include
)

if(MSVC)
    target_compile_options(strut_compiler PRIVATE /W4 /permissive-)
    if(STRUT_WARNINGS_AS_ERRORS)
        target_compile_options(strut_compiler PRIVATE /WX)
    endif()
else()
    target_compile_options(strut_compiler PRIVATE -Wall -Wextra -Wpedantic)
    if(STRUT_WARNINGS_AS_ERRORS)
        target_compile_options(strut_compiler PRIVATE -Werror)
    endif()
endif()

add_executable(strut src/main.cpp)
target_link_libraries(strut PRIVATE strut_compiler)

add_executable(strut_unit_tests tests/unit/cli_test.cpp)
target_link_libraries(strut_unit_tests PRIVATE strut_compiler)
add_test(NAME strut_unit_tests COMMAND strut_unit_tests)

add_executable(strut_lexer_tests tests/unit/lexer_test.cpp)
target_link_libraries(strut_lexer_tests PRIVATE strut_compiler)
add_test(NAME strut_lexer_tests COMMAND strut_lexer_tests)
