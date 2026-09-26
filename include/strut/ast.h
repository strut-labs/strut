#pragma once

#include <memory>
#include <optional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "strut/source.h"
#include "strut/type.h"

namespace strut {

struct TypeSyntax {
    std::string name;
    SourceSpan span;
    bool is_const = false;
    TypeId type_id;
    TypeSyntax() = default;
    TypeSyntax(std::string spelling, SourceSpan source_span, bool binding_const = false)
        : name(std::move(spelling)), span(source_span), is_const(binding_const), type_id(intern_type(name)) {}
};

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;
struct LambdaData;
struct Parameter { TypeSyntax type; std::string name; SourceSpan span; };

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;
struct Expr {
    enum class Kind {
        identifier, integer_literal, floating_literal, string_literal, boolean_literal, null_literal, array_literal, map_literal, tuple_literal, json_object,
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
struct SwitchCase { ExprPtr value; bool is_default = false; SourceSpan span; std::vector<StmtPtr> body; };
struct CatchClause { std::optional<TypeSyntax> type; std::string name; SourceSpan span; std::vector<StmtPtr> body; };

struct Stmt {
    enum class Kind { declaration, assignment, expression, block, if_stmt, while_stmt, switch_stmt, match_stmt, for_stmt, range_for, break_stmt, continue_stmt, return_stmt, throw_stmt, try_stmt, operator_decl, function_decl, type_alias, struct_decl, enum_decl, unsafe_stmt, include_stmt };
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
    std::vector<SwitchCase> switch_cases;
    std::vector<CatchClause> catches;
    std::vector<Parameter> parameters;
    std::vector<Parameter> fields;
    std::vector<std::string> generic_parameters;
    std::vector<std::string> bases;
    std::vector<std::string> enum_names;
    std::vector<std::string> enum_values;
    std::optional<TypeSyntax> return_type;
    std::vector<TypeSyntax> error_types;
    std::string owner;
    std::optional<TypeSyntax> alias_target;
    bool has_body = false;
    bool include_is_package = false;
    bool is_async = false;
    bool is_extern_c = false;
};

struct LambdaData {
    bool is_async = false;
    std::vector<Parameter> parameters;
    std::vector<Parameter> fields;
    std::vector<std::string> generic_parameters;
    std::vector<std::string> bases;
    ExprPtr expression_body;
    std::vector<StmtPtr> body;
};

struct Program {
    std::vector<StmtPtr> statements;
    std::vector<std::string> standard_modules;
    bool enforce_standard_modules = false;
};

} // namespace strut
