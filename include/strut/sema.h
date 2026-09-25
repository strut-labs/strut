#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "strut/ast.h"
#include "strut/lexer.h"
#include "strut/type.h"

namespace strut {

enum class SymbolNamespace { value, function, type };

struct Symbol {
    std::string name;
    SymbolNamespace name_space = SymbolNamespace::value;
    SourceSpan span;
    bool is_const = false;
    std::string type_name;
};

struct SemanticResult {
    std::vector<Diagnostic> diagnostics;
    std::vector<Diagnostic> warnings;
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
    Symbol* lookup(std::string_view name, SymbolNamespace name_space);
    void analyze_statements(SemanticResult& result, const std::vector<StmtPtr>& statements, bool create_scope);
    void analyze_statement(SemanticResult& result, const Stmt& statement);
    TypeInfo infer_expression(SemanticResult& result, const Expr& expression);
    TypeInfo resolve_type(std::string_view name) const;
    std::string resolved_type_name(std::string_view name) const;
    bool compatible(const TypeInfo& from, const TypeInfo& to) const;
    static std::unordered_map<std::string, Symbol>& namespace_map(Scope& scope, SymbolNamespace name_space);
    bool resolve_alias(SemanticResult& result, const std::string& name, std::unordered_set<std::string>& visiting);

    std::unordered_map<std::string, std::string> aliases_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> struct_fields_;
    std::vector<Scope> scopes_;
    std::string current_function_return_type_;
    int unsafe_depth_ = 0;
};

} // namespace strut
