#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "strut/source.h"

namespace strut {

struct TypeSyntax {
    std::string name;
    SourceSpan span;
    bool is_const = false;
};

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;
struct Expr {
    enum class Kind {
        identifier, integer_literal, floating_literal, string_literal, boolean_literal, null_literal,
        unary, binary, grouping, member, index, postfix
    };
    Kind kind;
    std::string text;
    SourceSpan span;
    ExprPtr left;
    ExprPtr right;
};

struct Parameter { TypeSyntax type; std::string name; SourceSpan span; };

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;
struct Stmt {
    enum class Kind { declaration, assignment, expression, block, if_stmt, while_stmt, for_stmt, range_for, break_stmt, continue_stmt, return_stmt, function_decl };
    Kind kind;
    SourceSpan span;
    std::string name;
    std::string op;
    std::optional<TypeSyntax> declared_type;
    bool is_const = false;
    ExprPtr value;
    ExprPtr condition;
    ExprPtr increment;
    StmtPtr initializer;
    std::vector<StmtPtr> body;
    std::vector<StmtPtr> else_body;
    std::vector<Parameter> parameters;
    std::vector<std::string> generic_parameters;
    std::optional<TypeSyntax> return_type;
    std::string owner;
    bool has_body = false;
};

struct Program { std::vector<StmtPtr> statements; };

} // namespace strut
