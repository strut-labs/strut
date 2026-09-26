#include "strut/sema.h"

#include <algorithm>
#include <cctype>

namespace strut {
namespace {
std::string function_signature(const Stmt& st){std::string sig="function<(";for(std::size_t i=0;i<st.parameters.size();++i){if(i)sig+=",";sig+=st.parameters[i].type.name;}sig+=")->"+(st.is_async?("future<"+(st.return_type?st.return_type->name:std::string("void"))+">"):(st.return_type?st.return_type->name:std::string("void")))+">";return sig;}
std::string function_return(std::string_view sig){auto p=sig.rfind(")->");if(p==std::string_view::npos||sig.empty()||sig.back()!='>')return "opaque";return std::string(sig.substr(p+3,sig.size()-(p+4)));}
std::string generic_inner(std::string_view type,std::string_view head){if(type.rfind(head,0)!=0||type.size()<=head.size()+1||type.back()!='>')return {};return std::string(type.substr(head.size(),type.size()-head.size()-1));}
std::string normalize_operator_type(std::string t){if(t.rfind("ref<",0)==0&&t.back()=='>')t=t.substr(4,t.size()-5);if(t.rfind("const ",0)==0)t=t.substr(6);return t;}
}

std::unordered_map<std::string, Symbol>& SemanticAnalyzer::namespace_map(Scope& scope, SymbolNamespace ns) {
    if (ns == SymbolNamespace::function) return scope.functions;
    if (ns == SymbolNamespace::type) return scope.types;
    return scope.values;
}
void SemanticAnalyzer::push_scope() { scopes_.push_back(Scope{}); }
void SemanticAnalyzer::pop_scope() { scopes_.pop_back(); }
bool SemanticAnalyzer::declare(SemanticResult& result, Symbol symbol) {
    auto& map = namespace_map(scopes_.back(), symbol.name_space);
    if (map.find(symbol.name) != map.end()) {
        result.diagnostics.push_back(Diagnostic{symbol.span, "duplicate definition of '" + symbol.name + "' in the same scope"});
        return false;
    }
    map.emplace(symbol.name, std::move(symbol));
    return true;
}
Symbol* SemanticAnalyzer::lookup(std::string_view name, SymbolNamespace ns) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto& map = namespace_map(*it, ns);
        auto found = map.find(std::string(name));
        if (found != map.end()) return &found->second;
    }
    return nullptr;
}

std::string SemanticAnalyzer::resolved_type_name(std::string_view name) const {
    const bool nullable = is_nullable_type(name);
    std::string current = strip_nullable(name);
    std::unordered_set<std::string> seen;
    while (aliases_.find(current) != aliases_.end() && seen.find(current) == seen.end()) {
        seen.insert(current);
        current = aliases_.at(current);
    }
    return nullable ? current + "?" : current;
}
TypeInfo SemanticAnalyzer::resolve_type(std::string_view name) const {
    const std::string resolved = resolved_type_name(name);
    if (is_nullable_type(resolved)) return {TypeKind::named, 0, resolved};
    auto type = builtin_type(resolved);
    if (type.valid()) return type;
    if (resolved == "null") return {TypeKind::null_type, 0, "null"};
    if (resolved == "opaque") return {TypeKind::named, 0, "opaque"};
    if (named_types_.find(resolved) != named_types_.end()) return {TypeKind::named,0,resolved};
    if (resolved.find('<') != std::string::npos || resolved.find('[') != std::string::npos ||
        (!resolved.empty() && std::all_of(resolved.begin(), resolved.end(), [](unsigned char c){ return !std::islower(c); }))) {
        return {TypeKind::named, 0, std::string(name)};
    }
    return {};
}
bool SemanticAnalyzer::compatible(const TypeInfo& from, const TypeInfo& to) const {
    if (!from.valid() || !to.valid()) return true; // later phases refine currently opaque compound/user types
    if (from.kind == TypeKind::named && from.name == "opaque") return true;
    if (from.kind == TypeKind::null_type) return to.kind == TypeKind::null_type || is_nullable_type(to.name) || to.name.rfind("ptr<",0)==0 || to.name.rfind("weak_ptr<",0)==0 || to.name.rfind("raw_ptr<",0)==0;
    if (is_nullable_type(to.name) && !is_nullable_type(from.name)) return compatible(from, resolve_type(strip_nullable(to.name)));
    if (is_nullable_type(from.name) && is_nullable_type(to.name)) return strip_nullable(from.name) == strip_nullable(to.name);
    if (from.kind == to.kind && from.bits == to.bits) return true;
    if (from.kind == TypeKind::string_type && to.kind == TypeKind::string_type) return true;
    if (from.kind == TypeKind::bool_type && to.kind == TypeKind::bool_type) return true;
    return can_implicitly_convert(from, to);
}

void SemanticAnalyzer::require_module(SemanticResult& result, std::string_view module, SourceSpan span, std::string_view facility) const {
    if (!enforce_standard_modules_ || standard_modules_.find(std::string(module)) != standard_modules_.end()) return;
    result.diagnostics.push_back(Diagnostic{span, "'" + std::string(facility) + "' requires include <" + std::string(module) + ">;"});
}

void SemanticAnalyzer::require_type_module(SemanticResult& result, std::string_view type_name, SourceSpan span) const {
    const std::string t(type_name);
    auto first_arg=[](const std::string& value,std::string_view head){if(value.rfind(std::string(head),0)!=0||value.back()!='>')return std::string();auto inner=value.substr(head.size(),value.size()-head.size()-1);int depth=0;for(std::size_t i=0;i<inner.size();++i){if(inner[i]=='<')++depth;else if(inner[i]=='>')--depth;else if(inner[i]==','&&depth==0)return inner.substr(0,i);}return inner;};
    auto key_supported=[&](std::string key){if(key.rfind("const ",0)==0)key=key.substr(6);auto b=builtin_type(key);if(b.valid()&&b.kind!=TypeKind::void_type&&b.kind!=TypeKind::json_type)return true;if(enum_members_.find(key)!=enum_members_.end())return true;if(key.rfind("ptr<",0)==0||key.rfind("raw_ptr<",0)==0)return true;return false;};
    if (t.find("[]") != std::string::npos || t.find("vector<") != std::string::npos) require_module(result, "vector", span, type_name);
    if (t.find("ordered_map<") != std::string::npos) require_module(result, "ordered_map", span, type_name);
    else if (t.find("map<") != std::string::npos) require_module(result, "map", span, type_name);
    if (t.find("ordered_set<") != std::string::npos) require_module(result, "ordered_set", span, type_name);
    else if (t.find("set<") != std::string::npos) require_module(result, "set", span, type_name);
    if (t.rfind("queue<",0) == 0 || t.find("<queue<") != std::string::npos || t.find(",queue<") != std::string::npos) require_module(result, "queue", span, type_name);
    if (t.find("stack<") != std::string::npos) require_module(result, "stack", span, type_name);
    if (t.find("deque<") != std::string::npos) require_module(result, "deque", span, type_name);
    if (t.find("list<") != std::string::npos) require_module(result, "list", span, type_name);
    if (t.find("prique<") != std::string::npos) result.diagnostics.push_back(Diagnostic{span, "'prique' was renamed to 'priority_queue'; use priority_queue<T> or priority_queue<T,min>"});
    if (t.find("priority_queue<") != std::string::npos) require_module(result, "priority_queue", span, type_name);
    if (t.find("tuple<") != std::string::npos) require_module(result, "tuple", span, type_name);
    if(t.rfind("map<",0)==0){auto key=first_arg(t,"map<");if(!key_supported(key))result.diagnostics.push_back(Diagnostic{span,"map key type '"+key+"' is not hashable; use a built-in/hashable key or ordered_map"});}
    if(t.rfind("set<",0)==0){auto key=first_arg(t,"set<");if(!key_supported(key))result.diagnostics.push_back(Diagnostic{span,"set element type '"+key+"' is not hashable; use a built-in/hashable element or ordered_set"});}
    if(t.rfind("ordered_map<",0)==0){auto key=first_arg(t,"ordered_map<");if(!key_supported(key))result.diagnostics.push_back(Diagnostic{span,"ordered_map key type '"+key+"' is not orderable by the standard library"});}
    if(t.rfind("ordered_set<",0)==0){auto key=first_arg(t,"ordered_set<");if(!key_supported(key))result.diagnostics.push_back(Diagnostic{span,"ordered_set element type '"+key+"' is not orderable by the standard library"});}
}

