#include "strut/sema.h"

#include <algorithm>
#include <cctype>

namespace strut {
namespace {
std::string function_signature(const Stmt& st){std::string sig="function<(";for(std::size_t i=0;i<st.parameters.size();++i){if(i)sig+=",";sig+=st.parameters[i].type.name;}sig+=")->"+(st.return_type?st.return_type->name:std::string("void"))+">";return sig;}
std::string function_return(std::string_view sig){auto p=sig.rfind(")->");if(p==std::string_view::npos||sig.empty()||sig.back()!='>')return "opaque";return std::string(sig.substr(p+3,sig.size()-(p+4)));}
std::string generic_inner(std::string_view type,std::string_view head){if(type.rfind(head,0)!=0||type.size()<=head.size()+1||type.back()!='>')return {};return std::string(type.substr(head.size(),type.size()-head.size()-1));}
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
            if(expr.text=="*"){if(operand.name.rfind("raw_ptr<",0)==0 && unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"raw_ptr<T> dereference requires unsafe block"});for(auto head:{std::string_view("ptr<"),std::string_view("raw_ptr<"),std::string_view("ref<")}){auto inner=generic_inner(operand.name,head);if(!inner.empty())return resolve_type(inner);}}
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
            return {};
        }
        case Expr::Kind::call: {
            for (const auto& arg : expr.arguments) infer_expression(result, *arg);
            if (expr.left && expr.left->kind == Expr::Kind::member && expr.left->left && expr.left->left->kind == Expr::Kind::identifier && expr.left->left->text == "json") {
                if (expr.left->text == "parse" || expr.left->text == "encode") return builtin_type("json");
                if (expr.left->text == "stringify" || expr.left->text == "pretty") return builtin_type("string");
            }
            if(expr.left && expr.left->kind==Expr::Kind::member && expr.left->left){auto base=infer_expression(result,*expr.left->left);const auto& m=expr.left->text;std::string elem="opaque";if(base.name.size()>2&&base.name.compare(base.name.size()-2,2,"[]")==0)elem=base.name.substr(0,base.name.size()-2);if(m=="lock" && base.name.rfind("weak_ptr<",0)==0)return {TypeKind::named,0,"ptr<"+generic_inner(base.name,"weak_ptr<")+">"};if(m=="expired" && base.name.rfind("weak_ptr<",0)==0)return builtin_type("bool");if(m=="filter")return base;if(m=="map")return {TypeKind::named,0,"opaque[]"};if(m=="reduce")return resolve_type(elem);if(m=="any"||m=="all")return builtin_type("bool");if(m=="find")return {TypeKind::named,0,elem+"?"};if(m=="count")return builtin_type("int");if(m=="sort")return {TypeKind::void_type,0,"void"};}
            if (expr.left && expr.left->kind == Expr::Kind::identifier) {
                const auto& name = expr.left->text;
                if(name=="raw"){if(unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"raw(...) requires unsafe block"});if(expr.arguments.size()!=1){result.diagnostics.push_back(Diagnostic{expr.span,"raw(...) requires exactly one ptr<T>"});return {};}auto t=infer_expression(result,*expr.arguments[0]);auto inner=generic_inner(t.name,"ptr<");if(inner.empty())result.diagnostics.push_back(Diagnostic{expr.arguments[0]->span,"raw(...) currently requires ptr<T>"});return {TypeKind::named,0,"raw_ptr<"+inner+">"};}
                if(name=="weak"){if(expr.arguments.size()!=1){result.diagnostics.push_back(Diagnostic{expr.span,"weak(...) requires exactly one ptr<T>"});return {};}auto t=infer_expression(result,*expr.arguments[0]);auto inner=generic_inner(t.name,"ptr<");if(inner.empty())result.diagnostics.push_back(Diagnostic{expr.arguments[0]->span,"weak(...) requires ptr<T>"});return {TypeKind::named,0,"weak_ptr<"+inner+">"};}
                if(name=="ref"){if(expr.arguments.size()!=1){result.diagnostics.push_back(Diagnostic{expr.span,"ref(...) requires exactly one argument"});return {};}const auto& a=*expr.arguments[0];const bool lvalue=a.kind==Expr::Kind::identifier||a.kind==Expr::Kind::member||a.kind==Expr::Kind::index||(a.kind==Expr::Kind::unary&&a.text=="*");if(!lvalue)result.diagnostics.push_back(Diagnostic{a.span,"ref(...) requires an lvalue with a lifetime that outlives the reference"});if(a.kind==Expr::Kind::index&&a.left){auto owner=infer_expression(result,*a.left);if(owner.name.size()>2&&owner.name.compare(owner.name.size()-2,2,"[]")==0)result.diagnostics.push_back(Diagnostic{a.span,"ref<T> cannot borrow a dynamic-array element because later mutation could invalidate the reference"});}auto t=infer_expression(result,a);return {TypeKind::named,0,"ref<"+t.name+">"};}
                if (name == "print" || name == "input") return {TypeKind::void_type, 0, "void"};
                if (auto* fn = lookup(name, SymbolNamespace::function)) return resolve_type(function_return(fn->type_name));
                if (auto* value = lookup(name, SymbolNamespace::value); value && value->type_name.rfind("function<(",0)==0) return resolve_type(function_return(value->type_name));
            }
            return {TypeKind::named, 0, "opaque"};
        }
        case Expr::Kind::lambda: {
            push_scope();
            if(expr.lambda){for(const auto& p:expr.lambda->parameters)declare(result,Symbol{p.name,SymbolNamespace::value,p.span,true,p.type.name.empty()?"opaque":resolved_type_name(p.type.name)});if(expr.lambda->expression_body)infer_expression(result,*expr.lambda->expression_body);analyze_statements(result,expr.lambda->body,false);}
            pop_scope();
            return {TypeKind::named, 0, "function"};
        }
        case Expr::Kind::member: {
            auto base=infer_expression(result,*expr.left);
            if (is_nullable_type(base.name)) { result.diagnostics.push_back(Diagnostic{expr.span,"cannot access member of nullable value without ?. or null check"}); return {}; }
            auto sit=struct_fields_.find(base.name);if(sit!=struct_fields_.end()){auto f=sit->second.find(expr.text);if(f!=sit->second.end())return resolve_type(f->second);}
            return {TypeKind::named,0,"opaque"};
        }
        case Expr::Kind::safe_member: {
            auto base=infer_expression(result,*expr.left);
            if (!is_nullable_type(base.name)) result.diagnostics.push_back(Diagnostic{expr.span,"?. requires a nullable value"});
            auto sit=struct_fields_.find(strip_nullable(base.name));if(sit!=struct_fields_.end()){auto f=sit->second.find(expr.text);if(f!=sit->second.end()){auto t=resolved_type_name(f->second);return {TypeKind::named,0,is_nullable_type(t)?t:t+"?"};}}
            return {TypeKind::named,0,"opaque?"};
        }
        case Expr::Kind::index: return {TypeKind::named, 0, "opaque"};
    }
    return {};
}

