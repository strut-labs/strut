#include "strut/sema.h"

#include <algorithm>
#include <cctype>

namespace strut {

std::unordered_map<std::string, Symbol>& SemanticAnalyzer::namespace_map(Scope& scope, SymbolNamespace ns) {
    if (ns == SymbolNamespace::function) return scope.functions;
    if (ns == SymbolNamespace::type) return scope.types;
    return scope.values;
}
void SemanticAnalyzer::push_scope() { scopes_.push_back(Scope{}); }
void SemanticAnalyzer::pop_scope() { scopes_.pop_back(); }
bool SemanticAnalyzer::declare(SemanticResult& result, Symbol symbol) {
    auto& map = namespace_map(scopes_.back(), symbol.name_space);
    if (map.contains(symbol.name)) {
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
    std::string current(name);
    std::unordered_set<std::string> seen;
    while (aliases_.contains(current) && !seen.contains(current)) {
        seen.insert(current);
        current = aliases_.at(current);
    }
    return current;
}
TypeInfo SemanticAnalyzer::resolve_type(std::string_view name) const {
    const std::string resolved = resolved_type_name(name);
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
    if (from.kind == TypeKind::null_type) return to.kind == TypeKind::null_type || to.kind == TypeKind::named;
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
            auto fit=struct_fields_.find(expr.text);if(fit!=struct_fields_.end()){std::unordered_set<std::string> seen;for(std::size_t i=0;i<expr.names.size();++i){auto field=fit->second.find(expr.names[i]);if(field==fit->second.end()){result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"unknown field '"+expr.names[i]+"' for struct "+expr.text});continue;}if(!seen.insert(expr.names[i]).second)result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"duplicate struct field '"+expr.names[i]+"'"});auto value=infer_expression(result,*expr.arguments[i]);auto dest=resolve_type(field->second);if(value.valid()&&dest.valid()&&!compatible(value,dest))result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"incompatible value for field '"+expr.names[i]+"'"});}for(const auto& field:fit->second)if(!seen.contains(field.first))result.diagnostics.push_back(Diagnostic{expr.span,"missing field '"+field.first+"' for struct "+expr.text});}
            return {TypeKind::named,0,expr.text};
        }
        case Expr::Kind::identifier: {
            auto* symbol = lookup(expr.text, SymbolNamespace::value);
            if (!symbol) {
                result.diagnostics.push_back(Diagnostic{expr.span, "unknown value '" + expr.text + "'"});
                return {};
            }
            return resolve_type(symbol->type_name);
        }
        case Expr::Kind::grouping: return expr.left ? infer_expression(result, *expr.left) : TypeInfo{};
        case Expr::Kind::unary:
        case Expr::Kind::postfix: {
            auto operand = expr.right ? infer_expression(result, *expr.right) : (expr.left ? infer_expression(result, *expr.left) : TypeInfo{});
            if (expr.text == "!") return {TypeKind::bool_type, 0, "bool"};
            return operand;
        }
        case Expr::Kind::binary: {
            auto left = infer_expression(result, *expr.left);
            auto right = infer_expression(result, *expr.right);
            if (expr.text == "==" || expr.text == "!=" || expr.text == "<" || expr.text == "<=" || expr.text == ">" || expr.text == ">=" || expr.text == "&&" || expr.text == "||") {
                return {TypeKind::bool_type, 0, "bool"};
            }
            if (expr.text == "<<" || expr.text == ">>") return left;
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
            if (expr.left && expr.left->kind == Expr::Kind::identifier) {
                const auto& name = expr.left->text;
                if (name == "print" || name == "input") return {TypeKind::void_type, 0, "void"};
                if (auto* fn = lookup(name, SymbolNamespace::function)) return resolve_type(fn->type_name);
            }
            return {TypeKind::named, 0, "opaque"};
        }
        case Expr::Kind::lambda: return {TypeKind::named, 0, "function"};
        case Expr::Kind::member: {
            auto base=infer_expression(result,*expr.left);auto sit=struct_fields_.find(base.name);if(sit!=struct_fields_.end()){auto f=sit->second.find(expr.text);if(f!=sit->second.end())return resolve_type(f->second);}
            return {TypeKind::named,0,"opaque"};
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
            declare(result, Symbol{st.name, SymbolNamespace::value, st.span, st.is_const, type_name});
            break;
        }
        case Stmt::Kind::assignment: {
            auto* target = lookup(st.name, SymbolNamespace::value);
            if (!target) { result.diagnostics.push_back(Diagnostic{st.span, "assignment to unknown value '" + st.name + "'"}); break; }
            if (target->is_const) result.diagnostics.push_back(Diagnostic{st.span, "cannot assign to const value '" + st.name + "'"});
            if (st.value) {
                auto rhs = infer_expression(result, *st.value); auto lhs = resolve_type(target->type_name);
                if (rhs.valid() && lhs.valid() && !compatible(rhs, lhs)) result.diagnostics.push_back(Diagnostic{st.value->span, "incompatible assignment to '" + st.name + "'"});
            }
            break;
        }
        case Stmt::Kind::type_alias:
            declare(result, Symbol{st.name, SymbolNamespace::type, st.span, true, {}});
            if (st.alias_target) aliases_[st.name] = st.alias_target->name;
            break;
        case Stmt::Kind::struct_decl: {
            declare(result, Symbol{st.name, SymbolNamespace::type, st.span, true, st.name});
            auto& fields=struct_fields_[st.name];
            for(const auto& field:st.fields){if(fields.contains(field.name))result.diagnostics.push_back(Diagnostic{field.span,"duplicate field '"+field.name+"'"});else fields[field.name]=resolved_type_name(field.type.name);}
            for(const auto& method:st.body){push_scope();declare(result,Symbol{"this",SymbolNamespace::value,method->span,true,st.name});for(const auto& field:st.fields)declare(result,Symbol{field.name,SymbolNamespace::value,field.span,false,resolved_type_name(field.type.name)});for(const auto& param:method->parameters)declare(result,Symbol{param.name,SymbolNamespace::value,param.span,false,resolved_type_name(param.type.name)});if(method->has_body)analyze_statements(result,method->body,false);pop_scope();}
            break;
        }
        case Stmt::Kind::function_decl: {
            declare(result, Symbol{st.name, SymbolNamespace::function, st.span, true, st.return_type ? st.return_type->name : "void"});
            if (st.has_body) {
                push_scope();
                if(!st.owner.empty()){declare(result,Symbol{"this",SymbolNamespace::value,st.span,true,st.owner});auto fit=struct_fields_.find(st.owner);if(fit!=struct_fields_.end())for(const auto& f:fit->second)declare(result,Symbol{f.first,SymbolNamespace::value,st.span,false,f.second});}
                for (const auto& p : st.parameters) declare(result, Symbol{p.name, SymbolNamespace::value, p.span, false, resolved_type_name(p.type.name)});
                analyze_statements(result, st.body, false);
                pop_scope();
            }
            break;
        }
        case Stmt::Kind::block: analyze_statements(result, st.body, true); break;
        case Stmt::Kind::if_stmt:
            if (st.condition) infer_expression(result, *st.condition);
            analyze_statements(result, st.body, true); analyze_statements(result, st.else_body, true); break;
        case Stmt::Kind::while_stmt:
            if (st.condition) infer_expression(result, *st.condition);
            analyze_statements(result, st.body, true); break;
        case Stmt::Kind::for_stmt:
            push_scope(); if (st.initializer) analyze_statement(result, *st.initializer); if (st.condition) infer_expression(result,*st.condition); if(st.increment) infer_expression(result,*st.increment); analyze_statements(result, st.body, false); pop_scope(); break;
        case Stmt::Kind::range_for:
            if (st.value) infer_expression(result, *st.value);
            push_scope(); declare(result, Symbol{st.name, SymbolNamespace::value, st.span, false, "opaque"}); analyze_statements(result, st.body, false); pop_scope(); break;
        case Stmt::Kind::expression: if (st.value) infer_expression(result, *st.value); break;
        case Stmt::Kind::return_stmt: if (st.value) infer_expression(result, *st.value); break;
        default: break;
    }
}

