#include "strut/ir.h"

#include <unordered_map>

#include "strut/type.h"

namespace strut {
namespace {
std::string literal_type(const Expr& expr) {
    switch (expr.kind) {
        case Expr::Kind::integer_literal: { auto t = infer_integer_literal(expr.text); return t.name; }
        case Expr::Kind::floating_literal: { auto t = infer_floating_literal(expr.text); return t.name; }
        case Expr::Kind::string_literal: return "string";
        case Expr::Kind::boolean_literal: return "bool";
        case Expr::Kind::null_literal: return "null";
        case Expr::Kind::lambda: return "function";
        default: return "opaque";
    }
}
IRExpr::Kind convert_expr_kind(Expr::Kind kind) { return static_cast<IRExpr::Kind>(kind); }
IRStmt::Kind convert_stmt_kind(Stmt::Kind kind) { return static_cast<IRStmt::Kind>(kind); }

class LoweringContext {
public:
    IRExprPtr expression(const Expr* expr) {
        if (!expr) return nullptr;
        auto out = std::make_unique<IRExpr>();
        out->kind = convert_expr_kind(expr->kind); out->text = expr->text; out->span = expr->span;
        out->type_name = literal_type(*expr);
        if (expr->kind == Expr::Kind::identifier) {
            auto it = value_types_.find(expr->text); if (it != value_types_.end()) out->type_name = it->second;
        }
        out->left = expression(expr->left.get()); out->right = expression(expr->right.get());
        return out;
    }
    IRStmtPtr statement(const Stmt& st) {
        auto out = std::make_unique<IRStmt>(); out->kind=convert_stmt_kind(st.kind); out->span=st.span; out->name=st.name; out->op=st.op; out->is_const=st.is_const;
        out->owner=st.owner; out->generic_parameters=st.generic_parameters; out->parameters=st.parameters; out->has_body=st.has_body;
        out->type_name = st.declared_type ? st.declared_type->name : "";
        if (st.return_type) out->return_type = st.return_type->name;
        out->value=expression(st.value.get()); out->condition=expression(st.condition.get()); out->increment=expression(st.increment.get());
        if (st.initializer) out->initializer=statement(*st.initializer);
        if (st.kind == Stmt::Kind::declaration) {
            if (out->type_name.empty() && out->value) out->type_name = out->value->type_name;
            value_types_[st.name] = out->type_name.empty() ? "opaque" : out->type_name;
        }
        for (const auto& child : st.body) out->body.push_back(statement(*child));
        for (const auto& child : st.else_body) out->else_body.push_back(statement(*child));
        return out;
    }
private:
    std::unordered_map<std::string,std::string> value_types_;
};
}
IRResult IRLowerer::lower(const Program& program) {
    IRResult result; LoweringContext ctx;
    for (const auto& statement : program.statements) result.program.statements.push_back(ctx.statement(*statement));
    return result;
}
} // namespace strut
