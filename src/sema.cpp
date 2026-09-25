#include "strut/sema.h"

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
        case Stmt::Kind::declaration:
            declare(result, Symbol{st.name, SymbolNamespace::value, st.span, st.is_const}); break;
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
SemanticResult SemanticAnalyzer::analyze(const Program& program) {
    SemanticResult result; scopes_.clear(); push_scope(); analyze_statements(result, program.statements, false); pop_scope(); return result;
}

} // namespace strut
