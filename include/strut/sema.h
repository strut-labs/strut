#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <unordered_set>

#include "strut/ast.h"
#include "strut/lexer.h"

namespace strut {

enum class SymbolNamespace { value, function, type };

struct Symbol {
    std::string name;
    SymbolNamespace name_space = SymbolNamespace::value;
    SourceSpan span;
    bool is_const = false;
};

struct SemanticResult {
    std::vector<Diagnostic> diagnostics;
    bool ok() const { return diagnostics.empty(); }
};

class SemanticAnalyzer {
public:
    SemanticResult analyze(const Program& program);

private:
    struct Scope {
        std::unordered_map<std::string, Symbol> values;
        std::unordered_map<std::string, Symbol> functions;
        std::unordered_map<std::string, Symbol> types;
    };

    void push_scope();
    void pop_scope();
    bool declare(SemanticResult& result, Symbol symbol);
    void analyze_statements(SemanticResult& result, const std::vector<StmtPtr>& statements, bool create_scope);
    void analyze_statement(SemanticResult& result, const Stmt& statement);
    static std::unordered_map<std::string, Symbol>& namespace_map(Scope& scope, SymbolNamespace name_space);

    bool resolve_alias(SemanticResult& result, const std::string& name, std::unordered_set<std::string>& visiting);
    std::unordered_map<std::string, std::string> aliases_;
    std::vector<Scope> scopes_;
};

} // namespace strut
