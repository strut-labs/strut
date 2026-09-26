#include "strut/ir.h"

#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>

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
        case Expr::Kind::tuple_literal: {
            std::string out="tuple<";for(std::size_t i=0;i<expr.arguments.size();++i){if(i)out+=",";out+=literal_type(*expr.arguments[i]);}return out+">";
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
std::string strip_ref_type(std::string t){if(t.rfind("ref<",0)==0&&t.back()=='>')t=t.substr(4,t.size()-5);if(t.rfind("const ",0)==0)t=t.substr(6);return t;}
std::string safe_name(std::string s){for(char& c:s)if(!std::isalnum(static_cast<unsigned char>(c)))c='_';return s;}
bool parse_i64(const std::string& text, long long& value){auto r=std::from_chars(text.data(),text.data()+text.size(),value);return r.ec==std::errc{}&&r.ptr==text.data()+text.size();}
void fold_binary(IRExpr& out){
    if(out.kind!=IRExpr::Kind::binary||!out.left||!out.right)return;
    if(out.left->kind!=IRExpr::Kind::integer_literal||out.right->kind!=IRExpr::Kind::integer_literal)return;
    long long a=0,b=0;if(!parse_i64(out.left->text,a)||!parse_i64(out.right->text,b))return;
    long long v=0; bool is_bool=false; bool bv=false;
    if(out.text=="+")v=a+b; else if(out.text=="-")v=a-b; else if(out.text=="*")v=a*b;
    else if(out.text=="/"&&b!=0)v=a/b; else if(out.text=="%"&&b!=0)v=a%b;
    else if(out.text=="=="){is_bool=true;bv=a==b;} else if(out.text=="!="){is_bool=true;bv=a!=b;}
    else if(out.text=="<"){is_bool=true;bv=a<b;} else if(out.text=="<="){is_bool=true;bv=a<=b;}
    else if(out.text==">"){is_bool=true;bv=a>b;} else if(out.text==">="){is_bool=true;bv=a>=b;} else return;
    out.left.reset();out.right.reset();out.arguments.clear();
    if(is_bool){out.kind=IRExpr::Kind::boolean_literal;out.text=bv?"true":"false";out.type_name="bool";}
    else{out.kind=IRExpr::Kind::integer_literal;out.text=std::to_string(v);out.type_name=infer_integer_literal(out.text).name;}
}

class LoweringContext {
public:
    void register_function(const Stmt& st) {
        std::string sig="function<(";
        for(std::size_t i=0;i<st.parameters.size();++i){if(i)sig+=",";sig+=st.parameters[i].type.name;}
        sig+=")->"+(st.is_async?("future<"+(st.return_type?st.return_type->name:std::string("void"))+">"):(st.return_type?st.return_type->name:std::string("void")))+">";
        function_types_[st.name]=sig;
    }
    void register_operator(const Stmt& st){if(st.parameters.size()!=2)return;std::string a=strip_ref_type(st.parameters[0].type.name),b=strip_ref_type(st.parameters[1].type.name);std::string helper="strut_op_"+(st.op==":="?std::string("init"):std::string("assign"))+"_"+safe_name(a)+"_"+safe_name(b);if(st.op==":=")init_overloads_[a+"|"+b]=helper;else if(st.op=="=")assign_overloads_[a+"|"+b]=helper;}
    IRExprPtr expression(const Expr* expr) {
        if (!expr) return nullptr;
        auto out = std::make_unique<IRExpr>();
        out->kind = convert_expr_kind(expr->kind); out->text = expr->text; out->span = expr->span;
        out->type_name = literal_type(*expr);
        if (expr->kind == Expr::Kind::identifier) {
            auto it = value_types_.find(expr->text); if (it != value_types_.end()) out->type_name = it->second; else { auto fn=function_types_.find(expr->text); if(fn!=function_types_.end()) out->type_name=fn->second; }
        }
        out->left = expression(expr->left.get()); out->right = expression(expr->right.get());
        fold_binary(*out);
        for (const auto& arg : expr->arguments) out->arguments.push_back(expression(arg.get()));
        out->names = expr->names;
        if(expr->kind==Expr::Kind::lambda && expr->lambda){out->lambda_async=expr->lambda->is_async;out->lambda_parameters=expr->lambda->parameters;out->lambda_expression=expression(expr->lambda->expression_body.get());for(const auto& child:expr->lambda->body)out->lambda_body.push_back(statement(*child));}
        if (expr->kind == Expr::Kind::index && out->left && out->left->type_name == "json") out->type_name = "json";
        if (expr->kind == Expr::Kind::index && out->left && out->left->type_name.rfind("tuple<",0)==0 && out->right && out->right->kind==IRExpr::Kind::integer_literal){auto inner=out->left->type_name.substr(6,out->left->type_name.size()-7);std::vector<std::string> parts;int depth=0;std::size_t start=0;for(std::size_t i=0;i<=inner.size();++i){char c=i<inner.size()?inner[i]:',';if(c=='<')++depth;else if(c=='>')--depth;else if(c==','&&depth==0){parts.push_back(inner.substr(start,i-start));start=i+1;}}try{auto idx=static_cast<std::size_t>(std::stoull(out->right->text));if(idx<parts.size())out->type_name=parts[idx];}catch(...){}}
        if (expr->kind == Expr::Kind::call && out->left && out->left->kind == IRExpr::Kind::member && out->left->left && out->left->left->kind == IRExpr::Kind::identifier && out->left->left->text == "json") {
            if (out->left->text == "parse" || out->left->text == "encode") out->type_name = "json";
            if (out->left->text == "stringify" || out->left->text == "pretty") out->type_name = "string";
        }
        return out;
    }
    IRStmtPtr statement(const Stmt& st) {
        auto out = std::make_unique<IRStmt>(); out->kind=convert_stmt_kind(st.kind); out->span=st.span; out->name=st.name; out->op=st.op; out->is_const=st.is_const;
        out->owner=st.owner; out->is_async=st.is_async; out->is_extern_c=st.is_extern_c; out->generic_parameters=st.generic_parameters; out->bases=st.bases; out->enum_names=st.enum_names; out->enum_values=st.enum_values; out->error_types=st.error_types; out->parameters=st.parameters; out->fields=st.fields; out->has_body=st.has_body;
        out->type_name = st.declared_type ? st.declared_type->name : ""; out->explicit_type=st.declared_type.has_value();
        if (st.return_type) out->return_type = st.return_type->name;
        out->value=expression(st.value.get()); out->target=expression(st.target.get()); out->condition=expression(st.condition.get()); out->increment=expression(st.increment.get());
        if (st.initializer) out->initializer=statement(*st.initializer);
        if (st.kind == Stmt::Kind::declaration) {
            if (out->type_name.empty() && out->value) out->type_name = out->value->type_name;
            if(out->value && out->value->kind==IRExpr::Kind::lambda && out->type_name.rfind("function<(",0)==0){
                auto arrow=out->type_name.rfind(")->"); if(arrow!=std::string::npos){auto args=out->type_name.substr(10,arrow-10);std::vector<std::string> types;int depth=0;std::size_t start=0;for(std::size_t i=0;i<=args.size();++i){char c=i<args.size()?args[i]:',';if(c=='<'||c=='['||c=='(')++depth;else if(c=='>'||c==']'||c==')')--depth;else if(c==','&&depth==0){types.push_back(args.substr(start,i-start));start=i+1;}}for(std::size_t i=0;i<out->value->lambda_parameters.size()&&i<types.size();++i)if(out->value->lambda_parameters[i].type.name.empty() || std::all_of(out->value->lambda_parameters[i].type.name.begin(),out->value->lambda_parameters[i].type.name.end(),[](unsigned char c){return !std::islower(c);}))out->value->lambda_parameters[i].type.name=types[i];}
            }
            if(st.declared_type && out->value){auto it=init_overloads_.find(strip_ref_type(st.declared_type->name)+"|"+strip_ref_type(out->value->type_name));if(it!=init_overloads_.end())out->overload_name=it->second;}
            value_types_[st.name] = out->type_name.empty() ? "opaque" : out->type_name;
        } else if(st.kind==Stmt::Kind::assignment && out->value){std::string lhs;if(!st.name.empty()){auto it=value_types_.find(st.name);if(it!=value_types_.end())lhs=strip_ref_type(it->second);}else if(out->target)lhs=strip_ref_type(out->target->type_name);auto it=assign_overloads_.find(lhs+"|"+strip_ref_type(out->value->type_name));if(it!=assign_overloads_.end())out->overload_name=it->second;}
        for (const auto& child : st.body) out->body.push_back(statement(*child));
        for (const auto& child : st.else_body) out->else_body.push_back(statement(*child));
        for (const auto& c : st.switch_cases) { IRSwitchCase ic; ic.is_default=c.is_default; ic.span=c.span; ic.value=expression(c.value.get()); for(const auto& child:c.body) ic.body.push_back(statement(*child)); out->switch_cases.push_back(std::move(ic)); }
        for (const auto& c : st.catches) { IRCatchClause ic; ic.catch_all=!c.type.has_value(); ic.type_name=c.type?c.type->name:""; ic.name=c.name; ic.span=c.span; for(const auto& child:c.body) ic.body.push_back(statement(*child)); out->catches.push_back(std::move(ic)); }
        return out;
    }
private:
    std::unordered_map<std::string,std::string> value_types_;
    std::unordered_map<std::string,std::string> function_types_;
    std::unordered_map<std::string,std::string> init_overloads_;
    std::unordered_map<std::string,std::string> assign_overloads_;
};
}
IRResult IRLowerer::lower(const Program& program) {
    IRResult result; LoweringContext ctx;
    result.program.standard_modules = program.standard_modules;
    for (const auto& statement : program.statements) {if(statement->kind==Stmt::Kind::function_decl) ctx.register_function(*statement);if(statement->kind==Stmt::Kind::operator_decl)ctx.register_operator(*statement);}
    for (const auto& statement : program.statements) result.program.statements.push_back(ctx.statement(*statement));
    return result;
}
} // namespace strut
