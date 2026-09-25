#include "strut/parser.h"

#include <string>

namespace strut {
namespace {
bool is_type_token(const Token& token) {
    return token.kind == TokenKind::identifier ||
           (token.kind == TokenKind::keyword && token.lexeme == "void");
}
}

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

const Token& Parser::peek(std::size_t lookahead) const {
    const auto index = current_ + lookahead;
    return tokens_[index < tokens_.size() ? index : tokens_.size() - 1];
}
const Token& Parser::previous() const { return tokens_[current_ - 1]; }
bool Parser::at_end() const { return peek().kind == TokenKind::end_of_file; }
const Token& Parser::advance() { if (!at_end()) ++current_; return previous(); }
bool Parser::check(std::string_view lexeme) const { return !at_end() && peek().lexeme == lexeme; }
bool Parser::match(std::string_view lexeme) { if (!check(lexeme)) return false; advance(); return true; }
bool Parser::check_kind(TokenKind kind) const { return !at_end() && peek().kind == kind; }

void Parser::error(ParseResult& result, const Token& token, std::string message) {
    result.diagnostics.push_back(Diagnostic{token.span, std::move(message)});
}

void Parser::synchronize() {
    while (!at_end()) {
        if (previous().lexeme == ";") return;
        if (peek().lexeme == "const" || peek().kind == TokenKind::identifier) return;
        advance();
    }
}

ExprPtr Parser::parse_primary(ParseResult& result) {
    const Token token = peek();
    Expr::Kind kind;
    switch (token.kind) {
        case TokenKind::identifier: kind = Expr::Kind::identifier; break;
        case TokenKind::integer_literal: kind = Expr::Kind::integer_literal; break;
        case TokenKind::floating_literal: kind = Expr::Kind::floating_literal; break;
        case TokenKind::string_literal: kind = Expr::Kind::string_literal; break;
        case TokenKind::boolean_literal: kind = Expr::Kind::boolean_literal; break;
        case TokenKind::null_literal: kind = Expr::Kind::null_literal; break;
        default:
            error(result, token, "expected expression");
            return nullptr;
    }
    advance();
    return std::make_unique<Expr>(Expr{kind, token.lexeme, token.span});
}

StmtPtr Parser::parse_declaration_or_assignment(ParseResult& result) {
    const Token begin = peek();
    bool is_const = match("const");
    if (at_end()) {
        error(result, peek(), "expected declaration after 'const'");
        return nullptr;
    }

    std::optional<TypeSyntax> declared_type;
    Token name;

    if (is_type_token(peek()) && peek(1).kind == TokenKind::identifier && peek(2).lexeme == ":=") {
        const Token type = advance();
        declared_type = TypeSyntax{type.lexeme, type.span, false};
        name = advance();
    } else if (peek().kind == TokenKind::identifier && (peek(1).lexeme == ":=" || peek(1).lexeme == "=")) {
        name = advance();
    } else {
        error(result, peek(), "expected declaration or assignment");
        return nullptr;
    }

    const bool declaration = match(":=");
    if (!declaration && !match("=")) {
        error(result, peek(), "expected ':=' or '='");
        return nullptr;
    }
    if (is_const && !declaration) {
        error(result, begin, "'const' may only be used on a declaration");
        return nullptr;
    }

    auto value = parse_primary(result);
    if (!value) return nullptr;

    if (!match(";")) {
        error(result, peek(), "expected ';' after statement");
        return nullptr;
    }

    auto statement = std::make_unique<Stmt>();
    statement->kind = declaration ? Stmt::Kind::declaration : Stmt::Kind::assignment;
    statement->span = SourceSpan{begin.span.begin, previous().span.end};
    statement->name = name.lexeme;
    statement->declared_type = std::move(declared_type);
    statement->is_const = is_const;
    statement->value = std::move(value);
    return statement;
}

StmtPtr Parser::parse_statement(ParseResult& result) {
    return parse_declaration_or_assignment(result);
}

ParseResult Parser::parse() {
    ParseResult result;
    while (!at_end()) {
        const auto before = current_;
        auto statement = parse_statement(result);
        if (statement) result.program.statements.push_back(std::move(statement));
        if (current_ == before) advance();
        if (!result.diagnostics.empty()) synchronize();
    }
    return result;
}

} // namespace strut
