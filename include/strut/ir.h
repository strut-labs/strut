#pragma once

#include <memory>
#include <string>
#include <vector>

#include "strut/ast.h"
#include "strut/diagnostic.h"
#include "strut/source.h"

namespace strut {

struct IRStmt;
using IRStmtPtr = std::unique_ptr<IRStmt>;
struct IRExpr;
using IRExprPtr = std::unique_ptr<IRExpr>;
struct IRExpr {
    enum class Kind { identifier, integer_literal, floating_literal, string_literal, boolean_literal, null_literal, array_literal, map_literal, json_object, unary, binary, grouping, member, safe_member, index, postfix, call, struct_literal, lambda };
    Kind kind = Kind::identifier;
    std::string text;
    std::string type_name;
    SourceSpan span;
    IRExprPtr left;
    IRExprPtr right;
    std::vector<IRExprPtr> arguments;
    std::vector<std::string> names;
    bool lambda_async = false;
    std::vector<Parameter> lambda_parameters;
    IRExprPtr lambda_expression;
    std::vector<IRStmtPtr> lambda_body;
};
struct IRStmt {
    enum class Kind { declaration, assignment, expression, block, if_stmt, while_stmt, for_stmt, range_for, break_stmt, continue_stmt, return_stmt, function_decl, type_alias, struct_decl };
    Kind kind = Kind::expression;
    SourceSpan span;
    std::string name;
    std::string op;
    std::string type_name;
    bool is_const = false;
    IRExprPtr value;
    IRExprPtr target;
    IRExprPtr condition;
    IRExprPtr increment;
    IRStmtPtr initializer;
    std::vector<IRStmtPtr> body;
    std::vector<IRStmtPtr> else_body;
    std::vector<Parameter> parameters;
    std::vector<Parameter> fields;
    std::vector<std::string> generic_parameters;
    std::string return_type;
    std::string owner;
    bool has_body = false;
};

struct IRProgram { std::vector<IRStmtPtr> statements; };

struct IRResult {
    IRProgram program;
    std::vector<Diagnostic> diagnostics;
    bool ok() const { return diagnostics.empty(); }
};

class IRLowerer {
public:
    IRResult lower(const Program& program);
};

} // namespace strut
