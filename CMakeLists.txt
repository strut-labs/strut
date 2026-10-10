cmake_minimum_required(VERSION 3.20)
project(strut VERSION 0.0.5 LANGUAGES CXX C)

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
    src/embed.cpp
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
add_library(strut_embed SHARED
    src/cli.cpp src/source.cpp src/token.cpp src/lexer.cpp src/parser.cpp src/diagnostic.cpp
    src/sema.cpp src/type.cpp src/operator.cpp src/ir.cpp src/codegen.cpp
    src/generated_runtime.cpp src/runtime_components.cpp src/api_registry.cpp
    src/package.cpp src/project.cpp src/formatter.cpp src/lsp.cpp src/embed.cpp
)
target_include_directories(strut_embed
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jsonic/include
)
target_compile_definitions(strut_embed PRIVATE STRUT_HOST_CXX="${CMAKE_CXX_COMPILER}" STRUT_EMBED_BUILD_SHARED=true)
set_target_properties(strut_embed PROPERTIES OUTPUT_NAME strut_embed)


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
install(TARGETS strut_embed
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/strut DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
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

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME strut_ffi_export_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_export_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_multilib_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_multilib_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_multilib_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_aggregate_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_aggregate_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_aggregate_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_aggregate_bidir_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_aggregate_bidir_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_aggregate_bidir_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_module_slug_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_ffi_module_slug_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_module_slug_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_pointer_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_pointer_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_pointer_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_pointer_bidir_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_pointer_bidir_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_pointer_bidir_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_abi_identity_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_ffi_abi_identity_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_abi_identity_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_callback_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_callback_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_callback_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_callback_bidir_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_callback_bidir_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_callback_bidir_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_error_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_error_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_error_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_error_bidir_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_error_bidir_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_error_bidir_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_fallback_callback_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_fallback_callback_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_fallback_callback_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_fallback_callback_bidir_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_fallback_callback_bidir_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_fallback_callback_bidir_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_retained_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_retained_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_retained_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_retained_b_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_retained_b_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_retained_b_tests PROPERTIES TIMEOUT 300)
    add_test(NAME strut_ffi_retained_f_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ffi/run_export_retained_f_tests.py $<TARGET_FILE:strut>)
    set_tests_properties(strut_ffi_retained_f_tests PROPERTIES TIMEOUT 300)
endif()
add_executable(strut_embed_host tests/embed/embed_host.c)
set_target_properties(strut_embed_host PROPERTIES LINKER_LANGUAGE CXX)
find_package(Threads REQUIRED)
target_link_libraries(strut_embed_host PRIVATE strut_compiler ${CMAKE_DL_LIBS} Threads::Threads)
add_test(NAME strut_embed_tests COMMAND strut_embed_host)
set_tests_properties(strut_embed_tests PROPERTIES TIMEOUT 300)
if(Python3_EXECUTABLE AND NOT STRUT_SANITIZERS)
    # The Python consumer loads the shared embedding library with ctypes; when that library is
    # ASan-instrumented and dlopened by a non-ASan interpreter the cross-DSO allocator order is
    # not guaranteed. The Python consumer therefore runs in the ordinary GCC/Clang builds and
    # in CI; C-harness embedding coverage remains fully ASan/UBSan-clean.
    add_test(NAME strut_ffi_embed_python COMMAND ${CMAKE_COMMAND} -E env
        "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:strut_embed>"
        "STRUT_EMBED_LIB=$<TARGET_FILE:strut_embed>"
        "STRUT_JSONIC_INCLUDE_DIR=${CMAKE_BINARY_DIR}/share/strut/jsonic"
        "ASAN_OPTIONS=verify_asan_link_order=0"
        ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/embed/python/strut_consumer.py)
    set_tests_properties(strut_ffi_embed_python PROPERTIES TIMEOUT 300)
endif()
find_program(GO_EXECUTABLE NAMES go)
if(GO_EXECUTABLE AND NOT STRUT_SANITIZERS)
    add_test(NAME strut_ffi_embed_go COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/embed/go/run_go_consumer.py
        $<TARGET_FILE_DIR:strut_embed>
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/share/strut/jsonic
        ${CMAKE_SOURCE_DIR}/tests/embed/go)
    set_tests_properties(strut_ffi_embed_go PROPERTIES TIMEOUT 300)
endif()
find_program(NODE_EXECUTABLE NAMES node nodejs)
if(NODE_EXECUTABLE AND NOT STRUT_SANITIZERS)
    # 9-NODE: build the dependency-free N-API addon with node-gyp, then execute the JS consumer
    # against the real shared embedding library (build and run must both pass).
    add_test(NAME strut_ffi_embed_node COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/embed/node/run_node_consumer.py
        $<TARGET_FILE_DIR:strut_embed>
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/share/strut/jsonic)
    set_tests_properties(strut_ffi_embed_node PROPERTIES TIMEOUT 600 SKIP_RETURN_CODE 77)
