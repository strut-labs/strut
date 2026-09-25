#include <cstdlib>
#include <iostream>
#include <string>

#include "strut/lexer.h"
#include "strut/source.h"
#include "strut/token.h"

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
}

int main() {
    const std::string text =
        "// lead comment\r\n"
        "function main() -> void {\n"
        "    /* inside */ return;\n"
        "}\n";
    strut::SourceFile source("fixture.p", text);
    strut::Lexer lexer(source);
    auto result = lexer.lex();
    require(result.ok(), "lexer should accept skeleton fixture");
    require(result.tokens.size() == 11, "expected 10 tokens plus eof");
    require(result.tokens[0].kind == strut::TokenKind::keyword, "function keyword");
    require(result.tokens[0].lexeme == "function", "function lexeme");
    require(result.tokens[0].span.begin.line == 2 && result.tokens[0].span.begin.column == 1, "CRLF line tracking");
    require(result.tokens[1].kind == strut::TokenKind::identifier && result.tokens[1].lexeme == "main", "main identifier");
    require(result.tokens[4].kind == strut::TokenKind::op && result.tokens[4].lexeme == "->", "arrow operator");
    require(result.tokens[7].kind == strut::TokenKind::keyword && result.tokens[7].lexeme == "return", "return keyword");
    require(result.tokens.back().kind == strut::TokenKind::end_of_file, "eof token");

    strut::SourceFile bad("bad.p", "/* missing");
    strut::Lexer bad_lexer(bad);
    auto bad_result = bad_lexer.lex();
    require(!bad_result.ok(), "unterminated comment rejected");
    require(bad_result.diagnostics.front().span.begin.line == 1, "diagnostic span");

    require(strut::SourceFile::has_strut_extension("x.p"), ".p accepted");
    require(strut::SourceFile::has_strut_extension("x.h"), ".h accepted");
    require(!strut::SourceFile::has_strut_extension("x.cpp"), "foreign extension rejected");
    return 0;
}
