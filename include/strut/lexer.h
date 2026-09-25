#pragma once

#include <string>
#include <vector>

#include "strut/source.h"
#include "strut/token.h"

namespace strut {

struct Diagnostic {
    SourceSpan span;
    std::string message;
};

struct LexResult {
    std::vector<Token> tokens;
    std::vector<Diagnostic> diagnostics;

    bool ok() const { return diagnostics.empty(); }
};

class Lexer {
public:
    explicit Lexer(const SourceFile& source);
    LexResult lex();

private:
    bool at_end() const;
    char peek(std::size_t lookahead = 0) const;
    char advance();
    bool match(char expected);
    SourceLocation location() const;
    SourceSpan span_from(const SourceLocation& begin) const;

    void skip_trivia(LexResult& result);
    void lex_identifier(LexResult& result);
    void lex_punctuation_or_operator(LexResult& result);
    void add_token(LexResult& result, TokenKind kind, const SourceLocation& begin, std::size_t begin_offset);
    void add_error(LexResult& result, const SourceLocation& begin, std::string message);

    const SourceFile& source_;
    std::size_t offset_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

}
