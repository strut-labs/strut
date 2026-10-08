cmake_minimum_required(VERSION 3.20)
project(strut VERSION 0.0.4 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STRUT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(STRUT_SANITIZERS "Enable address/undefined sanitizers on supported compilers" OFF)

enable_testing()

if(STRUT_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(strut_compiler STATIC
    src/cli.cpp
    src/source.cpp
    src/token.cpp
    src/lexer.cpp
    src/parser.cpp
    src/diagnostic.cpp
    src/sema.cpp
    src/type.cpp
    src/operator.cpp
    src/ir.cpp
    src/codegen.cpp
    src/generated_runtime.cpp
    src/runtime_components.cpp
    src/api_registry.cpp
    src/package.cpp
    src/project.cpp
    src/formatter.cpp
    src/lsp.cpp
)
target_include_directories(strut_compiler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jsonic/include
)
target_compile_definitions(strut_compiler PRIVATE
    STRUT_HOST_CXX="${CMAKE_CXX_COMPILER}"
)

# Keep generated-program support beside the build-tree compiler using the same
# relative layout as an installed compiler. This avoids embedding checkout paths.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/share/strut/jsonic")
configure_file(third_party/jsonic/include/json.h
    "${CMAKE_CURRENT_BINARY_DIR}/share/strut/jsonic/json.h" COPYONLY)

if(MSVC)
    target_compile_definitions(strut_compiler PRIVATE _CRT_SECURE_NO_WARNINGS)
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

install(TARGETS strut RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES third_party/jsonic/include/json.h
    DESTINATION ${CMAKE_INSTALL_DATADIR}/strut/jsonic)
install(FILES LICENSE BUILDING.md COMPATIBILITY.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES third_party/jsonic/LICENSE third_party/jsonic/README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/third-party/jsonic)

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
add_executable(strut_runtime_component_tests tests/unit/runtime_components_test.cpp)
target_link_libraries(strut_runtime_component_tests PRIVATE strut_compiler)
add_test(NAME strut_runtime_component_tests COMMAND strut_runtime_component_tests)
add_executable(strut_language_extensions_tests tests/unit/language_extensions_test.cpp)
target_link_libraries(strut_language_extensions_tests PRIVATE strut_compiler)
add_test(NAME strut_language_extensions_tests COMMAND strut_language_extensions_tests)
add_executable(strut_api_registry_tests tests/unit/api_registry_test.cpp)
target_link_libraries(strut_api_registry_tests PRIVATE strut_compiler)
add_test(NAME strut_api_registry_tests COMMAND strut_api_registry_tests)

add_executable(strut_package_tests tests/unit/package_test.cpp)
target_link_libraries(strut_package_tests PRIVATE strut_compiler)
add_test(NAME strut_package_tests COMMAND strut_package_tests)

add_executable(strut_project_tests tests/unit/project_test.cpp)
target_link_libraries(strut_project_tests PRIVATE strut_compiler)
add_test(NAME strut_project_tests COMMAND strut_project_tests)

add_executable(strut_formatter_tests tests/unit/formatter_test.cpp)
target_link_libraries(strut_formatter_tests PRIVATE strut_compiler)
add_test(NAME strut_formatter_tests COMMAND strut_formatter_tests)

add_executable(strut_lsp_tests tests/unit/lsp_test.cpp)
target_link_libraries(strut_lsp_tests PRIVATE strut_compiler)
add_test(NAME strut_lsp_tests COMMAND strut_lsp_tests)

add_executable(strut_frontend_fuzz_tests tests/fuzz/frontend_fuzz.cpp)
target_link_libraries(strut_frontend_fuzz_tests PRIVATE strut_compiler)
add_test(NAME strut_frontend_fuzz_tests COMMAND strut_frontend_fuzz_tests)
set_tests_properties(strut_frontend_fuzz_tests PROPERTIES TIMEOUT 30)
