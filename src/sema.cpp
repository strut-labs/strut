#include "strut/sema.h"
#include "strut/type.h"

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
    map.emplace(symbol.name, std::move(symbol)); return true;
}
void SemanticAnalyzer::analyze_statements(SemanticResult& result, const std::vector<StmtPtr>& statements, bool create_scope) {
    if (create_scope) push_scope();
    for (const auto& statement : statements) analyze_statement(result, *statement);
    if (create_scope) pop_scope();
}
void SemanticAnalyzer::analyze_statement(SemanticResult& result, const Stmt& st) {
    switch (st.kind) {
        case Stmt::Kind::declaration: {
            declare(result, Symbol{st.name, SymbolNamespace::value, st.span, st.is_const});
            if (st.declared_type && st.value) {
                const auto destination = builtin_type(st.declared_type->name);
                if (destination.numeric() && st.value->kind == Expr::Kind::integer_literal && !integer_literal_fits(st.value->text, destination)) {
                    result.diagnostics.push_back(Diagnostic{st.value->span, "integer literal does not fit " + st.declared_type->name});
                }
                if (destination.kind == TypeKind::signed_int || destination.kind == TypeKind::unsigned_int) {
                    if (st.value->kind == Expr::Kind::floating_literal) result.diagnostics.push_back(Diagnostic{st.value->span, "narrowing floating-to-integer initialization requires an explicit conversion"});
                }
            }
            break;
        }
        case Stmt::Kind::type_alias: {
            declare(result, Symbol{st.name, SymbolNamespace::type, st.span, true});
            if (st.alias_target) aliases_[st.name] = st.alias_target->name;
            break;
        }
        case Stmt::Kind::function_decl: {
            declare(result, Symbol{st.name, SymbolNamespace::function, st.span, true});
            if (st.has_body) {
                push_scope();
                for (const auto& p : st.parameters) declare(result, Symbol{p.name, SymbolNamespace::value, p.span, false});
                analyze_statements(result, st.body, false);
                pop_scope();
            }
            break;
        }
        case Stmt::Kind::block: analyze_statements(result, st.body, true); break;
        case Stmt::Kind::if_stmt:
            analyze_statements(result, st.body, true); analyze_statements(result, st.else_body, true); break;
        case Stmt::Kind::while_stmt: analyze_statements(result, st.body, true); break;
        case Stmt::Kind::for_stmt:
            push_scope(); if (st.initializer) analyze_statement(result, *st.initializer); analyze_statements(result, st.body, false); pop_scope(); break;
        case Stmt::Kind::range_for:
            push_scope(); declare(result, Symbol{st.name, SymbolNamespace::value, st.span, false}); analyze_statements(result, st.body, false); pop_scope(); break;
        default: break;
    }
}
bool SemanticAnalyzer::resolve_alias(SemanticResult& result, const std::string& name, std::unordered_set<std::string>& visiting) {
    auto it = aliases_.find(name); if (it == aliases_.end()) return builtin_type(name).valid();
    if (visiting.contains(name)) { result.diagnostics.push_back(Diagnostic{{}, "cyclic type alias involving '" + name + "'"}); return false; }
    visiting.insert(name);
    const std::string target = it->second;
    bool ok = builtin_type(target).valid() || aliases_.contains(target);
    if (aliases_.contains(target)) ok = resolve_alias(result, target, visiting);
    // Compound/function aliases are syntactically valid here; their component validation arrives with richer types.
    if (!ok && (target.find('<') != std::string::npos || target.find('[') != std::string::npos || target.rfind("function",0)==0)) ok = true;
    if (!ok) result.diagnostics.push_back(Diagnostic{{}, "unknown type '" + target + "' in alias '" + name + "'"});
    visiting.erase(name); return ok;
}
SemanticResult SemanticAnalyzer::analyze(const Program& program) {
    SemanticResult result; scopes_.clear(); aliases_.clear();
    aliases_["int"]="int_32"; aliases_["uint"]="uint_32"; aliases_["double"]="double_32";
    push_scope();
    for (const auto& [name,target] : aliases_) declare(result, Symbol{name,SymbolNamespace::type,{},true});
    analyze_statements(result, program.statements, false);
    for (const auto& [name,target] : aliases_) { (void)target; std::unordered_set<std::string> visiting; resolve_alias(result,name,visiting); }
    pop_scope(); return result;
}

} // namespace strut