endif()
find_program(DOTNET_EXECUTABLE NAMES dotnet)
if(DOTNET_EXECUTABLE AND NOT STRUT_SANITIZERS)
    # 9-CSHARP: build with dotnet, then execute the P/Invoke consumer against the real shared
    # embedding library (build and run must both pass). The ABI-safe float type is double_64;
    # bool is not an ABI-exportable type in Strut, so the BOOL kind is not exercised here.
    add_test(NAME strut_ffi_embed_csharp COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/embed/csharp/run_csharp_consumer.py
        $<TARGET_FILE_DIR:strut_embed>
        ${CMAKE_BINARY_DIR}/share/strut/jsonic)
    set_tests_properties(strut_ffi_embed_csharp PROPERTIES TIMEOUT 600 SKIP_RETURN_CODE 77)
endif()
find_library(NIFT_C_LIBRARY NAMES nift_c HINTS ${STRUT_NIFT_LIBDIR} $ENV{STRUT_NIFT_LIBDIR})
if(NIFT_C_LIBRARY AND NOT STRUT_SANITIZERS)
    # FFI-10 Nift <-> Strut vertical slice: one host links libnift_c (Nift C ABI 1.3) and
    # libstrut_embed, evaluating real Nift expressions against values from a real Strut
    # module invocation and feeding Nift results back into Strut. Skipped where Nift's C ABI
    # distribution is absent.
    get_filename_component(NIFT_C_LIB_DIR ${NIFT_C_LIBRARY} DIRECTORY)
    find_path(NIFT_C_INCLUDE_DIR nift/c_abi.h HINTS ${NIFT_C_LIB_DIR}/../include
        ${STRUT_NIFT_INCLUDEDIR} $ENV{STRUT_NIFT_INCLUDEDIR})
    if(NIFT_C_INCLUDE_DIR)
        add_executable(strut_ffi_nift_strut tests/embed/nift/nift_strut_test.c)
        target_include_directories(strut_ffi_nift_strut PRIVATE ${NIFT_C_INCLUDE_DIR})
        set_target_properties(strut_ffi_nift_strut PROPERTIES LINKER_LANGUAGE CXX)
        target_link_libraries(strut_ffi_nift_strut PRIVATE strut_embed ${NIFT_C_LIBRARY}
            ${CMAKE_DL_LIBS})
        add_test(NAME strut_ffi_nift_strut COMMAND ${CMAKE_COMMAND} -E env
            "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:strut_embed>:${NIFT_C_LIB_DIR}"
            "DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:strut_embed>:${NIFT_C_LIB_DIR}"
            "STRUT_JSONIC_INCLUDE_DIR=${CMAKE_BINARY_DIR}/share/strut/jsonic"
            $<TARGET_FILE:strut_ffi_nift_strut>)
        set_tests_properties(strut_ffi_nift_strut PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    endif()
    # FFI-10B: Strut source originates a call into Nift (extern "C" -> libnift_c ABI 1.3),
    # compiled with the strut CLI's native link options and executed; expects 42.
    if(NIFT_C_INCLUDE_DIR)
        add_test(NAME strut_ffi_nift_from_strut COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/embed/nift/run_nift_from_strut.py
            ${CMAKE_BINARY_DIR}
            ${NIFT_C_LIB_DIR}
            ${CMAKE_SOURCE_DIR}/tests/embed/nift)
        set_tests_properties(strut_ffi_nift_from_strut PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    endif()
    # FFI-10A: Nift source origins a call into a Strut-exported C function (ffi_open/ffi_call
    # inside the evaluated Nift script targeting the Strut shared library's exported add).
    if(NIFT_C_INCLUDE_DIR)
        add_test(NAME strut_ffi_nift_to_strut COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/embed/nift/run_nift_to_strut.py
            ${CMAKE_BINARY_DIR}
            ${NIFT_C_LIB_DIR}
            ${CMAKE_SOURCE_DIR}/tests/embed/nift)
        set_tests_properties(strut_ffi_nift_to_strut PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    endif()
    # FFI-10: genuine Strut-source binary round trip through Nift's byte engine.
    if(NIFT_C_INCLUDE_DIR)
        add_test(NAME strut_ffi_nift_bytes_rt COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/embed/nift/run_nift_bytes_rt.py
            ${CMAKE_BINARY_DIR}
            ${NIFT_C_LIB_DIR}
            ${CMAKE_SOURCE_DIR}/tests/embed/nift)
        set_tests_properties(strut_ffi_nift_bytes_rt PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    endif()
endif()