bool SemanticAnalyzer::resolve_alias(SemanticResult& result, const std::string& name, std::unordered_set<std::string>& visiting) {
    auto it = aliases_.find(name); if (it == aliases_.end()) return builtin_type(name).valid();
    if (visiting.contains(name)) { result.diagnostics.push_back(Diagnostic{{}, "cyclic type alias involving '" + name + "'"}); return false; }
    visiting.insert(name); const std::string target = it->second;
    bool ok = builtin_type(target).valid() || aliases_.contains(target);
    if (aliases_.contains(target)) ok = resolve_alias(result, target, visiting);
    if (!ok && (target.find('<') != std::string::npos || target.find('[') != std::string::npos || target.rfind("function",0)==0)) ok = true;
    if (!ok) result.diagnostics.push_back(Diagnostic{{}, "unknown type '" + target + "' in alias '" + name + "'"});
    visiting.erase(name); return ok;
}

SemanticResult SemanticAnalyzer::analyze(const Program& program) {
    SemanticResult result; scopes_.clear(); aliases_.clear(); struct_fields_.clear();
    aliases_["int"]="int_32"; aliases_["uint"]="uint_32"; aliases_["double"]="double_32";
    push_scope();
    for (const auto& [name,target] : aliases_) { (void)target; declare(result, Symbol{name,SymbolNamespace::type,{},true,{}}); }
    analyze_statements(result, program.statements, false);
    for (const auto& [name,target] : aliases_) { (void)target; std::unordered_set<std::string> visiting; resolve_alias(result,name,visiting); }
    pop_scope(); return result;
}

} // namespace strut
