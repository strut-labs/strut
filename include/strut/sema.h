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
    TypeId type_id;
    std::string source_owner;
    Symbol() = default;
    Symbol(std::string symbol_name, SymbolNamespace ns, SourceSpan source_span, bool constant, std::string spelling)
        : name(std::move(symbol_name)), name_space(ns), span(source_span), is_const(constant),
          type_name(std::move(spelling)), type_id(intern_type(type_name)) {}
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
    TypeInfo infer_expression(SemanticResult& result, const Expr& expression, TypeId expected = {});
    TypeInfo resolve_type(std::string_view name) const;
    std::string resolved_type_name(std::string_view name) const;
    bool compatible(const TypeInfo& from, const TypeInfo& to) const;
    static std::unordered_map<std::string, Symbol>& namespace_map(Scope& scope, SymbolNamespace name_space);
    bool resolve_alias(SemanticResult& result, const std::string& name, std::unordered_set<std::string>& visiting);
    bool can_access(std::string_view owner) const;
    bool can_access(std::string_view owner, std::string_view symbol) const;
    void require_type_module(SemanticResult& result, std::string_view type_name, SourceSpan span) const;
    void require_module(SemanticResult& result, std::string_view module, SourceSpan span, std::string_view facility) const;

    std::unordered_map<std::string, std::string> aliases_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> struct_fields_;
    std::unordered_map<std::string, std::unordered_set<std::string>> abstract_methods_;
    std::unordered_map<std::string, std::vector<std::string>> struct_bases_;
    std::unordered_map<std::string, std::unordered_set<std::string>> enum_members_;
    std::unordered_set<std::string> named_types_;
    std::unordered_map<std::string, std::string> type_owners_;
    std::unordered_set<std::string> checked_error_types_;
    std::vector<Scope> scopes_;
    std::string current_function_return_type_;
    std::unordered_set<std::string> current_function_errors_;
    std::unordered_map<std::string, std::unordered_set<std::string>> function_errors_;
    std::unordered_map<std::string, std::vector<const Stmt*>> function_candidates_;
    std::unordered_map<std::string, std::unordered_set<std::string>> operator_signatures_;
    std::unordered_map<std::string, std::string> operator_returns_;
    std::unordered_set<std::string> extern_c_functions_;
    int unsafe_depth_ = 0;
    int catch_all_depth_ = 0;
    bool enforce_standard_modules_ = false;
    std::unordered_set<std::string> standard_modules_;
    std::unordered_map<std::string, std::unordered_set<std::string>> owner_package_imports_;
    std::unordered_map<std::string, std::unordered_set<std::string>> owner_private_symbols_;
    std::unordered_map<std::string, std::unordered_set<std::string>> owner_internal_symbols_;
    std::string current_source_owner_;
};

} // namespace strut