TypeInfo SemanticAnalyzer::infer_expression(SemanticResult& result, const Expr& expr) {
    switch (expr.kind) {
        case Expr::Kind::integer_literal: return infer_integer_literal(expr.text);
        case Expr::Kind::floating_literal: return infer_floating_literal(expr.text);
        case Expr::Kind::string_literal: return {TypeKind::string_type, 0, "string"};
        case Expr::Kind::boolean_literal: return {TypeKind::bool_type, 0, "bool"};
        case Expr::Kind::null_literal: return {TypeKind::null_type, 0, "null"};
        case Expr::Kind::array_literal: {
            if (expr.arguments.empty()) return {TypeKind::named,0,"opaque[]"};
            auto first=infer_expression(result,*expr.arguments.front());
            for(std::size_t i=1;i<expr.arguments.size();++i){auto next=infer_expression(result,*expr.arguments[i]);if(first.valid()&&next.valid()&&!compatible(next,first))result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"array literal element type mismatch"});}
            return {TypeKind::named,0,(first.name.empty()?std::string("opaque"):first.name)+"[]"};
        }
        case Expr::Kind::tuple_literal: {
            require_module(result, "tuple", expr.span, "tuple literal");
            std::string name="tuple<";
            for(std::size_t i=0;i<expr.arguments.size();++i){if(i)name+=",";auto t=infer_expression(result,*expr.arguments[i]);name+=t.name.empty()?"opaque":t.name;}
            name+=">"; return {TypeKind::named,0,name};
        }
        case Expr::Kind::map_literal: {
            if(expr.arguments.size()<2)return {TypeKind::named,0,"map<opaque,opaque>"};
            auto key=infer_expression(result,*expr.arguments[0]);auto value=infer_expression(result,*expr.arguments[1]);
            for(std::size_t i=2;i+1<expr.arguments.size();i+=2){auto k=infer_expression(result,*expr.arguments[i]);auto v=infer_expression(result,*expr.arguments[i+1]);if(key.valid()&&k.valid()&&!compatible(k,key))result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"map key type mismatch"});if(value.valid()&&v.valid()&&!compatible(v,value))result.diagnostics.push_back(Diagnostic{expr.arguments[i+1]->span,"map value type mismatch"});}
            return {TypeKind::named,0,"map<"+(key.name.empty()?std::string("opaque"):key.name)+","+(value.name.empty()?std::string("opaque"):value.name)+">"};
        }
        case Expr::Kind::json_object: {
            for(std::size_t i=1;i<expr.arguments.size();i+=2) infer_expression(result,*expr.arguments[i]);
            return builtin_type("json");
        }
        case Expr::Kind::struct_literal: {
            auto* type_symbol=lookup(expr.text,SymbolNamespace::type);if(!type_symbol){result.diagnostics.push_back(Diagnostic{expr.span,"unknown struct type '"+expr.text+"'"});return {};}
            auto ait=abstract_methods_.find(expr.text); if(ait!=abstract_methods_.end()&&!ait->second.empty()){std::string names;for(const auto& n:ait->second){if(!names.empty())names+=", ";names+=n;}result.diagnostics.push_back(Diagnostic{expr.span,"cannot instantiate abstract struct "+expr.text+"; unsatisfied methods: "+names});}
            auto fit=struct_fields_.find(expr.text);if(fit!=struct_fields_.end()){std::unordered_set<std::string> seen;for(std::size_t i=0;i<expr.names.size();++i){auto field=fit->second.find(expr.names[i]);if(field==fit->second.end()){result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"unknown field '"+expr.names[i]+"' for struct "+expr.text});continue;}if(!seen.insert(expr.names[i]).second)result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"duplicate struct field '"+expr.names[i]+"'"});auto value=infer_expression(result,*expr.arguments[i]);auto dest=resolve_type(field->second);if(value.valid()&&dest.valid()&&!compatible(value,dest))result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"incompatible value for field '"+expr.names[i]+"'"});}for(const auto& field:fit->second)if(seen.find(field.first)==seen.end())result.diagnostics.push_back(Diagnostic{expr.span,"missing field '"+field.first+"' for struct "+expr.text});}
            return {TypeKind::named,0,expr.text};
        }
        case Expr::Kind::identifier: {
            if(expr.text=="in") return {TypeKind::named,0,"istream"};
            if(expr.text=="out"||expr.text=="err") return {TypeKind::named,0,"ostream"};
            if(expr.text=="endl") return {TypeKind::named,0,"opaque"};
            auto* symbol = lookup(expr.text, SymbolNamespace::value);
            if (symbol) return resolve_type(symbol->type_name);
            if (auto* fn=lookup(expr.text,SymbolNamespace::function)) return {TypeKind::named,0,fn->type_name};
            result.diagnostics.push_back(Diagnostic{expr.span, "unknown value '" + expr.text + "'"});
            return {};
        }
        case Expr::Kind::grouping: return expr.left ? infer_expression(result, *expr.left) : TypeInfo{};
        case Expr::Kind::unary:
        case Expr::Kind::postfix: {
            auto operand = expr.right ? infer_expression(result, *expr.right) : (expr.left ? infer_expression(result, *expr.left) : TypeInfo{});
            if (expr.text == "!") return {TypeKind::bool_type, 0, "bool"};
            if(expr.text=="await"){auto inner=generic_inner(operand.name,"future<");if(inner.empty())result.diagnostics.push_back(Diagnostic{expr.span,"await requires a future<T>"});return resolve_type(inner.empty()?"opaque":inner);}
            if(expr.text=="*"){if(operand.name.rfind("raw_ptr<",0)==0 && unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"ptr<T> dereference requires unsafe block"});for(auto head:{std::string_view("ptr<"),std::string_view("raw_ptr<"),std::string_view("ref<")}){auto inner=generic_inner(operand.name,head);if(!inner.empty())return resolve_type(inner);}}
            auto oit=operator_returns_.find("prefix:"+expr.text+"|"+operand.name);if(oit!=operator_returns_.end())return resolve_type(oit->second);
            return operand;
        }
        case Expr::Kind::binary: {
            auto left = infer_expression(result, *expr.left);
            auto right = infer_expression(result, *expr.right);
            if (expr.text == "??") {
                if (!is_nullable_type(left.name)) result.diagnostics.push_back(Diagnostic{expr.left->span, "left operand of ?? must be nullable"});
                auto inner = resolve_type(strip_nullable(left.name));
                if (right.valid() && inner.valid() && !compatible(right, inner)) result.diagnostics.push_back(Diagnostic{expr.right->span, "fallback value is incompatible with nullable type"});
                return inner;
            }
            if (expr.text == "==" || expr.text == "!=" || expr.text == "<" || expr.text == "<=" || expr.text == ">" || expr.text == ">=" || expr.text == "&&" || expr.text == "||") {
                return {TypeKind::bool_type, 0, "bool"};
            }
            if (expr.text == "<<" || expr.text == ">>") return left;
            if((expr.text=="+"||expr.text=="-") && left.name.rfind("raw_ptr<",0)==0){if(unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"raw pointer arithmetic requires unsafe block"});return left;}
            if (left.numeric() && right.numeric()) {
                if (left.kind == TypeKind::floating || right.kind == TypeKind::floating) return builtin_type((left.bits > 32 || right.bits > 32) ? "double_64" : "double_32");
                if (left.kind == right.kind) return left.bits >= right.bits ? left : right;
                return left.bits > right.bits ? left : right;
            }
            auto oit=operator_returns_.find("infix:"+expr.text+"|"+left.name+","+right.name);if(oit!=operator_returns_.end())return resolve_type(oit->second);
            return {};
        }
        case Expr::Kind::call: {
            for (const auto& arg : expr.arguments) { if(arg && arg->kind==Expr::Kind::array_literal) require_module(result,"vector",arg->span,"array/vector argument"); infer_expression(result, *arg); }
            if(expr.left && expr.left->kind==Expr::Kind::identifier){if(extern_c_functions_.find(expr.left->text)!=extern_c_functions_.end() && unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"extern C call requires unsafe block"});auto fit=function_errors_.find(expr.left->text);if(fit!=function_errors_.end())for(const auto& e:fit->second)if(current_function_errors_.find(e)==current_function_errors_.end() && catch_all_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"call to '"+expr.left->text+"' may throw checked error "+e+" not declared by current function"});}
            if (expr.left && expr.left->kind == Expr::Kind::member && expr.left->left && expr.left->left->kind == Expr::Kind::identifier && expr.left->left->text == "json") {
                if (expr.left->text == "parse" || expr.left->text == "encode") return builtin_type("json");
                if (expr.left->text == "stringify" || expr.left->text == "pretty") return builtin_type("string");
            }
            if(expr.left && expr.left->kind==Expr::Kind::member && expr.left->left){auto base=infer_expression(result,*expr.left->left);const auto& m=expr.left->text;if((base.name=="ifstream"||base.name=="sstream")&&(m=="read_all"||m=="str"))return builtin_type("string");if((base.name=="ifstream"||base.name=="ofstream"||base.name=="sstream")&&m=="is_open")return builtin_type("bool");if((base.name=="ifstream"||base.name=="ofstream"||base.name=="sstream")&&(m=="open"||m=="close"||m=="write"))return {TypeKind::void_type,0,"void"};
                if(base.name=="process_out"&&(m=="read"||m=="read_line"||m=="read_all"))return builtin_type("string");
                if(base.name=="process_out"&&m=="eof")return builtin_type("bool");
                if(base.name=="process_in"&&(m=="write"||m=="write_line"||m=="close"))return {TypeKind::void_type,0,"void"};
                if(base.name=="process"&&(m=="wait"||m=="exit_code"))return builtin_type("int");
                if(base.name=="process"&&m=="running")return builtin_type("bool");
                if(base.name=="process"&&(m=="terminate"||m=="close_input"))return {TypeKind::void_type,0,"void"};
                if(base.name=="thread"&&m=="join")return {TypeKind::void_type,0,"void"};
                if(base.name=="thread"&&m=="joinable")return builtin_type("bool");
                if(base.name=="tcp_socket"){if(m=="read")return builtin_type("string");if(m=="is_open")return builtin_type("bool");if(m=="write"||m=="close")return {TypeKind::void_type,0,"void"};}
                if(base.name=="tcp_listener"){if(m=="accept")return {TypeKind::named,0,"tcp_socket"};if(m=="accept_async")return {TypeKind::named,0,"future<tcp_socket>"};if(m=="is_open")return builtin_type("bool");if(m=="close")return {TypeKind::void_type,0,"void"};}
                if(base.name=="tls_stream"){if(m=="read")return builtin_type("string");if(m=="is_open")return builtin_type("bool");if(m=="write"||m=="close")return {TypeKind::void_type,0,"void"};}
                if(base.name=="http_response"&&m=="json")return builtin_type("json");
                if(base.name=="http_request"&&m=="json")return builtin_type("json");
                if(base.name=="http_server"&&(m=="get"||m=="post"||m=="get_async"||m=="post_async"||m=="listen"||m=="static"))return {TypeKind::void_type,0,"void"};
                if(base.name=="sqlite_db"){if(m=="query")return builtin_type("json");if(m=="exec"||m=="close"||m=="transaction")return {TypeKind::void_type,0,"void"};}
                if(base.name=="mutex"&&(m=="lock"||m=="unlock"))return {TypeKind::void_type,0,"void"};
                if(base.name.rfind("channel<",0)==0){auto elem=generic_inner(base.name,"channel<");if(m=="send"||m=="close")return {TypeKind::void_type,0,"void"};if(m=="receive")return {TypeKind::named,0,elem+"?"};if(m=="closed")return builtin_type("bool");}
                auto container_elem=[&](std::string_view head){return generic_inner(base.name,head);};
                if(base.name.rfind("set<",0)==0||base.name.rfind("ordered_set<",0)==0){auto elem=base.name.rfind("ordered_set<",0)==0?container_elem("ordered_set<"):container_elem("set<");if(m=="add"||m=="remove")return {TypeKind::void_type,0,"void"};if(m=="contains")return builtin_type("bool");if(m=="length")return builtin_type("int");}
                if(base.name.rfind("queue<",0)==0){auto elem=container_elem("queue<");if(m=="push"||m=="pop")return {TypeKind::void_type,0,"void"};if(m=="front"||m=="back")return resolve_type(elem);if(m=="length")return builtin_type("int");if(m=="empty")return builtin_type("bool");}
                if(base.name.rfind("stack<",0)==0){auto elem=container_elem("stack<");if(m=="push"||m=="pop")return {TypeKind::void_type,0,"void"};if(m=="top")return resolve_type(elem);if(m=="length")return builtin_type("int");if(m=="empty")return builtin_type("bool");}
                if(base.name.rfind("priority_queue<",0)==0){auto elem=container_elem("priority_queue<");auto comma=elem.find(',');if(comma!=std::string::npos)elem=elem.substr(0,comma);if(m=="push"||m=="pop")return {TypeKind::void_type,0,"void"};if(m=="top")return resolve_type(elem);if(m=="length")return builtin_type("int");if(m=="empty")return builtin_type("bool");}
                if(base.name.rfind("deque<",0)==0||base.name.rfind("list<",0)==0){auto elem=base.name.rfind("deque<",0)==0?container_elem("deque<"):container_elem("list<");if(m=="push"||m=="pop"||m=="push_front"||m=="pop_front")return {TypeKind::void_type,0,"void"};if(m=="front"||m=="back")return resolve_type(elem);if(m=="length")return builtin_type("int");if(m=="empty")return builtin_type("bool");}
                if(base.name.rfind("map<",0)==0||base.name.rfind("ordered_map<",0)==0){if(m=="contains")return builtin_type("bool");if(m=="length")return builtin_type("int");if(m=="remove"||m=="insert")return {TypeKind::void_type,0,"void"};}
                if(m=="count_by"||m=="index_by") require_module(result,"map",expr.span,m);
                std::string elem="opaque";if(base.name.size()>2&&base.name.compare(base.name.size()-2,2,"[]")==0)elem=base.name.substr(0,base.name.size()-2);if(m=="lock" && base.name.rfind("weak_ptr<",0)==0)return {TypeKind::named,0,"ptr<"+generic_inner(base.name,"weak_ptr<")+">"};if(m=="expired" && base.name.rfind("weak_ptr<",0)==0)return builtin_type("bool");if(m=="filter")return base;if(m=="map")return {TypeKind::named,0,"opaque[]"};if(m=="reduce")return resolve_type(elem);if(m=="any"||m=="all")return builtin_type("bool");if(m=="find")return {TypeKind::named,0,elem+"?"};if(m=="count")return builtin_type("int");if(m=="sort"||m=="reserve")return {TypeKind::void_type,0,"void"};}
            if (expr.left && expr.left->kind == Expr::Kind::identifier) {
                const auto& name = expr.left->text;
                if(name=="ptr"){if(expr.arguments.size()!=1){result.diagnostics.push_back(Diagnostic{expr.span,"ptr(...) requires exactly one argument"});return {};}auto t=infer_expression(result,*expr.arguments[0]);return {TypeKind::named,0,"ptr<"+t.name+">"};}
                if(name=="raw"){if(unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"raw(...) requires unsafe block"});if(expr.arguments.size()!=1){result.diagnostics.push_back(Diagnostic{expr.span,"raw(...) requires exactly one T* safe pointer"});return {};}auto t=infer_expression(result,*expr.arguments[0]);auto inner=generic_inner(t.name,"ptr<");if(inner.empty())result.diagnostics.push_back(Diagnostic{expr.arguments[0]->span,"raw(...) currently requires a T* safe pointer"});return {TypeKind::named,0,"raw_ptr<"+inner+">"};}
                if(name=="weak"){if(expr.arguments.size()!=1){result.diagnostics.push_back(Diagnostic{expr.span,"weak(...) requires exactly one T* safe pointer"});return {};}auto t=infer_expression(result,*expr.arguments[0]);auto inner=generic_inner(t.name,"ptr<");if(inner.empty())result.diagnostics.push_back(Diagnostic{expr.arguments[0]->span,"weak(...) requires a T* safe pointer"});return {TypeKind::named,0,"weak_ptr<"+inner+">"};}
                if(name=="ref"){if(expr.arguments.size()!=1){result.diagnostics.push_back(Diagnostic{expr.span,"ref(...) requires exactly one argument"});return {};}const auto& a=*expr.arguments[0];const bool lvalue=a.kind==Expr::Kind::identifier||a.kind==Expr::Kind::member||a.kind==Expr::Kind::index||(a.kind==Expr::Kind::unary&&a.text=="*");if(!lvalue)result.diagnostics.push_back(Diagnostic{a.span,"ref(...) requires an lvalue with a lifetime that outlives the reference"});if(a.kind==Expr::Kind::index&&a.left){auto owner=infer_expression(result,*a.left);if(owner.name.size()>2&&owner.name.compare(owner.name.size()-2,2,"[]")==0)result.diagnostics.push_back(Diagnostic{a.span,"T& cannot borrow a dynamic-array element because later mutation could invalidate the reference"});}auto t=infer_expression(result,a);return {TypeKind::named,0,"ref<"+t.name+">"};}
                if (name == "print") return {TypeKind::void_type, 0, "void"};
                if (name == "input") return expr.arguments.empty()?builtin_type("string"):TypeInfo{TypeKind::void_type,0,"void"};
                if (name == "istream" || name == "ostream" || name == "sstream" || name == "ifstream" || name == "ofstream") return {TypeKind::named,0,name};
                if (name == "exists" || name == "is_file" || name == "is_dir") { require_module(result, "filesystem", expr.span, name); return builtin_type("bool"); }
                if (name == "ls" || name == "walk") { require_module(result, "filesystem", expr.span, name); require_module(result, "vector", expr.span, name + std::string(" result")); return {TypeKind::named,0,"string[]"}; }
                if (name == "file_size" || name == "modified") { require_module(result, "filesystem", expr.span, name); return builtin_type("int_64"); }
                if (name == "cwd" || name == "absolute" || name == "canonical" || name == "parent" || name == "filename" || name == "extension" || name == "stem" || name == "join_path" || name == "read_file") { require_module(result, "filesystem", expr.span, name); return builtin_type("string"); }
                if (name == "read_bytes") { require_module(result, "filesystem", expr.span, name); return {TypeKind::named,0,"bytes"}; }
                if (name == "env") return {TypeKind::named,0,"string?"};
                if (name == "exec" || name == "exec_shell" || name == "pipe_exec") return {TypeKind::named,0,"exec_result"};
                if (name == "process") return {TypeKind::named,0,"process"};
                if (name == "mutex") return {TypeKind::named,0,"mutex"};
                if (name == "tcp_connect") return {TypeKind::named,0,"tcp_socket"};
                if (name == "tcp_connect_async") return {TypeKind::named,0,"future<tcp_socket>"};
                if (name == "tcp_listen") return {TypeKind::named,0,"tcp_listener"};
                if (name == "tls_connect") return {TypeKind::named,0,"tls_stream"};
                if (name == "http_get" || name == "http_request") return {TypeKind::named,0,"http_response"};
                if (name == "http_server") return {TypeKind::named,0,"http_server"};
                if (name == "http_text" || name == "http_html" || name == "http_json_response") return {TypeKind::named,0,"http_server_response"};
                if (name == "sqlite_open") return {TypeKind::named,0,"sqlite_db"};
                if (name == "embed_file") return builtin_type("string");
                if (name == "embed_dir") return {TypeKind::named,0,"map<string,string>"};
                if (name == "http_get_json") return builtin_type("json");
                if (name == "http_get_async" || name == "http_request_async") return {TypeKind::named,0,"future<http_response>"};
                if (name == "thread") { for(std::size_t i=1;i<expr.arguments.size();++i){auto t=infer_expression(result,*expr.arguments[i]);if(t.name.rfind("ref<",0)==0)result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"T& cannot be passed directly across a thread boundary; use T* or synchronize owned state"});} return {TypeKind::named,0,"thread"}; }
                if (name == "now_ms" || name == "unix_ms") return builtin_type("int_64");
                if (name == "make_dir" || name == "remove" || name == "remove_all" || name == "copy" || name == "move" || name == "touch" || name == "cd" || name == "write_file" || name == "append_file") { require_module(result, "filesystem", expr.span, name); return {TypeKind::void_type,0,"void"}; }
                if (name == "set_env" || name == "unset_env" || name == "sleep_ms") return {TypeKind::void_type,0,"void"};
                if (auto* fn = lookup(name, SymbolNamespace::function)) return resolve_type(function_return(fn->type_name));
                if (auto* value = lookup(name, SymbolNamespace::value)) { if(value->type_name.rfind("function<(",0)==0) return resolve_type(function_return(value->type_name)); if(value->type_name=="async_function") return {TypeKind::named,0,"future<opaque>"}; }
            }
            return {TypeKind::named, 0, "opaque"};
        }
        case Expr::Kind::lambda: {
            push_scope();
            if(expr.lambda){for(const auto& p:expr.lambda->parameters)declare(result,Symbol{p.name,SymbolNamespace::value,p.span,true,p.type.name.empty()?"opaque":resolved_type_name(p.type.name)});if(expr.lambda->expression_body)infer_expression(result,*expr.lambda->expression_body);analyze_statements(result,expr.lambda->body,false);}
            pop_scope();
            return {TypeKind::named, 0, expr.lambda && expr.lambda->is_async ? "async_function" : "function"};
        }
        case Expr::Kind::member: {
            if(expr.text.rfind("::",0)==0 && expr.left && expr.left->kind==Expr::Kind::identifier){auto it=enum_members_.find(expr.left->text);std::string member=expr.text.substr(2);if(it==enum_members_.end()){result.diagnostics.push_back(Diagnostic{expr.span,"unknown enum type '"+expr.left->text+"'"});return {};}if(it->second.find(member)==it->second.end())result.diagnostics.push_back(Diagnostic{expr.span,"unknown enum member '"+member+"' for "+expr.left->text});return {TypeKind::named,0,expr.left->text};}
            auto base=infer_expression(result,*expr.left);
            if (is_nullable_type(base.name)) { result.diagnostics.push_back(Diagnostic{expr.span,"cannot access member of nullable value without ?. or null check"}); return {}; }
            const bool arrow=expr.text.rfind("->",0)==0;
            const std::string member=arrow?expr.text.substr(2):expr.text;
            if(arrow){const bool raw=base.name.rfind("raw_ptr<",0)==0;const bool safe=base.name.rfind("ptr<",0)==0;if(!raw&&!safe)result.diagnostics.push_back(Diagnostic{expr.span,"-> member access requires T* or unsafe ptr<T>"});if(raw&&unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"ptr<T> member access requires unsafe block"});}
            std::string owner=base.name;for(auto head:{std::string_view("ref<"),std::string_view("ptr<"),std::string_view("raw_ptr<")}){auto inner=generic_inner(owner,head);if(!inner.empty()){owner=inner;break;}}if(owner.rfind("const ",0)==0)owner=owner.substr(6);auto sit=struct_fields_.find(owner);if(sit!=struct_fields_.end()){auto f=sit->second.find(member);if(f!=sit->second.end())return resolve_type(f->second);}
            return {TypeKind::named,0,"opaque"};
        }
        case Expr::Kind::safe_member: {
            auto base=infer_expression(result,*expr.left);
            if (!is_nullable_type(base.name)) result.diagnostics.push_back(Diagnostic{expr.span,"?. requires a nullable value"});
            auto sit=struct_fields_.find(strip_nullable(base.name));if(sit!=struct_fields_.end()){auto f=sit->second.find(expr.text);if(f!=sit->second.end()){auto t=resolved_type_name(f->second);return {TypeKind::named,0,is_nullable_type(t)?t:t+"?"};}}
            return {TypeKind::named,0,"opaque?"};
        }
        case Expr::Kind::index: {
            auto base=infer_expression(result,*expr.left);
            if(base.name.rfind("tuple<",0)==0 && base.name.back()=='>'){
                if(expr.right->kind!=Expr::Kind::integer_literal){result.diagnostics.push_back(Diagnostic{expr.right->span,"tuple index must be an integer literal"});return {};}
                std::size_t idx=0;try{idx=static_cast<std::size_t>(std::stoull(expr.right->text));}catch(...){return {};}
                auto inner=base.name.substr(6,base.name.size()-7);std::vector<std::string> parts;int depth=0;std::size_t start=0;
                for(std::size_t i=0;i<=inner.size();++i){char c=i<inner.size()?inner[i]:',';if(c=='<')++depth;else if(c=='>')--depth;else if(c==','&&depth==0){parts.push_back(inner.substr(start,i-start));start=i+1;}}
                if(idx>=parts.size()){result.diagnostics.push_back(Diagnostic{expr.right->span,"tuple index out of range"});return {};}
                return resolve_type(parts[idx]);
            }
            return {TypeKind::named, 0, "opaque"};
        }
    }
    return {};
}

