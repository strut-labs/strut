#pragma once

#include <vector>

#include "strut/ast.h"
#include "strut/lexer.h"
#include "strut/token.h"

namespace strut {

struct ParseResult {
    Program program;
    std::vector<Diagnostic> diagnostics;
    bool ok() const { return diagnostics.empty(); }
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    ParseResult parse();

private:
    const Token& peek(std::size_t lookahead = 0) const;
    const Token& previous() const;
    bool at_end() const;
    const Token& advance();
    bool check(std::string_view lexeme) const;
    bool match(std::string_view lexeme);
    bool check_kind(TokenKind kind) const;
    void error(ParseResult& result, const Token& token, std::string message);
    void synchronize();
    ExprPtr parse_primary(ParseResult& result);
    StmtPtr parse_statement(ParseResult& result);
    StmtPtr parse_declaration_or_assignment(ParseResult& result);

    const std::vector<Token>& tokens_;
    std::size_t current_ = 0;
};

} // namespace strut
