cmake_minimum_required(VERSION 3.20)
project(strut VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STRUT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

enable_testing()

add_library(strut_compiler STATIC
    src/cli.cpp
    src/source.cpp
    src/token.cpp
    src/lexer.cpp
    src/parser.cpp
    src/diagnostic.cpp
    src/sema.cpp
    src/type.cpp
    src/ir.cpp
    src/codegen.cpp
)
target_include_directories(strut_compiler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jsonic/include
)
target_compile_definitions(strut_compiler PRIVATE STRUT_JSONIC_INCLUDE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/jsonic/include")

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

add_executable(strut_parser_tests tests/unit/parser_test.cpp)
target_link_libraries(strut_parser_tests PRIVATE strut_compiler)
add_test(NAME strut_parser_tests COMMAND strut_parser_tests)

add_executable(strut_diagnostic_tests tests/unit/diagnostic_test.cpp)
target_link_libraries(strut_diagnostic_tests PRIVATE strut_compiler)
add_test(NAME strut_diagnostic_tests COMMAND strut_diagnostic_tests)

add_executable(strut_sema_tests tests/unit/sema_test.cpp)
target_link_libraries(strut_sema_tests PRIVATE strut_compiler)
add_test(NAME strut_sema_tests COMMAND strut_sema_tests)

add_executable(strut_type_tests tests/unit/type_test.cpp)
target_link_libraries(strut_type_tests PRIVATE strut_compiler)
add_test(NAME strut_type_tests COMMAND strut_type_tests)

add_executable(strut_ir_tests tests/unit/ir_test.cpp)
target_link_libraries(strut_ir_tests PRIVATE strut_compiler)
add_test(NAME strut_ir_tests COMMAND strut_ir_tests)
add_executable(strut_codegen_tests tests/unit/codegen_test.cpp)
target_link_libraries(strut_codegen_tests PRIVATE strut_compiler)
add_test(NAME strut_codegen_tests COMMAND strut_codegen_tests)
