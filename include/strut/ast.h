#pragma once

#include <memory>
#include <optional>
#include <memory>
#include <string>
#include <vector>

#include "strut/source.h"

namespace strut {

struct TypeSyntax {
    std::string name;
    SourceSpan span;
    bool is_const = false;
};

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;
struct LambdaData;
struct Parameter { TypeSyntax type; std::string name; SourceSpan span; };

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;
struct Expr {
    enum class Kind {
        identifier, integer_literal, floating_literal, string_literal, boolean_literal, null_literal, array_literal, map_literal, json_object,
        unary, binary, grouping, member, safe_member, index, postfix, call, struct_literal, lambda
    };
    Kind kind;
    std::string text;
    SourceSpan span;
    ExprPtr left;
    ExprPtr right;
    std::vector<ExprPtr> arguments;
    std::vector<std::string> names;
    std::shared_ptr<LambdaData> lambda;
};
struct Stmt {
    enum class Kind { declaration, assignment, expression, block, if_stmt, while_stmt, for_stmt, range_for, break_stmt, continue_stmt, return_stmt, function_decl, type_alias, struct_decl, unsafe_stmt };
    Kind kind;
    SourceSpan span;
    std::string name;
    std::string op;
    std::optional<TypeSyntax> declared_type;
    bool is_const = false;
    ExprPtr value;
    ExprPtr target;
    ExprPtr condition;
    ExprPtr increment;
    StmtPtr initializer;
    std::vector<StmtPtr> body;
    std::vector<StmtPtr> else_body;
    std::vector<Parameter> parameters;
    std::vector<Parameter> fields;
    std::vector<std::string> generic_parameters;
    std::optional<TypeSyntax> return_type;
    std::string owner;
    std::optional<TypeSyntax> alias_target;
    bool has_body = false;
};

struct LambdaData {
    bool is_async = false;
    std::vector<Parameter> parameters;
    std::vector<Parameter> fields;
    std::vector<std::string> generic_parameters;
    ExprPtr expression_body;
    std::vector<StmtPtr> body;
};

struct Program { std::vector<StmtPtr> statements; };

} // namespace strut
