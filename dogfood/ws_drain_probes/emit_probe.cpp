// Test-only tool: parse probe.p and emit the CURRENT generated C++ via the public CppBackend API.
#include "strut/source.h"
#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/sema.h"
#include "strut/ir.h"
#include "strut/codegen.h"
#include <fstream>
#include <iostream>
#include <string>
int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "usage: emit_probe <probe.p> <out.cpp>\n"; return 2; }
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) { std::cerr << "cannot open " << argv[1] << "\n"; return 2; }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string source_path = argv[1];
    auto lexer = strut::Lexer(strut::SourceFile(source_path, text));
    auto tokens = lexer.lex();

    strut::Parser parser(tokens.tokens);
    auto ast = parser.parse();
    if (!ast.ok()) { for (auto& d : ast.diagnostics) std::cerr << d.message << "\n"; return 1; }
    strut::SemanticAnalyzer sema;
    auto analysis = sema.analyze(ast.program);
    if (!analysis.ok()) { std::cerr << "sema failed\n"; return 1; }
    strut::IRLowerer lower;
    auto ir = lower.lower(ast.program);
    if (!ir.ok()) { std::cerr << "lower failed\n"; return 1; }
    ir.program.source_path = source_path;
    strut::CppBackend backend;
    auto result = backend.generate(ir.program);
    if (!result.ok()) { std::cerr << result.error << "\n"; return 1; }
    std::ofstream out(argv[2], std::ios::binary);
    if (!out) { std::cerr << "cannot write " << argv[2] << "\n"; return 2; }
    out << result.cpp;
    return 0;
}
