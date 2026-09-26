#include <cstdlib>
#include <iostream>
#include <string>

#include "strut/codegen.h"
#include "strut/ir.h"
#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/runtime_components.h"
#include "strut/sema.h"
#include "strut/source.h"

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

strut::IRProgram compile_frontend(std::string source) {
    strut::SourceFile file("extensions.p", std::move(source));
    strut::Lexer lexer(file);
    auto lexed = lexer.lex();
    require(lexed.ok(), "extension source lexes");
    strut::Parser parser(lexed.tokens);
    auto parsed = parser.parse();
    require(parsed.ok(), "extension source parses");
    strut::SemanticAnalyzer analyzer;
    auto semantics = analyzer.analyze(parsed.program);
    if (!semantics.ok()) std::cerr << semantics.diagnostics.front().message << '\n';
    require(semantics.ok(), "extension source passes semantic analysis");
    strut::IRLowerer lowerer;
    return lowerer.lower(parsed.program).program;
}
}

int main() {
    auto errors = compile_frontend(R"STRUT(
error ValidationError { string message; int code; }
function validate(int value) -> int : ValidationError {
    if (value < 0) { throw ValidationError { message: "negative", code: 42 }; }
    return value;
}
function identity[T](T value) -> T : ValidationError { return value; }
function main() -> int {
    try { print(identity(validate(-1))); }
    catch (ValidationError err) { println(err.message); println(err.code); }
    return 0;
}
)STRUT");
    strut::CppBackend backend;
    auto error_cpp = backend.generate(errors);
    require(error_cpp.ok(), "custom error C++ generates");
    require(error_cpp.cpp.find("__strut_error.type == \"ValidationError\"") != std::string::npos,
            "custom error catch preserves nominal identity");
    require(error_cpp.cpp.find("strut_checked_error(\"ValidationError\",\"negative\",42)") != std::string::npos,
            "custom error payload lowers");

    auto atomics = compile_frontend(R"STRUT(
atomic<int> count := 0;
atomic<bool> running := true;
function increment() -> void { count.fetch_add(1); return; }
function main() -> int : ThreadError {
    worker := thread(increment); worker.join();
    count.compare_exchange(1, 2); count.fetch_sub(1); running.store(false);
    return count.load();
}
)STRUT");
    auto components = strut::analyze_runtime_components(atomics);
    require(components.contains(strut::RuntimeComponentId::atomics), "atomic runtime component selected");
    auto atomic_cpp = backend.generate(atomics);
    require(atomic_cpp.ok() && atomic_cpp.cpp.find("class strut_atomic") != std::string::npos,
            "atomic runtime generates");
    return 0;
}
