#include "strut/lexer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace strut {
namespace {
constexpr std::array<std::string_view, 26> keywords = {
    "async", "await", "break", "case", "catch", "const", "continue",
    "default", "else", "enum", "extern", "for", "function", "if", "include", "match",
    "operator", "return", "struct", "switch", "throw", "try", "type", "unsafe",
    "void", "while"
};

constexpr std::array<std::string_view, 23> multi_ops = {
    "<<=", ">>=", "::", ":=", "->", "=>", "==", "!=", "<=", ">=", "<<", ">>",
    "++", "--", "+=", "-=", "*=", "/=", "%=", "&&", "||", "?.", "??"
};

bool is_identifier_start(char c) {
    const auto uc = static_cast<unsigned char>(c);
    return std::isalpha(uc) || c == '_';
}

bool is_identifier_continue(char c) {
    const auto uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) || c == '_';
}

bool is_keyword(std::string_view value) {
    return std::find(keywords.begin(), keywords.end(), value) != keywords.end();
}

bool is_single_operator(char c) {
    constexpr std::string_view ops = "+-*/%=<>!&|^~?";
    return ops.find(c) != std::string_view::npos;
}

bool is_punctuation(char c) {
    constexpr std::string_view punctuation = "(){}[];,:.";
    return punctuation.find(c) != std::string_view::npos;
}
}

Lexer::Lexer(const SourceFile& source) : source_(source) {}

bool Lexer::at_end() const { return offset_ >= source_.text().size(); }

char Lexer::peek(std::size_t lookahead) const {
    const auto index = offset_ + lookahead;
    return index < source_.text().size() ? source_.text()[index] : '\0';
}

char Lexer::advance() {
    const char c = source_.text()[offset_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (at_end() || peek() != expected) return false;
    advance();
    return true;
}

SourceLocation Lexer::location() const { return SourceLocation{offset_, line_, column_}; }
SourceSpan Lexer::span_from(const SourceLocation& begin) const { return SourceSpan{begin, location()}; }

void Lexer::add_token(LexResult& result, TokenKind kind, const SourceLocation& begin, std::size_t begin_offset) {
    result.tokens.push_back(Token{kind, source_.text().substr(begin_offset, offset_ - begin_offset), span_from(begin)});
}

void Lexer::add_error(LexResult& result, const SourceLocation& begin, std::string message) {
    result.diagnostics.push_back(Diagnostic{span_from(begin), std::move(message)});
}

void Lexer::skip_trivia(LexResult& result) {
    while (!at_end()) {
        const char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
            continue;
        }
        if (c == '/' && peek(1) == '/') {
            advance(); advance();
            while (!at_end() && peek() != '\n') advance();
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            const auto begin = location();
            advance(); advance();
            bool closed = false;
            while (!at_end()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance(); advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) add_error(result, begin, "unterminated block comment");
            continue;
        }
        break;
    }
}

void Lexer::lex_identifier(LexResult& result) {
    const auto begin = location();
    const auto begin_offset = offset_;
    advance();
    while (!at_end() && is_identifier_continue(peek())) advance();
    const std::string_view value(source_.text().data() + begin_offset, offset_ - begin_offset);
    if (value == "true" || value == "false") {
        add_token(result, TokenKind::boolean_literal, begin, begin_offset);
    } else if (value == "null") {
        add_token(result, TokenKind::null_literal, begin, begin_offset);
    } else {
        add_token(result, is_keyword(value) ? TokenKind::keyword : TokenKind::identifier, begin, begin_offset);
    }
}

void Lexer::lex_number(LexResult& result) {
    const auto begin = location();
    const auto begin_offset = offset_;
    bool floating = false;

    while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) advance();

    if (!at_end() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        floating = true;
        advance();
        while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
    }

    if (!at_end() && (peek() == 'e' || peek() == 'E')) {
        floating = true;
        advance();
        if (!at_end() && (peek() == '+' || peek() == '-')) advance();
        if (at_end() || !std::isdigit(static_cast<unsigned char>(peek()))) {
            while (!at_end() && is_identifier_continue(peek())) advance();
            add_error(result, begin, "malformed floating literal: exponent requires digits");
            return;
        }
        while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
    }

    if (!at_end() && is_identifier_start(peek())) {
        while (!at_end() && is_identifier_continue(peek())) advance();
        add_error(result, begin, "malformed numeric literal");
        return;
    }

    add_token(result, floating ? TokenKind::floating_literal : TokenKind::integer_literal, begin, begin_offset);
}

void Lexer::lex_string(LexResult& result) {
    const auto begin = location();
    const auto begin_offset = offset_;
    advance(); // opening quote
    bool valid = true;

    while (!at_end()) {
        const char c = peek();
        if (c == '"') {
            advance();
            if (valid) add_token(result, TokenKind::string_literal, begin, begin_offset);
            return;
        }
        if (c == '\n' || c == '\r') {
            add_error(result, begin, "unterminated string literal");
            return;
        }
        if (c == '\\') {
            const auto escape_begin = location();
            advance();
            if (at_end()) {
                add_error(result, begin, "unterminated string literal");
                return;
            }
            const char escaped = advance();
            switch (escaped) {
                case '\\': case '"': case 'n': case 'r': case 't': case '0':
                    break;
                default:
                    add_error(result, escape_begin, std::string("invalid string escape character '") + escaped + "'");
                    valid = false;
                    break;
            }
            continue;
        }
        advance();
    }

    add_error(result, begin, "unterminated string literal");
}

void Lexer::lex_punctuation_or_operator(LexResult& result) {
    const auto begin = location();
    const auto begin_offset = offset_;

    for (const auto op : multi_ops) {
        if (source_.text().compare(offset_, op.size(), op) == 0) {
            for (std::size_t i = 0; i < op.size(); ++i) advance();
            add_token(result, TokenKind::op, begin, begin_offset);
            return;
        }
    }

    const char c = peek();
    if (is_single_operator(c)) {
        advance();
        add_token(result, TokenKind::op, begin, begin_offset);
        return;
    }
    if (is_punctuation(c)) {
        advance();
        add_token(result, TokenKind::punctuation, begin, begin_offset);
        return;
    }

    advance();
    add_error(result, begin, std::string("unexpected character '") + c + "'");
}

LexResult Lexer::lex() {
    LexResult result;
    while (!at_end()) {
        skip_trivia(result);
        if (at_end()) break;
        if (is_identifier_start(peek())) {
            lex_identifier(result);
        } else if (std::isdigit(static_cast<unsigned char>(peek()))) {
            lex_number(result);
        } else if (peek() == '"') {
            lex_string(result);
        } else {
            lex_punctuation_or_operator(result);
        }
    }
    const auto here = location();
    result.tokens.push_back(Token{TokenKind::end_of_file, "", SourceSpan{here, here}});
    return result;
}

}
