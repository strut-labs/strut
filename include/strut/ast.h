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

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;
struct Stmt {
    enum class Kind { declaration, assignment, expression };
    Kind kind;
    SourceSpan span;
    std::string name;
    std::string op;
    std::optional<TypeSyntax> declared_type;
    bool is_const = false;
    ExprPtr value;
};

struct Program { std::vector<StmtPtr> statements; };

} // namespace strut