void SemanticAnalyzer::analyze_statements(SemanticResult& result, const std::vector<StmtPtr>& statements, bool create_scope) {
    if (create_scope) push_scope();
    for (const auto& statement : statements) analyze_statement(result, *statement);
    if (create_scope) pop_scope();
}

void SemanticAnalyzer::analyze_statement(SemanticResult& result, const Stmt& st) {
    if (st.declared_type) require_type_module(result, st.declared_type->name, st.declared_type->span);
    if (st.alias_target) require_type_module(result, st.alias_target->name, st.alias_target->span);
    if (st.return_type) require_type_module(result, st.return_type->name, st.return_type->span);
    for (const auto& p : st.parameters) require_type_module(result, p.type.name, p.type.span);
    for (const auto& f : st.fields) require_type_module(result, f.type.name, f.type.span);
    switch (st.kind) {
        case Stmt::Kind::declaration: {
            TypeInfo value_type = st.value ? infer_expression(result, *st.value) : TypeInfo{};
            std::string type_name;
            if (st.declared_type) {
                auto destination = resolve_type(st.declared_type->name);
                if (!destination.valid()) {
                    result.diagnostics.push_back(Diagnostic{st.declared_type->span, "unknown type '" + st.declared_type->name + "'"});
                }
                type_name = resolved_type_name(st.declared_type->name);
                if (st.value && st.value->kind == Expr::Kind::array_literal) {
                    if (auto extent = array_extent(st.declared_type->name); extent && *extent != st.value->arguments.size()) {
                        result.diagnostics.push_back(Diagnostic{st.value->span, "fixed array initializer has " + std::to_string(st.value->arguments.size()) + " elements but type requires " + std::to_string(*extent)});
                    }
                }
                bool literal_integer_ok = false;
                if (destination.numeric() && st.value && st.value->kind == Expr::Kind::integer_literal) {
                    if ((destination.kind == TypeKind::signed_int || destination.kind == TypeKind::unsigned_int) && integer_literal_fits(st.value->text, destination)) literal_integer_ok = true;
                    if (!literal_integer_ok && (destination.kind == TypeKind::signed_int || destination.kind == TypeKind::unsigned_int)) {
                        result.diagnostics.push_back(Diagnostic{st.value->span, "integer literal does not fit " + st.declared_type->name});
                    }
                }
                bool reference_bind_ok = false;
                if (st.value && destination.name.rfind("ref<",0)==0) {
                    const auto inner = generic_inner(destination.name,"ref<");
                    const auto target = resolve_type(inner.rfind("const ",0)==0 ? inner.substr(6) : inner);
                    const bool lvalue = st.value->kind==Expr::Kind::identifier || st.value->kind==Expr::Kind::member || st.value->kind==Expr::Kind::index || (st.value->kind==Expr::Kind::unary && st.value->text=="*");
                    const bool existing_ref = value_type.name.rfind("ref<",0)==0;
                    reference_bind_ok = (lvalue && compatible(value_type,target)) || (existing_ref && compatible(value_type,destination));
                    if(!lvalue && !existing_ref) result.diagnostics.push_back(Diagnostic{st.value->span,"T& requires an lvalue with a lifetime that outlives the reference"});
                }
                if (!literal_integer_ok && !reference_bind_ok && destination.valid() && value_type.valid() && !compatible(value_type, destination)) {
                    const auto init_key="infix::=|"+normalize_operator_type(resolved_type_name(st.declared_type->name))+","+normalize_operator_type(value_type.name);
                    if(operator_returns_.find(init_key)==operator_returns_.end()) result.diagnostics.push_back(Diagnostic{st.value->span, "cannot initialize '" + st.name + "' of type " + st.declared_type->name + " from incompatible value"});
                }
            } else {
                if (!value_type.valid()) result.diagnostics.push_back(Diagnostic{st.span, "cannot infer type of '" + st.name + "'"});
                type_name = value_type.name.empty() ? "opaque" : std::string(value_type.name);
                require_type_module(result, type_name, st.span);
            }
            if(type_name.rfind("raw_ptr<",0)==0 && unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{st.span,"ptr<T> values may only be created inside unsafe blocks"});
            declare(result, Symbol{st.name, SymbolNamespace::value, st.span, st.is_const, type_name});
            break;
        }
        case Stmt::Kind::assignment: {
            TypeInfo lhs; std::string label=st.name.empty()?"assignment target":st.name;
            if(st.target){
                if(st.target->kind==Expr::Kind::unary && st.target->text=="*" && st.target->right){
                    auto owner=infer_expression(result,*st.target->right);
                    auto inner=generic_inner(owner.name,"ptr<"); if(inner.empty())inner=generic_inner(owner.name,"raw_ptr<");
                    if(inner.rfind("const ",0)==0){result.diagnostics.push_back(Diagnostic{st.target->span,"cannot modify through pointer to const value"});inner=inner.substr(6);}
                    lhs=resolve_type(inner);
                } else lhs=infer_expression(result,*st.target);
            } else {
                auto* target = lookup(st.name, SymbolNamespace::value);
                if (!target) { result.diagnostics.push_back(Diagnostic{st.span, "assignment to unknown value '" + st.name + "'"}); break; }
                if (target->is_const) result.diagnostics.push_back(Diagnostic{st.span, "cannot assign to const value '" + st.name + "'"});
                if (target->type_name.rfind("ref<",0)==0) result.diagnostics.push_back(Diagnostic{st.span,"T& bindings cannot be reassigned"});
                lhs=resolve_type(target->type_name);
            }
            if (st.value) { auto rhs=infer_expression(result,*st.value); if(rhs.valid()&&lhs.valid()&&!compatible(rhs,lhs)){const auto assign_key="infix:=|"+normalize_operator_type(lhs.name)+","+normalize_operator_type(rhs.name);if(operator_returns_.find(assign_key)==operator_returns_.end())result.diagnostics.push_back(Diagnostic{st.value->span,"incompatible assignment to '"+label+"'"});} }
            break;
        }
        case Stmt::Kind::type_alias:
            declare(result, Symbol{st.name, SymbolNamespace::type, st.span, true, {}});
            if (st.alias_target) aliases_[st.name] = st.alias_target->name;
            break;
        case Stmt::Kind::enum_decl: {
            declare(result,Symbol{st.name,SymbolNamespace::type,st.span,true,st.name});auto& set=enum_members_[st.name];for(const auto& n:st.enum_names)if(!set.insert(n).second)result.diagnostics.push_back(Diagnostic{st.span,"duplicate enum member '"+n+"'"});break;
        }
        case Stmt::Kind::struct_decl: {
            declare(result, Symbol{st.name, SymbolNamespace::type, st.span, true, st.name});
            auto& fields=struct_fields_[st.name];
            int data_bases=0;
            for(const auto& base:st.bases){auto b=struct_fields_.find(base);if(b==struct_fields_.end()&&abstract_methods_.find(base)==abstract_methods_.end()){result.diagnostics.push_back(Diagnostic{st.span,"unknown base struct '"+base+"'"});continue;}if(b!=struct_fields_.end()&&!b->second.empty()){++data_bases;for(const auto& f:b->second)fields.emplace(f.first,f.second);}}
            if(data_bases>1)result.diagnostics.push_back(Diagnostic{st.span,"multiple data-bearing base structs are not supported; use one concrete base plus contracts"});
            for(const auto& field:st.fields){if(fields.find(field.name)!=fields.end())result.diagnostics.push_back(Diagnostic{field.span,"duplicate field '"+field.name+"'"});else {auto ft=resolved_type_name(field.type.name);if(ft.rfind("ref<",0)==0)result.diagnostics.push_back(Diagnostic{field.span,"T& struct fields require lifetime proof and are not yet allowed"});fields[field.name]=ft;}}
            std::unordered_set<std::string> own_methods; for(const auto& method:st.body)if(!own_methods.insert(method->name).second)result.diagnostics.push_back(Diagnostic{method->span,"duplicate/conflicting method declaration '"+method->name+"' in struct "+st.name});
            for(const auto& method:st.body){push_scope();declare(result,Symbol{"this",SymbolNamespace::value,method->span,true,st.name});for(const auto& field:fields)declare(result,Symbol{field.first,SymbolNamespace::value,method->span,false,resolved_type_name(field.second)});for(const auto& param:method->parameters)declare(result,Symbol{param.name,SymbolNamespace::value,param.span,param.type.is_const,resolved_type_name(param.type.name)});if(method->has_body)analyze_statements(result,method->body,false);pop_scope();}
            break;
        }
        case Stmt::Kind::operator_decl: {
            std::string signature;for(std::size_t i=0;i<st.parameters.size();++i){if(i)signature+=",";signature+=normalize_operator_type(resolved_type_name(st.parameters[i].type.name));}
            const std::string fixity=st.parameters.size()==1?"prefix":"infix";const std::string key=fixity+":"+st.op;
            auto& seen=operator_signatures_[key];if(!seen.insert(signature).second)result.diagnostics.push_back(Diagnostic{st.span,"ambiguous duplicate operator overload for '"+st.op+"' with signature ("+signature+")"});
            if(st.has_body){const auto previous_return=current_function_return_type_;current_function_return_type_=st.return_type?st.return_type->name:"void";push_scope();for(const auto& p:st.parameters)declare(result,Symbol{p.name,SymbolNamespace::value,p.span,p.type.is_const,resolved_type_name(p.type.name)});if(st.value)infer_expression(result,*st.value);else analyze_statements(result,st.body,false);pop_scope();current_function_return_type_=previous_return;}
            break;
        }
        case Stmt::Kind::function_decl: {
            declare(result, Symbol{st.name, SymbolNamespace::function, st.span, true, function_signature(st)});
            if (st.has_body) {
                const auto previous_return=current_function_return_type_; const auto previous_errors=current_function_errors_; current_function_return_type_=st.return_type?st.return_type->name:"void"; current_function_errors_.clear();for(const auto& e:st.error_types)current_function_errors_.insert(resolved_type_name(e.name));
                push_scope();
                if(!st.owner.empty()){declare(result,Symbol{"this",SymbolNamespace::value,st.span,true,st.owner});auto fit=struct_fields_.find(st.owner);if(fit!=struct_fields_.end())for(const auto& f:fit->second)declare(result,Symbol{f.first,SymbolNamespace::value,st.span,false,f.second});}
                for (const auto& p : st.parameters) declare(result, Symbol{p.name, SymbolNamespace::value, p.span, p.type.is_const, resolved_type_name(p.type.name)});
                analyze_statements(result, st.body, false);
                pop_scope(); current_function_return_type_=previous_return; current_function_errors_=previous_errors;
            }
            break;
        }
        case Stmt::Kind::include_stmt: break;
        case Stmt::Kind::unsafe_stmt: ++unsafe_depth_; analyze_statements(result,st.body,true); --unsafe_depth_; break;
        case Stmt::Kind::block: analyze_statements(result, st.body, true); break;
        case Stmt::Kind::if_stmt: {
            if (st.condition) infer_expression(result, *st.condition);
            Symbol* narrowed = nullptr; std::string original;
            if (st.condition && st.condition->kind==Expr::Kind::binary && st.condition->text=="!=" &&
                st.condition->left && st.condition->left->kind==Expr::Kind::identifier && st.condition->right && st.condition->right->kind==Expr::Kind::null_literal) {
                narrowed=lookup(st.condition->left->text,SymbolNamespace::value);
                if(narrowed && is_nullable_type(narrowed->type_name)){original=narrowed->type_name;narrowed->type_name=strip_nullable(original);}
            }
            analyze_statements(result, st.body, true);
            if(narrowed) narrowed->type_name=original;
            analyze_statements(result, st.else_body, true); break;
        }
        case Stmt::Kind::while_stmt:
            if (st.condition) infer_expression(result, *st.condition);
            analyze_statements(result, st.body, true); break;
        case Stmt::Kind::switch_stmt: {
            auto subject=st.condition?infer_expression(result,*st.condition):TypeInfo{};
            if(subject.valid() && !(subject.kind==TypeKind::signed_int||subject.kind==TypeKind::unsigned_int||enum_members_.find(subject.name)!=enum_members_.end())) result.diagnostics.push_back(Diagnostic{st.span,"switch currently requires an integer or enum expression"});
            bool has_default=false;std::unordered_set<std::string> seen;
            for(const auto& c:st.switch_cases){if(c.is_default){has_default=true;}else if(c.value){auto ct=infer_expression(result,*c.value);if(subject.valid()&&ct.valid()&&!compatible(ct,subject))result.diagnostics.push_back(Diagnostic{c.value->span,"switch case type is incompatible with subject"});std::string key=c.value->text;if(!key.empty()&&!seen.insert(key).second)result.diagnostics.push_back(Diagnostic{c.value->span,"duplicate switch case"});}analyze_statements(result,c.body,true);}
            if(!has_default && subject.valid() && enum_members_.find(subject.name)!=enum_members_.end()) result.warnings.push_back(Diagnostic{st.span,"enum switch has no default; exhaustiveness is checked more strictly by match"});
            break;
        }
        case Stmt::Kind::match_stmt: {
            auto subject=st.condition?infer_expression(result,*st.condition):TypeInfo{};bool wildcard=false;std::unordered_set<std::string> matched;
            for(const auto& c:st.switch_cases){if(c.is_default){wildcard=true;}else if(c.value){auto pt=infer_expression(result,*c.value);if(subject.valid()&&pt.valid()&&!compatible(pt,subject))result.diagnostics.push_back(Diagnostic{c.value->span,"match pattern type is incompatible with subject"});if(c.value->kind==Expr::Kind::member&&c.value->text.rfind("::",0)==0)matched.insert(c.value->text.substr(2));}analyze_statements(result,c.body,true);}
            auto eit=enum_members_.find(subject.name);if(!wildcard&&eit!=enum_members_.end()){std::vector<std::string> missing;for(const auto& n:eit->second)if(matched.find(n)==matched.end())missing.push_back(n);if(!missing.empty()){std::string list;for(const auto& n:missing){if(!list.empty())list+=", ";list+=n;}result.diagnostics.push_back(Diagnostic{st.span,"non-exhaustive enum match; missing: "+list});}}
            break;
        }
        case Stmt::Kind::for_stmt:
            push_scope(); if (st.initializer) analyze_statement(result, *st.initializer); if (st.condition) infer_expression(result,*st.condition); if(st.increment) infer_expression(result,*st.increment); analyze_statements(result, st.body, false); pop_scope(); break;
        case Stmt::Kind::range_for:
            if (st.value) infer_expression(result, *st.value);
            push_scope(); declare(result, Symbol{st.name, SymbolNamespace::value, st.span, false, "opaque"}); analyze_statements(result, st.body, false); pop_scope(); break;
        case Stmt::Kind::expression: if (st.value) infer_expression(result, *st.value); break;
        case Stmt::Kind::return_stmt: if (st.value) {
            auto returned = infer_expression(result,*st.value);
            if(current_function_return_type_.rfind("ref<",0)==0) {
                bool safe_escape=false;
                if(st.value->kind==Expr::Kind::identifier) {
                    if(auto* symbol=lookup(st.value->text,SymbolNamespace::value)) {
                        safe_escape = symbol->type_name.rfind("ref<",0)==0 && compatible(resolve_type(symbol->type_name), resolve_type(current_function_return_type_));
                    }
                }
                if(!safe_escape) result.diagnostics.push_back(Diagnostic{st.span,"returning T& is only permitted when returning an existing compatible T& binding"});
            }
            (void)returned;
        } break;
        case Stmt::Kind::throw_stmt: {
            std::string thrown; if(st.value){if(st.value->kind==Expr::Kind::call&&st.value->left&&st.value->left->kind==Expr::Kind::identifier){thrown=st.value->left->text;for(const auto& a:st.value->arguments)infer_expression(result,*a);}else if(st.value->kind==Expr::Kind::struct_literal)thrown=st.value->text;else {auto t=infer_expression(result,*st.value);thrown=t.name;}}
            thrown=resolved_type_name(thrown);if(current_function_errors_.find(thrown)==current_function_errors_.end() && catch_all_depth_==0)result.diagnostics.push_back(Diagnostic{st.span,"throw of checked error "+thrown+" is not declared in function signature"});break;
        }
        case Stmt::Kind::try_stmt: {
            const auto saved_errors=current_function_errors_;
            bool has_catch_all=false;
            for(const auto& c:st.catches){if(!c.type){has_catch_all=true;}else current_function_errors_.insert(resolved_type_name(c.type->name));}
            if(has_catch_all)++catch_all_depth_;
            analyze_statements(result,st.body,true);
            if(has_catch_all)--catch_all_depth_;
            current_function_errors_=saved_errors;
            for(const auto& c:st.catches){push_scope();if(c.type && !c.name.empty())declare(result,Symbol{c.name,SymbolNamespace::value,c.span,true,resolved_type_name(c.type->name)});analyze_statements(result,c.body,false);pop_scope();}
            break;
        }
        default: break;
    }
}

