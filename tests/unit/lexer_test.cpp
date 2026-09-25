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
    {
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
        require(result.tokens[0].span.begin.line == 2 && result.tokens[0].span.begin.column == 1, "CRLF line tracking");
        require(result.tokens[1].kind == strut::TokenKind::identifier && result.tokens[1].lexeme == "main", "main identifier");
        require(result.tokens[4].kind == strut::TokenKind::op && result.tokens[4].lexeme == "->", "arrow operator");
        require(result.tokens.back().kind == strut::TokenKind::end_of_file, "eof token");
    }

    {
        strut::SourceFile source("literals.p", "42 3.14 1e3 2.5e-2 \"hello\\nworld\\\"\" true false null");
        strut::Lexer lexer(source);
        auto result = lexer.lex();
        require(result.ok(), "valid literals accepted");
        require(result.tokens.size() == 9, "eight literals plus eof");
        require(result.tokens[0].kind == strut::TokenKind::integer_literal && result.tokens[0].lexeme == "42", "integer literal");
        require(result.tokens[1].kind == strut::TokenKind::floating_literal && result.tokens[1].lexeme == "3.14", "decimal literal");
        require(result.tokens[2].kind == strut::TokenKind::floating_literal && result.tokens[2].lexeme == "1e3", "exponent literal");
        require(result.tokens[4].kind == strut::TokenKind::string_literal, "string literal");
        require(result.tokens[5].kind == strut::TokenKind::boolean_literal && result.tokens[5].lexeme == "true", "true literal");
        require(result.tokens[6].kind == strut::TokenKind::boolean_literal && result.tokens[6].lexeme == "false", "false literal");
        require(result.tokens[7].kind == strut::TokenKind::null_literal, "null literal");
    }

    {
        strut::SourceFile bad("bad.p", "/* missing");
        strut::Lexer lexer(bad);
        auto result = lexer.lex();
        require(!result.ok(), "unterminated comment rejected");
    }

    {
        strut::SourceFile bad("bad-string.p", "\"bad\\q\"");
        strut::Lexer lexer(bad);
        auto result = lexer.lex();
        require(!result.ok(), "invalid escape rejected");
        require(result.diagnostics.front().span.begin.column == 5, "invalid escape location");
    }

    {
        strut::SourceFile bad("bad-number.p", "1e+");
        strut::Lexer lexer(bad);
        auto result = lexer.lex();
        require(!result.ok(), "malformed exponent rejected");
        require(result.diagnostics.front().message.find("exponent") != std::string::npos, "malformed exponent diagnostic");
    }

    require(strut::SourceFile::has_strut_extension("x.p"), ".p accepted");
    require(strut::SourceFile::has_strut_extension("x.h"), ".h accepted");
    require(!strut::SourceFile::has_strut_extension("x.cpp"), "foreign extension rejected");
    return 0;
}
