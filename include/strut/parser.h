#pragma once

#include <string_view>
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
    void error(ParseResult& result, const Token& token, std::string message);
    void synchronize();

    ExprPtr parse_expression(ParseResult& result, int min_precedence = 0);
    ExprPtr parse_unary(ParseResult& result);
    ExprPtr parse_postfix(ParseResult& result);
    ExprPtr parse_primary(ParseResult& result);
    ExprPtr parse_lambda(ParseResult& result, bool is_async);
    StmtPtr parse_typed_function_value(ParseResult& result);
    StmtPtr parse_type_alias(ParseResult& result);
    StmtPtr parse_statement(ParseResult& result);
    StmtPtr parse_block(ParseResult& result);
    StmtPtr parse_if(ParseResult& result);
    StmtPtr parse_while(ParseResult& result);
    StmtPtr parse_for(ParseResult& result);
    StmtPtr parse_function(ParseResult& result);
    StmtPtr parse_struct(ParseResult& result);
    StmtPtr parse_include(ParseResult& result);
    TypeSyntax parse_type(ParseResult& result);
    StmtPtr parse_declaration_or_assignment(ParseResult& result);

    static int precedence(std::string_view op);
    static bool is_binary_operator(std::string_view op);
    static bool is_assignment_operator(std::string_view op);

    const std::vector<Token>& tokens_;
    std::size_t current_ = 0;
};

} // namespace strut