bool SemanticAnalyzer::resolve_alias(SemanticResult& result, const std::string& name, std::unordered_set<std::string>& visiting) {
    auto it = aliases_.find(name); if (it == aliases_.end()) return builtin_type(name).valid();
    if (visiting.find(name) != visiting.end()) { result.diagnostics.push_back(Diagnostic{{}, "cyclic type alias involving '" + name + "'"}); return false; }
    visiting.insert(name); const std::string target = it->second;
    bool ok = builtin_type(target).valid() || aliases_.find(target) != aliases_.end();
    if (aliases_.find(target) != aliases_.end()) ok = resolve_alias(result, target, visiting);
    if (!ok && (target.find('<') != std::string::npos || target.find('[') != std::string::npos || target.rfind("function",0)==0)) ok = true;
    if (!ok) result.diagnostics.push_back(Diagnostic{{}, "unknown type '" + target + "' in alias '" + name + "'"});
    visiting.erase(name); return ok;
}

SemanticResult SemanticAnalyzer::analyze(const Program& program) {
    SemanticResult result; scopes_.clear(); aliases_.clear(); struct_fields_.clear(); abstract_methods_.clear(); struct_bases_.clear(); enum_members_.clear(); named_types_.clear(); current_function_return_type_.clear(); current_function_errors_.clear(); function_errors_.clear(); operator_signatures_.clear(); operator_returns_.clear(); extern_c_functions_.clear(); unsafe_depth_=0; catch_all_depth_=0; enforce_standard_modules_=program.enforce_standard_modules; standard_modules_.clear(); standard_modules_.insert(program.standard_modules.begin(), program.standard_modules.end());
    named_types_.insert("http_request"); named_types_.insert("http_server_response"); named_types_.insert("http_server"); named_types_.insert("SqliteError"); named_types_.insert("sqlite_db"); named_types_.insert("EmbedError"); named_types_.insert("FilesystemError"); named_types_.insert("StreamError"); named_types_.insert("EnvironmentError"); named_types_.insert("TimeError"); named_types_.insert("ExecError"); named_types_.insert("exec_result"); named_types_.insert("process"); named_types_.insert("thread"); named_types_.insert("ThreadError"); named_types_.insert("process_in"); named_types_.insert("process_out"); named_types_.insert("mutex"); named_types_.insert("MutexError"); named_types_.insert("NetworkError"); named_types_.insert("tcp_socket"); named_types_.insert("tcp_listener"); named_types_.insert("TlsError"); named_types_.insert("tls_stream"); named_types_.insert("HttpError"); named_types_.insert("http_response");
    struct_fields_["exec_result"]={{"exit_code","int"},{"stdout","string"},{"stderr","string"}};
    struct_fields_["http_response"]={{"status","int"},{"body","string"},{"headers","map<string,string>"}};
    struct_fields_["http_request"]={{"method","string"},{"path","string"},{"body","string"},{"headers","map<string,string>"},{"query","map<string,string>"},{"params","map<string,string>"}};
    struct_fields_["http_server_response"]={{"status","int"},{"body","string"},{"content_type","string"},{"headers","map<string,string>"}};
    struct_fields_["process"]={{"in","process_in"},{"out","process_out"},{"err","process_out"}};
    for(const auto& t:{std::string("istream"),std::string("ostream"),std::string("sstream"),std::string("ifstream"),std::string("ofstream"),std::string("bytes")})named_types_.insert(t);
    for(const auto& name:{std::string("exists"),std::string("is_file"),std::string("is_dir"),std::string("file_size"),std::string("modified"),std::string("make_dir"),std::string("remove"),std::string("remove_all"),std::string("copy"),std::string("move"),std::string("touch"),std::string("ls"),std::string("walk"),std::string("cwd"),std::string("cd"),std::string("absolute"),std::string("canonical"),std::string("read_file"),std::string("read_bytes"),std::string("write_file"),std::string("append_file")})function_errors_[name].insert("FilesystemError");
    for(const auto& name:{std::string("set_env"),std::string("unset_env")})function_errors_[name].insert("EnvironmentError");
    function_errors_["sleep_ms"].insert("TimeError");
    function_errors_["http_get"].insert("HttpError"); function_errors_["http_request"].insert("HttpError"); function_errors_["http_get_json"].insert("HttpError"); function_errors_["http_get_async"].insert("HttpError"); function_errors_["http_request_async"].insert("HttpError");
    function_errors_["sqlite_open"].insert("SqliteError"); function_errors_["embed_file"].insert("EmbedError"); function_errors_["embed_dir"].insert("EmbedError");
    function_errors_["tls_connect"].insert("TlsError"); function_errors_["tcp_connect"].insert("NetworkError"); function_errors_["tcp_connect_async"].insert("NetworkError"); function_errors_["tcp_listen"].insert("NetworkError");
    function_errors_["exec"].insert("ExecError"); function_errors_["exec_shell"].insert("ExecError"); function_errors_["process"].insert("ExecError"); function_errors_["pipe_exec"].insert("ExecError");
    for(const auto& st:program.statements)if(st->kind==Stmt::Kind::struct_decl){named_types_.insert(st->name);struct_bases_[st->name]=st->bases;for(const auto& m:st->body)if(m->kind==Stmt::Kind::function_decl&&!m->has_body)abstract_methods_[st->name].insert(m->name);for(const auto& m:st->body)if(m->kind==Stmt::Kind::function_decl&&m->has_body)abstract_methods_[st->name].erase(m->name);}
    for(const auto& st:program.statements)if(st->kind==Stmt::Kind::enum_decl)named_types_.insert(st->name);
    for(std::size_t pass=0;pass<program.statements.size()+1;++pass)for(const auto& st:program.statements)if(st->kind==Stmt::Kind::struct_decl){for(const auto& base:st->bases){auto it=abstract_methods_.find(base);if(it!=abstract_methods_.end())abstract_methods_[st->name].insert(it->second.begin(),it->second.end());}for(const auto& m:st->body)if(m->kind==Stmt::Kind::function_decl&&m->has_body)abstract_methods_[st->name].erase(m->name);}
    for(const auto& st:program.statements)if(st->kind==Stmt::Kind::function_decl&&!st->owner.empty()&&st->has_body)abstract_methods_[st->owner].erase(st->name);
    aliases_["int"]="int_32"; aliases_["uint"]="uint_32"; aliases_["double"]="double_32";
    for(const auto& st:program.statements)if(st->kind==Stmt::Kind::function_decl){if(st->is_extern_c)extern_c_functions_.insert(st->name);auto& errs=function_errors_[st->name];for(const auto& e:st->error_types)errs.insert(resolved_type_name(e.name));}
    for(const auto& st:program.statements)if(st->kind==Stmt::Kind::operator_decl){std::string signature;for(std::size_t i=0;i<st->parameters.size();++i){if(i)signature+=",";signature+=resolved_type_name(st->parameters[i].type.name);}const std::string fixity=st->parameters.size()==1?"prefix":"infix";operator_returns_[fixity+":"+st->op+"|"+signature]=st->return_type?resolved_type_name(st->return_type->name):"void";}
    push_scope();
    for (const auto& [name,target] : aliases_) { (void)target; declare(result, Symbol{name,SymbolNamespace::type,{},true,{}}); }
    analyze_statements(result, program.statements, false);
    for (const auto& [name,target] : aliases_) { (void)target; std::unordered_set<std::string> visiting; resolve_alias(result,name,visiting); }
    std::unordered_map<std::string,const Stmt*> structs; for(const auto& st:program.statements)if(st->kind==Stmt::Kind::struct_decl)structs[st->name]=st.get();
    std::unordered_set<std::string> warned;
    for(const auto& [name,st]:structs){for(const auto& field:st->fields){auto target=generic_inner(field.type.name,"ptr<");if(target.rfind("const ",0)==0)target=target.substr(6);auto it=structs.find(target);if(it==structs.end())continue;for(const auto& back:it->second->fields){auto back_target=generic_inner(back.type.name,"ptr<");if(back_target.rfind("const ",0)==0)back_target=back_target.substr(6);if(back_target==name){auto key=name<target?name+":"+target:target+":"+name;if(warned.insert(key).second)result.warnings.push_back(Diagnostic{field.span,"possible reference cycle between "+name+" and "+target+"; consider weak_ptr<T> for a back-reference"});}}}}
    pop_scope(); return result;
}

} // namespace strut
