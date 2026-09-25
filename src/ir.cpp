#include "strut/ir.h"

#include <unordered_map>
#include <algorithm>
#include <cctype>

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
        case Expr::Kind::array_literal: {
            if (expr.arguments.empty()) return "opaque[]";
            return literal_type(*expr.arguments.front()) + "[]";
        }
        case Expr::Kind::map_literal: {
            if (expr.arguments.size() < 2) return "map<opaque,opaque>";
            return "map<" + literal_type(*expr.arguments[0]) + "," + literal_type(*expr.arguments[1]) + ">";
        }
        case Expr::Kind::json_object: return "json";
        case Expr::Kind::struct_literal: return expr.text;
        case Expr::Kind::lambda: return "function";
        default: return "opaque";
    }
}
IRExpr::Kind convert_expr_kind(Expr::Kind kind) { return static_cast<IRExpr::Kind>(kind); }
IRStmt::Kind convert_stmt_kind(Stmt::Kind kind) { return static_cast<IRStmt::Kind>(kind); }

class LoweringContext {
public:
    void register_function(const Stmt& st) {
        std::string sig="function<(";
        for(std::size_t i=0;i<st.parameters.size();++i){if(i)sig+=",";sig+=st.parameters[i].type.name;}
        sig+=")->"+(st.return_type?st.return_type->name:std::string("void"))+">";
        function_types_[st.name]=sig;
    }
    IRExprPtr expression(const Expr* expr) {
        if (!expr) return nullptr;
        auto out = std::make_unique<IRExpr>();
        out->kind = convert_expr_kind(expr->kind); out->text = expr->text; out->span = expr->span;
        out->type_name = literal_type(*expr);
        if (expr->kind == Expr::Kind::identifier) {
            auto it = value_types_.find(expr->text); if (it != value_types_.end()) out->type_name = it->second; else { auto fn=function_types_.find(expr->text); if(fn!=function_types_.end()) out->type_name=fn->second; }
        }
        out->left = expression(expr->left.get()); out->right = expression(expr->right.get());
        for (const auto& arg : expr->arguments) out->arguments.push_back(expression(arg.get()));
        out->names = expr->names;
        if(expr->kind==Expr::Kind::lambda && expr->lambda){out->lambda_async=expr->lambda->is_async;out->lambda_parameters=expr->lambda->parameters;out->lambda_expression=expression(expr->lambda->expression_body.get());for(const auto& child:expr->lambda->body)out->lambda_body.push_back(statement(*child));}
        if (expr->kind == Expr::Kind::index && out->left && out->left->type_name == "json") out->type_name = "json";
        if (expr->kind == Expr::Kind::call && out->left && out->left->kind == IRExpr::Kind::member && out->left->left && out->left->left->kind == IRExpr::Kind::identifier && out->left->left->text == "json") {
            if (out->left->text == "parse" || out->left->text == "encode") out->type_name = "json";
            if (out->left->text == "stringify" || out->left->text == "pretty") out->type_name = "string";
        }
        return out;
    }
    IRStmtPtr statement(const Stmt& st) {
        auto out = std::make_unique<IRStmt>(); out->kind=convert_stmt_kind(st.kind); out->span=st.span; out->name=st.name; out->op=st.op; out->is_const=st.is_const;
        out->owner=st.owner; out->generic_parameters=st.generic_parameters; out->parameters=st.parameters; out->fields=st.fields; out->has_body=st.has_body;
        out->type_name = st.declared_type ? st.declared_type->name : "";
        if (st.return_type) out->return_type = st.return_type->name;
        out->value=expression(st.value.get()); out->target=expression(st.target.get()); out->condition=expression(st.condition.get()); out->increment=expression(st.increment.get());
        if (st.initializer) out->initializer=statement(*st.initializer);
        if (st.kind == Stmt::Kind::declaration) {
            if (out->type_name.empty() && out->value) out->type_name = out->value->type_name;
            if(out->value && out->value->kind==IRExpr::Kind::lambda && out->type_name.rfind("function<(",0)==0){
                auto arrow=out->type_name.rfind(")->"); if(arrow!=std::string::npos){auto args=out->type_name.substr(10,arrow-10);std::vector<std::string> types;int depth=0;std::size_t start=0;for(std::size_t i=0;i<=args.size();++i){char c=i<args.size()?args[i]:',';if(c=='<'||c=='['||c=='(')++depth;else if(c=='>'||c==']'||c==')')--depth;else if(c==','&&depth==0){types.push_back(args.substr(start,i-start));start=i+1;}}for(std::size_t i=0;i<out->value->lambda_parameters.size()&&i<types.size();++i)if(out->value->lambda_parameters[i].type.name.empty() || std::all_of(out->value->lambda_parameters[i].type.name.begin(),out->value->lambda_parameters[i].type.name.end(),[](unsigned char c){return !std::islower(c);}))out->value->lambda_parameters[i].type.name=types[i];}
            }
            value_types_[st.name] = out->type_name.empty() ? "opaque" : out->type_name;
        }
        for (const auto& child : st.body) out->body.push_back(statement(*child));
        for (const auto& child : st.else_body) out->else_body.push_back(statement(*child));
        return out;
    }
private:
    std::unordered_map<std::string,std::string> value_types_;
    std::unordered_map<std::string,std::string> function_types_;
};
}
IRResult IRLowerer::lower(const Program& program) {
    IRResult result; LoweringContext ctx;
    for (const auto& statement : program.statements) if(statement->kind==Stmt::Kind::function_decl) ctx.register_function(*statement);
    for (const auto& statement : program.statements) result.program.statements.push_back(ctx.statement(*statement));
    return result;
}
} // namespace strut
