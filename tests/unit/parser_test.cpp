#include <cstdlib>
#include <iostream>
#include <string>

#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/source.h"

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
strut::ParseResult parse(std::string text) {
    strut::SourceFile source("fixture.p", std::move(text));
    strut::Lexer lexer(source);
    auto lexed = lexer.lex();
    require(lexed.ok(), "fixture should lex");
    strut::Parser parser(lexed.tokens);
    return parser.parse();
}
}

int main() {
    auto ok = parse("x := 2; int y := 3; const z := 4; const int q := 5; y = x;");
    require(ok.ok(), "declaration fixture parses");
    require(ok.program.statements.size() == 5, "five statements parsed");
    require(!ok.program.statements[0]->declared_type.has_value(), "inferred declaration");
    require(ok.program.statements[1]->declared_type->name == "int", "typed declaration");
    require(ok.program.statements[2]->is_const, "const inferred declaration");
    require(ok.program.statements[3]->is_const && ok.program.statements[3]->declared_type->name == "int", "typed const");
    require(ok.program.statements[4]->kind == strut::Stmt::Kind::assignment, "assignment");

    auto missing = parse("x := 2");
    require(!missing.ok(), "missing semicolon rejected");
    require(missing.diagnostics.front().message.find("';'") != std::string::npos, "semicolon diagnostic");

    auto cppish = parse("int *x := 2;");
    require(!cppish.ok(), "C-style declarator spelling rejected");
    return 0;
}