void SemanticAnalyzer::analyze_statements(SemanticResult& result, const std::vector<StmtPtr>& statements, bool create_scope) {
    if (create_scope) push_scope();
    for (const auto& statement : statements) analyze_statement(result, *statement);
    if (create_scope) pop_scope();
}

void SemanticAnalyzer::analyze_statement(SemanticResult& result, const Stmt& st) {
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
                if (!literal_integer_ok && destination.valid() && value_type.valid() && !compatible(value_type, destination)) {
                    result.diagnostics.push_back(Diagnostic{st.value->span, "cannot initialize '" + st.name + "' of type " + st.declared_type->name + " from incompatible value"});
                }
            } else {
                if (!value_type.valid()) result.diagnostics.push_back(Diagnostic{st.span, "cannot infer type of '" + st.name + "'"});
                type_name = value_type.name.empty() ? "opaque" : std::string(value_type.name);
            }
            if(type_name.rfind("raw_ptr<",0)==0 && unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{st.span,"raw_ptr<T> values may only be created inside unsafe blocks"});
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
                if (target->type_name.rfind("ref<",0)==0) result.diagnostics.push_back(Diagnostic{st.span,"ref<T> bindings cannot be reassigned"});
                lhs=resolve_type(target->type_name);
            }
            if (st.value) { auto rhs=infer_expression(result,*st.value); if(rhs.valid()&&lhs.valid()&&!compatible(rhs,lhs))result.diagnostics.push_back(Diagnostic{st.value->span,"incompatible assignment to '"+label+"'"}); }
            break;
        }
        case Stmt::Kind::type_alias:
            declare(result, Symbol{st.name, SymbolNamespace::type, st.span, true, {}});
            if (st.alias_target) aliases_[st.name] = st.alias_target->name;
            break;
        case Stmt::Kind::struct_decl: {
            declare(result, Symbol{st.name, SymbolNamespace::type, st.span, true, st.name});
            auto& fields=struct_fields_[st.name];
            int data_bases=0;
            for(const auto& base:st.bases){auto b=struct_fields_.find(base);if(b==struct_fields_.end()&&abstract_methods_.find(base)==abstract_methods_.end()){result.diagnostics.push_back(Diagnostic{st.span,"unknown base struct '"+base+"'"});continue;}if(b!=struct_fields_.end()&&!b->second.empty()){++data_bases;for(const auto& f:b->second)fields.emplace(f.first,f.second);}}
            if(data_bases>1)result.diagnostics.push_back(Diagnostic{st.span,"multiple data-bearing base structs are not supported; use one concrete base plus contracts"});
            for(const auto& field:st.fields){if(fields.find(field.name)!=fields.end())result.diagnostics.push_back(Diagnostic{field.span,"duplicate field '"+field.name+"'"});else {auto ft=resolved_type_name(field.type.name);if(ft.rfind("ref<",0)==0)result.diagnostics.push_back(Diagnostic{field.span,"ref<T> struct fields require lifetime proof and are not yet allowed"});fields[field.name]=ft;}}
            std::unordered_set<std::string> own_methods; for(const auto& method:st.body)if(!own_methods.insert(method->name).second)result.diagnostics.push_back(Diagnostic{method->span,"duplicate/conflicting method declaration '"+method->name+"' in struct "+st.name});
            for(const auto& method:st.body){push_scope();declare(result,Symbol{"this",SymbolNamespace::value,method->span,true,st.name});for(const auto& field:fields)declare(result,Symbol{field.first,SymbolNamespace::value,method->span,false,resolved_type_name(field.second)});for(const auto& param:method->parameters)declare(result,Symbol{param.name,SymbolNamespace::value,param.span,false,resolved_type_name(param.type.name)});if(method->has_body)analyze_statements(result,method->body,false);pop_scope();}
            break;
        }
        case Stmt::Kind::function_decl: {
            declare(result, Symbol{st.name, SymbolNamespace::function, st.span, true, function_signature(st)});
            if (st.has_body) {
                const auto previous_return=current_function_return_type_; current_function_return_type_=st.return_type?st.return_type->name:"void";
                push_scope();
                if(!st.owner.empty()){declare(result,Symbol{"this",SymbolNamespace::value,st.span,true,st.owner});auto fit=struct_fields_.find(st.owner);if(fit!=struct_fields_.end())for(const auto& f:fit->second)declare(result,Symbol{f.first,SymbolNamespace::value,st.span,false,f.second});}
                for (const auto& p : st.parameters) declare(result, Symbol{p.name, SymbolNamespace::value, p.span, false, resolved_type_name(p.type.name)});
                analyze_statements(result, st.body, false);
                pop_scope(); current_function_return_type_=previous_return;
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
        case Stmt::Kind::for_stmt:
            push_scope(); if (st.initializer) analyze_statement(result, *st.initializer); if (st.condition) infer_expression(result,*st.condition); if(st.increment) infer_expression(result,*st.increment); analyze_statements(result, st.body, false); pop_scope(); break;
        case Stmt::Kind::range_for:
            if (st.value) infer_expression(result, *st.value);
            push_scope(); declare(result, Symbol{st.name, SymbolNamespace::value, st.span, false, "opaque"}); analyze_statements(result, st.body, false); pop_scope(); break;
        case Stmt::Kind::expression: if (st.value) infer_expression(result, *st.value); break;
        case Stmt::Kind::return_stmt: if (st.value) { infer_expression(result,*st.value); if(current_function_return_type_.rfind("ref<",0)==0) result.diagnostics.push_back(Diagnostic{st.span,"returning ref<T> is not permitted until lifetime proof can establish a safe escape"}); } break;
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
    SemanticResult result; scopes_.clear(); aliases_.clear(); struct_fields_.clear(); abstract_methods_.clear(); struct_bases_.clear(); current_function_return_type_.clear(); unsafe_depth_=0;
    for(const auto& st:program.statements)if(st->kind==Stmt::Kind::struct_decl){struct_bases_[st->name]=st->bases;for(const auto& m:st->body)if(m->kind==Stmt::Kind::function_decl&&!m->has_body)abstract_methods_[st->name].insert(m->name);for(const auto& m:st->body)if(m->kind==Stmt::Kind::function_decl&&m->has_body)abstract_methods_[st->name].erase(m->name);}
    for(std::size_t pass=0;pass<program.statements.size()+1;++pass)for(const auto& st:program.statements)if(st->kind==Stmt::Kind::struct_decl){for(const auto& base:st->bases){auto it=abstract_methods_.find(base);if(it!=abstract_methods_.end())abstract_methods_[st->name].insert(it->second.begin(),it->second.end());}for(const auto& m:st->body)if(m->kind==Stmt::Kind::function_decl&&m->has_body)abstract_methods_[st->name].erase(m->name);}
    for(const auto& st:program.statements)if(st->kind==Stmt::Kind::function_decl&&!st->owner.empty()&&st->has_body)abstract_methods_[st->owner].erase(st->name);
    aliases_["int"]="int_32"; aliases_["uint"]="uint_32"; aliases_["double"]="double_32";
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
