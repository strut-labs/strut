#include "strut/sema.h"
#include "strut/api_registry.h"

#include <algorithm>
#include <cctype>
#include <functional>

namespace strut {
namespace {
std::string function_signature(const Stmt& st){std::string sig="function<(";for(std::size_t i=0;i<st.parameters.size();++i){if(i)sig+=",";sig+=st.parameters[i].type.name;}sig+=")->"+(st.is_async?("future<"+(st.return_type?st.return_type->name:std::string("void"))+">"):(st.return_type?st.return_type->name:std::string("void")))+">";return sig;}
std::string function_return(std::string_view sig){const auto& args=type_arguments(intern_type(sig));return type_is(intern_type(sig),TypeNodeKind::function)&&!args.empty()?type_spelling(args.back()):"opaque";}
std::string generic_inner(std::string_view type,std::string_view head){const auto& node=type_node(intern_type(type));const auto expected=head.empty()?std::string_view{}:head.substr(0,head.size()-1);const bool match=(node.kind==TypeNodeKind::generic&&node.name==expected)||(expected=="ref"&&node.kind==TypeNodeKind::reference)||(expected=="ptr"&&node.kind==TypeNodeKind::safe_pointer)||(expected=="raw_ptr"&&node.kind==TypeNodeKind::raw_pointer)||(expected=="weak_ptr"&&node.kind==TypeNodeKind::weak_pointer);return match&&!node.children.empty()?type_spelling(node.children.front()):std::string{};}
TypeId child_of(const TypeInfo& type,TypeNodeKind kind){const auto id=type.id?type.id:intern_type(type.name);return type_is(id,kind)?type_element(id):TypeId{};}
std::string normalize_operator_type(std::string t){if(t.rfind("ref<",0)==0&&t.back()=='>')t=t.substr(4,t.size()-5);if(t.rfind("const ",0)==0)t=t.substr(6);return t;}
std::string operator_key(OperatorFixity fixity,std::string_view spelling){return std::string(operator_fixity_name(fixity))+":"+std::string(spelling);}
bool statement_returns(const Stmt& st);
bool block_returns(const std::vector<StmtPtr>& body){for(const auto& st:body)if(statement_returns(*st))return true;return false;}
bool statement_returns(const Stmt& st){
    if(st.kind==Stmt::Kind::return_stmt||st.kind==Stmt::Kind::throw_stmt)return true;
    if(st.kind==Stmt::Kind::block||st.kind==Stmt::Kind::unsafe_stmt)return block_returns(st.body);
    if(st.kind==Stmt::Kind::if_stmt)return !st.else_body.empty()&&block_returns(st.body)&&block_returns(st.else_body);
    if(st.kind==Stmt::Kind::match_stmt||st.kind==Stmt::Kind::switch_stmt){bool fallback=false;if(st.switch_cases.empty())return false;for(const auto& c:st.switch_cases){fallback|=c.is_default;if(!block_returns(c.body))return false;}return fallback;}
    if(st.kind==Stmt::Kind::try_stmt){if(!block_returns(st.body)||st.catches.empty())return false;for(const auto& c:st.catches)if(!block_returns(c.body))return false;return true;}
    return false;
}
std::string iterable_element(std::string type){
    const auto id=intern_type(type);const auto& node=type_node(id);
    if((node.kind==TypeNodeKind::vector||node.kind==TypeNodeKind::fixed_array)&&!node.children.empty())return type_spelling(node.children.front());
    if(node.kind==TypeNodeKind::generic&&(node.name=="list"||node.name=="deque"||node.name=="set"||node.name=="ordered_set")&&!node.children.empty())return type_spelling(node.children.front());
    return {};
}
bool unordered_iterable(std::string_view type){const auto& node=type_node(intern_type(type));return node.kind==TypeNodeKind::generic&&node.name=="set";}
using TypeBindings=std::unordered_map<std::string,TypeId>;
bool generic_name(TypeId id,const std::unordered_set<std::string>& parameters){const auto& n=type_node(id);return n.kind==TypeNodeKind::named&&parameters.find(n.name)!=parameters.end();}
bool unify_type(TypeId pattern,TypeId actual,const std::unordered_set<std::string>& parameters,TypeBindings& bindings){
    if(!pattern||!actual)return false;
    if(generic_name(pattern,parameters)){const auto name=type_node(pattern).name;auto [it,inserted]=bindings.emplace(name,actual);return inserted||it->second==actual;}
    const auto& p=type_node(pattern);const auto& a=type_node(actual);
    if(p.kind!=a.kind||p.name!=a.name||p.extent!=a.extent||p.children.size()!=a.children.size())return false;
    for(std::size_t i=0;i<p.children.size();++i)if(!unify_type(p.children[i],a.children[i],parameters,bindings))return false;
    return true;
}
TypeId substitute_type(TypeId pattern,const std::unordered_set<std::string>& parameters,const TypeBindings& bindings){
    if(generic_name(pattern,parameters)){auto it=bindings.find(type_node(pattern).name);return it==bindings.end()?pattern:it->second;}
    const auto& n=type_node(pattern);if(n.children.empty())return pattern;
    std::vector<TypeId> children;children.reserve(n.children.size());bool changed=false;for(auto child:n.children){auto replacement=substitute_type(child,parameters,bindings);children.push_back(replacement);changed|=replacement!=child;}if(!changed)return pattern;
    std::string spelling;
    if(n.kind==TypeNodeKind::const_type)spelling="const "+type_spelling(children[0]);
    else if(n.kind==TypeNodeKind::reference)spelling="ref<"+type_spelling(children[0])+">";
    else if(n.kind==TypeNodeKind::safe_pointer)spelling="ptr<"+type_spelling(children[0])+">";
    else if(n.kind==TypeNodeKind::raw_pointer)spelling="raw_ptr<"+type_spelling(children[0])+">";
    else if(n.kind==TypeNodeKind::weak_pointer)spelling="weak_ptr<"+type_spelling(children[0])+">";
    else if(n.kind==TypeNodeKind::nullable)spelling=type_spelling(children[0])+"?";
    else if(n.kind==TypeNodeKind::vector)spelling=type_spelling(children[0])+"[]";
    else if(n.kind==TypeNodeKind::fixed_array)spelling=type_spelling(children[0])+"["+std::to_string(n.extent)+"]";
    else if(n.kind==TypeNodeKind::function){spelling=n.name+"<(";for(std::size_t i=0;i+1<children.size();++i){if(i)spelling+=",";spelling+=type_spelling(children[i]);}spelling+=")->"+type_spelling(children.back())+">";}
    else {spelling=n.kind==TypeNodeKind::tuple?"tuple<":n.name+"<";for(std::size_t i=0;i<children.size();++i){if(i)spelling+=",";spelling+=type_spelling(children[i]);}spelling+=">";}
    return intern_type(spelling);
}
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
    if (is_nullable_type(resolved)) return {TypeKind::named, 0, resolved,intern_type(resolved)};
    auto type = builtin_type(resolved);
    if (type.valid()) return type;
    if (resolved == "null") return {TypeKind::null_type, 0, "null",intern_type("null")};
    if (resolved == "opaque") return {TypeKind::named, 0, "opaque",intern_type("opaque")};
    if (named_types_.find(resolved) != named_types_.end()) return {TypeKind::named,0,resolved,intern_type(resolved)};
    if (resolved.find('<') != std::string::npos || resolved.find('[') != std::string::npos ||
        (!resolved.empty() && std::all_of(resolved.begin(), resolved.end(), [](unsigned char c){ return !std::islower(c); }))) {
        return {TypeKind::named, 0, resolved,intern_type(resolved)};
    }
    return {};
}
bool SemanticAnalyzer::compatible(const TypeInfo& from, const TypeInfo& to) const {
    if (!from.valid() || !to.valid()) return true; // later phases refine currently opaque compound/user types
    if (from.kind == TypeKind::named && from.name == "opaque") return true;
    const auto to_id=to.id?to.id:intern_type(to.name),from_id=from.id?from.id:intern_type(from.name);
    const auto to_kind=type_node(to_id).kind,from_kind=type_node(from_id).kind;
    if (from.kind == TypeKind::null_type) return to.kind == TypeKind::null_type || to_kind==TypeNodeKind::nullable || to_kind==TypeNodeKind::safe_pointer || to_kind==TypeNodeKind::weak_pointer || to_kind==TypeNodeKind::raw_pointer;
    if(to_kind==TypeNodeKind::nullable&&from_kind!=TypeNodeKind::nullable){auto inner=type_element(to_id);return from_id==inner||compatible(from,resolve_type(type_spelling(inner)));}
    if(from_kind==TypeNodeKind::nullable&&to_kind==TypeNodeKind::nullable)return type_element(from_id)==type_element(to_id);
    if(from_id&&to_id&&from_id==to_id)return true;
    if (from.kind == to.kind && from.bits == to.bits) return true;
    if (from.kind == TypeKind::string_type && to.kind == TypeKind::string_type) return true;
    if (from.kind == TypeKind::bool_type && to.kind == TypeKind::bool_type) return true;
    return can_implicitly_convert(from, to);
}

void SemanticAnalyzer::require_module(SemanticResult& result, std::string_view module, SourceSpan span, std::string_view facility) const {
    if (!enforce_standard_modules_ || standard_modules_.find(std::string(module)) != standard_modules_.end()) return;
    result.diagnostics.push_back(Diagnostic{span, "'" + std::string(facility) + "' requires standard module <" + std::string(module) + ">\nhelp: add `include <" + std::string(module) + ">;`"});
}

void SemanticAnalyzer::require_type_module(SemanticResult& result, std::string_view type_name, SourceSpan span) const {
    auto key_supported=[&](TypeId key){if(type_is(key,TypeNodeKind::const_type))key=type_element(key);const auto& n=type_node(key);auto b=builtin_type(type_spelling(key));if(b.valid()&&b.kind!=TypeKind::void_type&&b.kind!=TypeKind::json_type)return true;if(n.kind==TypeNodeKind::named&&enum_members_.find(n.name)!=enum_members_.end())return true;return n.kind==TypeNodeKind::safe_pointer||n.kind==TypeNodeKind::raw_pointer;};
    std::function<void(TypeId)> visit=[&](TypeId id){const auto& n=type_node(id);if(n.kind==TypeNodeKind::tuple)require_module(result,"tuple",span,type_name);if(n.kind==TypeNodeKind::generic){const auto& head=n.name;if(head=="prique")result.diagnostics.push_back(Diagnostic{span,"'prique' was renamed to 'priority_queue'; use priority_queue<T> or priority_queue<T,min>"});if(head=="map"||head=="ordered_map"||head=="set"||head=="ordered_set"||head=="queue"||head=="stack"||head=="deque"||head=="list"||head=="priority_queue")require_module(result,head=="prique"?"priority_queue":head,span,type_name);if(head=="atomic"&&(n.children.size()!=1||!(builtin_type(type_spelling(n.children.front())).kind==TypeKind::signed_int||builtin_type(type_spelling(n.children.front())).kind==TypeKind::unsigned_int||builtin_type(type_spelling(n.children.front())).kind==TypeKind::bool_type)))result.diagnostics.push_back(Diagnostic{span,"atomic<T> requires one integer or bool scalar type"});if(!n.children.empty()&&(head=="map"||head=="set"||head=="ordered_map"||head=="ordered_set")&&!key_supported(n.children.front())){const auto key=type_spelling(n.children.front());if(head=="map")result.diagnostics.push_back(Diagnostic{span,"map key type '"+key+"' is not hashable; use a built-in/hashable key or ordered_map"});else if(head=="set")result.diagnostics.push_back(Diagnostic{span,"set element type '"+key+"' is not hashable; use a built-in/hashable element or ordered_set"});else result.diagnostics.push_back(Diagnostic{span,head+" key/element type '"+key+"' is not orderable by the standard library"});}}for(auto child:n.children)visit(child);};
    visit(intern_type(type_name));
}

TypeInfo SemanticAnalyzer::infer_expression(SemanticResult& result, const Expr& expr, TypeId expected) {
    switch (expr.kind) {
        case Expr::Kind::integer_literal: return infer_integer_literal(expr.text);
        case Expr::Kind::floating_literal: return infer_floating_literal(expr.text);
        case Expr::Kind::string_literal: return {TypeKind::string_type, 0, "string"};
        case Expr::Kind::boolean_literal: return {TypeKind::bool_type, 0, "bool"};
        case Expr::Kind::null_literal: return {TypeKind::null_type, 0, "null"};
        case Expr::Kind::array_literal: {
            const bool byte_literal=type_spelling(expected)=="bytes";
            TypeId element;const auto& expected_node=type_node(expected);if(expected_node.kind==TypeNodeKind::vector||expected_node.kind==TypeNodeKind::fixed_array)element=type_element(expected);else if(byte_literal)element=intern_type("uint_8");
            if (expr.arguments.empty()) {if(element)expr.inferred_type=expected;return element?resolve_type(type_spelling(expected)) : TypeInfo{TypeKind::named,0,"opaque[]"};}
            if(byte_literal){const auto byte_type=builtin_type("uint_8");for(const auto& argument:expr.arguments){auto value=infer_expression(result,*argument,element);const bool literal_ok=argument->kind==Expr::Kind::integer_literal&&integer_literal_fits(argument->text,byte_type);if(!literal_ok&&!compatible(value,byte_type))result.diagnostics.push_back(Diagnostic{argument->span,"byte literal element must fit uint_8"});}expr.inferred_type=expected;return {TypeKind::named,0,"bytes",expected};}
            auto first=infer_expression(result,*expr.arguments.front(),element);
            for(std::size_t i=1;i<expr.arguments.size();++i){auto next=infer_expression(result,*expr.arguments[i],element);if(first.valid()&&next.valid()&&!compatible(next,first))result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"array literal element type mismatch at index "+std::to_string(i)+": expected "+first.name+", found "+next.name+"\nhelp: use one compatible element type or declare and populate separate typed arrays"});}
            TypeInfo inferred{TypeKind::named,0,(first.name.empty()?std::string("opaque"):first.name)+"[]"};expr.inferred_type=inferred.id;return inferred;
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
            if(expr.text=="await"){const auto& node=type_node(operand.id);auto inner=node.kind==TypeNodeKind::generic&&node.name=="future"&&!node.children.empty()?node.children.front():TypeId{};if(!inner)result.diagnostics.push_back(Diagnostic{expr.span,"await requires a future<T>"});return resolve_type(inner?type_spelling(inner):"opaque");}
            if(expr.text=="*"){const auto kind=type_node(operand.id).kind;if(kind==TypeNodeKind::raw_pointer&&unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"ptr<T> dereference requires unsafe block"});if(kind==TypeNodeKind::safe_pointer||kind==TypeNodeKind::raw_pointer||kind==TypeNodeKind::reference)return resolve_type(type_spelling(type_element(operand.id)));}
            const auto fixity=expr.kind==Expr::Kind::postfix?OperatorFixity::postfix:OperatorFixity::prefix;
            auto oit=operator_returns_.find(operator_key(fixity,expr.text)+"|"+normalize_operator_type(operand.name));if(oit!=operator_returns_.end())return resolve_type(oit->second);
            if((expr.text=="++"||expr.text=="--")&&!operand.numeric()&&operand.name!="opaque")result.diagnostics.push_back(Diagnostic{expr.span,"no "+std::string(operator_fixity_name(fixity))+" "+expr.text+" overload exists for '"+operand.name+"'"});
            return operand;
        }
        case Expr::Kind::binary: {
            auto left = infer_expression(result, *expr.left);
            auto right = infer_expression(result, *expr.right);
            if (expr.text == "??") {
                if (!type_is(left.id,TypeNodeKind::nullable)) result.diagnostics.push_back(Diagnostic{expr.left->span, "left operand of ?? must be nullable"});
                auto inner = resolve_type(type_spelling(type_element(left.id)));
                if (right.valid() && inner.valid() && !compatible(right, inner)) result.diagnostics.push_back(Diagnostic{expr.right->span, "fallback value is incompatible with nullable type"});
                return inner;
            }
            if (expr.text == "==" || expr.text == "!=" || expr.text == "<" || expr.text == "<=" || expr.text == ">" || expr.text == ">=" || expr.text == "&&" || expr.text == "||") {
                if((left.name=="bytes"||right.name=="bytes")&&left.name!=right.name)result.diagnostics.push_back(Diagnostic{expr.span,"bytes equality requires two bytes values"});
                return {TypeKind::bool_type, 0, "bool"};
            }
            if (expr.text == "<<" || expr.text == ">>") return left;
            if((expr.text=="+"||expr.text=="-") && type_is(left.id,TypeNodeKind::raw_pointer)){if(unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"raw pointer arithmetic requires unsafe block"});return left;}
            if (left.numeric() && right.numeric()) {
                if (left.kind == TypeKind::floating || right.kind == TypeKind::floating) return builtin_type((left.bits > 32 || right.bits > 32) ? "double_64" : "double_32");
                if (left.kind == right.kind) return left.bits >= right.bits ? left : right;
                return left.bits > right.bits ? left : right;
            }
            auto oit=operator_returns_.find("infix:"+expr.text+"|"+left.name+","+right.name);if(oit!=operator_returns_.end())return resolve_type(oit->second);
            return {};
        }
        case Expr::Kind::call: {
            const Stmt* selected=nullptr;TypeBindings selected_bindings;std::unordered_set<std::string> selected_generics;
            if(expr.left&&expr.left->kind==Expr::Kind::identifier){auto found=function_candidates_.find(expr.left->text);if(found!=function_candidates_.end()){
                std::vector<std::tuple<const Stmt*,TypeBindings,std::unordered_set<std::string>,int>> viable;
                for(const auto* candidate:found->second){if(candidate->parameters.size()!=expr.arguments.size())continue;std::unordered_set<std::string> generics(candidate->generic_parameters.begin(),candidate->generic_parameters.end());TypeBindings bindings;int rank=generics.empty()?0:1;bool ok=true;
                    if(expected&&candidate->return_type&&type_node(expected).kind!=TypeNodeKind::nullable&&!unify_type(candidate->return_type->type_id,expected,generics,bindings))ok=false;
                    for(std::size_t i=0;ok&&i<expr.arguments.size();++i){auto parameter=substitute_type(candidate->parameters[i].type.type_id,generics,bindings);auto argument=infer_expression(result,*expr.arguments[i],parameter);if(argument.name=="opaque[]")continue;if(!unify_type(candidate->parameters[i].type.type_id,argument.id,generics,bindings)){auto resolved=resolve_type(type_spelling(parameter));if(!compatible(argument,resolved))ok=false;else ++rank;}}
                    for(const auto& generic:generics)if(bindings.find(generic)==bindings.end())ok=false;
                    if(ok)viable.emplace_back(candidate,std::move(bindings),std::move(generics),rank);
                }
                if(!viable.empty()){std::sort(viable.begin(),viable.end(),[](const auto& a,const auto& b){return std::get<3>(a)<std::get<3>(b);});if(viable.size()>1&&std::get<3>(viable[0])==std::get<3>(viable[1])){std::string message="ambiguous call to '"+expr.left->text+"'; viable candidates:";for(const auto& item:viable)message+="\n  "+function_signature(*std::get<0>(item));result.diagnostics.push_back(Diagnostic{expr.span,std::move(message)});}selected=std::get<0>(viable[0]);selected_bindings=std::get<1>(viable[0]);selected_generics=std::get<2>(viable[0]);}
            }}
            if(expr.left&&expr.left->kind==Expr::Kind::member&&expr.left->left&&expr.left->left->kind==Expr::Kind::identifier&&expr.left->left->text=="json"){
                if(expr.left->text=="parse"||expr.left->text=="encode")return builtin_type("json");
                if(expr.left->text=="stringify"||expr.left->text=="pretty")return builtin_type("string");
            }
            const ApiCallable* builtin=nullptr;std::string builtin_owner;TypeBindings builtin_bindings;
            if(expr.left&&expr.left->kind==Expr::Kind::identifier)builtin=api_callable(expr.left->text);
            if(expr.left&&expr.left->kind==Expr::Kind::member&&expr.left->left){TypeInfo base;if(expr.left->left->kind==Expr::Kind::identifier&&expr.left->left->text=="bytes")base={TypeKind::named,0,"bytes",intern_type("bytes")};else base=infer_expression(result,*expr.left->left);builtin_owner=base.name;builtin=api_callable(expr.left->text,builtin_owner);if(builtin){const auto& schema=type_node(intern_type(builtin->owner));const auto& actual=type_node(base.id);if(schema.kind==TypeNodeKind::generic&&actual.kind==TypeNodeKind::generic&&!schema.children.empty()&&!actual.children.empty())builtin_bindings[type_node(schema.children.front()).name]=actual.children.front();}}
            std::vector<TypeInfo> argument_types;argument_types.reserve(expr.arguments.size());
            for (std::size_t i=0;i<expr.arguments.size();++i){TypeId argument_expected;if(selected)argument_expected=substitute_type(selected->parameters[i].type.type_id,selected_generics,selected_bindings);argument_types.push_back(infer_expression(result,*expr.arguments[i],argument_expected));}
            for(std::size_t i=0;i<argument_types.size();++i)if(argument_types[i].name=="opaque[]"){
                bool bytes_context=false;if(builtin)for(const auto& candidate:builtin->overloads){std::size_t required=0;for(const auto& parameter:candidate.parameters)if(!parameter.optional)++required;if(expr.arguments.size()>=required&&expr.arguments.size()<=candidate.parameters.size()&&i<candidate.parameters.size()&&type_spelling(substitute_type(candidate.parameters[i].type,{"T"},builtin_bindings))=="bytes"){bytes_context=true;break;}}
                if(bytes_context)argument_types[i]=infer_expression(result,*expr.arguments[i],intern_type("bytes"));
                if(!bytes_context)result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"cannot infer element type of empty array literal\nhelp: add an explicit type, for example `string[] values := []`, before passing it"});
            }
            if(expr.left && expr.left->kind==Expr::Kind::identifier){if(extern_c_functions_.find(expr.left->text)!=extern_c_functions_.end() && unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"extern C call requires unsafe block"});auto fit=function_errors_.find(expr.left->text);if(fit!=function_errors_.end())for(const auto& e:fit->second)if(current_function_errors_.find(e)==current_function_errors_.end() && catch_all_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"call to '"+expr.left->text+"' may throw checked error "+e+" not declared by current function\nhelp: handle "+e+" with `try`/`catch`, or add it after `:` in the enclosing function signature"});}
            if(builtin_owner=="atomic<bool>"&&expr.left&&(expr.left->text=="fetch_add"||expr.left->text=="fetch_sub"))result.diagnostics.push_back(Diagnostic{expr.span,expr.left->text+" is only available on integer atomic values"});
            if(builtin&&!builtin->owner.empty()&&builtin->generic_parameters.empty()){
                const ApiOverload* signature=nullptr;for(const auto& candidate:builtin->overloads){std::size_t required=0;for(const auto& p:candidate.parameters)if(!p.optional)++required;if(expr.arguments.size()>=required&&expr.arguments.size()<=candidate.parameters.size()){signature=&candidate;break;}}
                const auto leaf=builtin->owner.empty()?builtin->name:builtin->name.substr(builtin->name.find('.')+1);
                if(!signature){std::size_t least=builtin->overloads.front().parameters.size(),most=0;for(const auto& candidate:builtin->overloads){std::size_t required=0;for(const auto& p:candidate.parameters)if(!p.optional)++required;least=std::min(least,required);most=std::max(most,candidate.parameters.size());}result.diagnostics.push_back(Diagnostic{expr.span,"call to '"+leaf+"' expects "+(least==most?std::to_string(least):std::to_string(least)+" to "+std::to_string(most))+" argument(s), found "+std::to_string(expr.arguments.size())});}
                else for(std::size_t i=0;i<argument_types.size();++i){auto expected_type=substitute_type(signature->parameters[i].type,{"T"},builtin_bindings);const auto expected_name=type_spelling(expected_type);if(expected_name=="bytes"&&expr.arguments[i]->kind==Expr::Kind::array_literal)argument_types[i]=infer_expression(result,*expr.arguments[i],expected_type);if(type_is(expected_type,TypeNodeKind::function)&&(argument_types[i].name=="function"||argument_types[i].name=="async_function"))continue;auto destination=resolve_type(expected_name);if(argument_types[i].valid()&&destination.valid()&&!compatible(argument_types[i],destination))result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"argument "+std::to_string(i+1)+" to '"+leaf+"' expects "+expected_name+", found "+argument_types[i].name});}
                if(!builtin->owner.empty())for(const auto& error:builtin->checked_errors)if(current_function_errors_.find(error)==current_function_errors_.end()&&catch_all_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"call to '"+leaf+"' may throw checked error "+error+" not declared by current function\nhelp: handle "+error+" with `try`/`catch`, or add it after `:` in the enclosing function signature"});
                if(signature)return resolve_type(type_spelling(substitute_type(signature->return_type,{"T"},builtin_bindings)));
            }
            if(expr.left && expr.left->kind==Expr::Kind::member && expr.left->left){auto base=infer_expression(result,*expr.left->left);const auto& m=expr.left->text;
                if((base.name=="cancellation_source"||base.name=="cancellation_token")&&!builtin)result.diagnostics.push_back(Diagnostic{expr.span,"unknown "+base.name+" method '"+m+"'"});
                if(base.name.rfind("atomic<",0)==0){auto elem=generic_inner(base.name,"atomic<");if((m=="fetch_add"||m=="fetch_sub")&&elem=="bool")result.diagnostics.push_back(Diagnostic{expr.span,m+" is only available on integer atomic values"});if(m=="load"||m=="exchange"||m=="fetch_add"||m=="fetch_sub")return resolve_type(elem);if(m=="store")return {TypeKind::void_type,0,"void"};if(m=="compare_exchange")return builtin_type("bool");}
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
                if(name=="new"){if(expr.arguments.size()!=1){result.diagnostics.push_back(Diagnostic{expr.span,"new(...) requires exactly one argument"});return {};}auto t=infer_expression(result,*expr.arguments[0]);return {TypeKind::named,0,"ptr<"+t.name+">"};}
                if(name=="ptr"){if(unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"ptr(...) requires unsafe block"});if(expr.arguments.size()!=1){result.diagnostics.push_back(Diagnostic{expr.span,"ptr(...) requires exactly one T* safe pointer"});return {};}auto t=infer_expression(result,*expr.arguments[0]);auto inner=child_of(t,TypeNodeKind::safe_pointer);if(!inner)result.diagnostics.push_back(Diagnostic{expr.arguments[0]->span,"ptr(...) requires a T* safe pointer"});return {TypeKind::named,0,"raw_ptr<"+type_spelling(inner)+">"};}
                if(name=="raw"){result.diagnostics.push_back(Diagnostic{expr.span,"raw(...) has been replaced by ptr(...) for raw pointer conversion"});return {}; }
                if(name=="weak"){if(expr.arguments.size()!=1){result.diagnostics.push_back(Diagnostic{expr.span,"weak(...) requires exactly one T* safe pointer"});return {};}auto t=infer_expression(result,*expr.arguments[0]);auto inner=child_of(t,TypeNodeKind::safe_pointer);if(!inner)result.diagnostics.push_back(Diagnostic{expr.arguments[0]->span,"weak(...) requires a T* safe pointer"});return {TypeKind::named,0,"weak_ptr<"+type_spelling(inner)+">"};}
                if(name=="ref"){if(expr.arguments.size()!=1){result.diagnostics.push_back(Diagnostic{expr.span,"ref(...) requires exactly one argument"});return {};}const auto& a=*expr.arguments[0];const bool lvalue=a.kind==Expr::Kind::identifier||a.kind==Expr::Kind::member||a.kind==Expr::Kind::index||(a.kind==Expr::Kind::unary&&a.text=="*");if(!lvalue)result.diagnostics.push_back(Diagnostic{a.span,"ref(...) requires an lvalue with a lifetime that outlives the reference"});if(a.kind==Expr::Kind::index&&a.left){auto owner=infer_expression(result,*a.left);if(owner.name.size()>2&&owner.name.compare(owner.name.size()-2,2,"[]")==0)result.diagnostics.push_back(Diagnostic{a.span,"T& cannot borrow a dynamic-array element because later mutation could invalidate the reference"});}auto t=infer_expression(result,a);return {TypeKind::named,0,"ref<"+t.name+">"};}
                if (name == "print") return {TypeKind::void_type, 0, "void"};
                if (name == "println") return {TypeKind::void_type, 0, "void"};
                if (name == "input") return expr.arguments.empty()?builtin_type("string"):TypeInfo{TypeKind::void_type,0,"void"};
                if(name=="bytes"){if(expr.arguments.size()>1)result.diagnostics.push_back(Diagnostic{expr.span,"bytes(...) expects zero or one size argument"});if(!argument_types.empty()){const auto size_type=builtin_type("int_64");const bool literal_ok=expr.arguments.front()->kind==Expr::Kind::integer_literal&&integer_literal_fits(expr.arguments.front()->text,size_type);if(!literal_ok&&!compatible(argument_types.front(),size_type))result.diagnostics.push_back(Diagnostic{expr.arguments.front()->span,"bytes size must fit int_64"});}return {TypeKind::named,0,"bytes",intern_type("bytes")};}
                if (name == "istream" || name == "ostream" || name == "sstream" || name == "ifstream" || name == "ofstream") return {TypeKind::named,0,name};
                if (name == "exists" || name == "is_file" || name == "is_dir") { require_module(result, "filesystem", expr.span, name); return builtin_type("bool"); }
                if (name == "ls" || name == "walk") { require_module(result, "filesystem", expr.span, name); return {TypeKind::named,0,"string[]"}; }
                if (name == "file_size" || name == "modified") { require_module(result, "filesystem", expr.span, name); return builtin_type("int_64"); }
                if (name == "cwd" || name == "absolute" || name == "canonical" || name == "parent" || name == "filename" || name == "extension" || name == "stem" || name == "join_path" || name == "read_file") { require_module(result, "filesystem", expr.span, name); return builtin_type("string"); }
                if (name == "read_bytes") { require_module(result, "filesystem", expr.span, name); return {TypeKind::named,0,"bytes"}; }
                if (name == "mutex") return {TypeKind::named,0,"mutex"};
                if (name == "thread") { for(std::size_t i=1;i<expr.arguments.size();++i){auto t=infer_expression(result,*expr.arguments[i]);if(type_is(t.id,TypeNodeKind::reference))result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,"T& cannot be passed directly across a thread boundary; use T* or synchronize owned state"});} return {TypeKind::named,0,"thread"}; }
                if (name == "now_ms" || name == "unix_ms") return builtin_type("int_64");
                if (name == "make_dir" || name == "remove" || name == "remove_all" || name == "copy" || name == "move" || name == "touch" || name == "cd" || name == "write_file" || name == "append_file") {
                    require_module(result, "filesystem", expr.span, name);
                    if((name=="remove"||name=="copy"||name=="move")&&!argument_types.empty()){
                        auto check_paths=[&](std::size_t i){if(i>=argument_types.size())return;const auto& t=argument_types[i].name;if(t=="string")return;auto elem=iterable_element(t);if(elem.empty())result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,name+" expects a path string or iterable collection of strings"});else if(elem!="string")result.diagnostics.push_back(Diagnostic{expr.arguments[i]->span,name+" iterable elements must be convertible to string paths"});};
                        check_paths(0);
                        if((name=="copy"||name=="move")&&argument_types.size()>1&&argument_types[1].name!="string"){
                            check_paths(1);
                            if(unordered_iterable(argument_types[0].name)||unordered_iterable(argument_types[1].name))result.diagnostics.push_back(Diagnostic{expr.span,name+" pairwise mapping requires ordered iterable collections"});
                        }
                    }
                    return {TypeKind::void_type,0,"void"};
                }
                if (name == "set_env" || name == "unset_env" || name == "sleep_ms") return {TypeKind::void_type,0,"void"};
                if (const auto* callable=api_callable(name);callable&&!callable->overloads.empty())return resolve_type(type_spelling(callable->overloads.front().return_type));
                if(selected&&selected->return_type){auto inferred=type_spelling(substitute_type(selected->return_type->type_id,selected_generics,selected_bindings));auto resolved=resolve_type(selected->is_async?"future<"+inferred+">":inferred);expr.inferred_type=resolved.id;return resolved;}
                if (auto* fn = lookup(name, SymbolNamespace::function)) return resolve_type(function_return(fn->type_name));
                if (auto* value = lookup(name, SymbolNamespace::value)) { if(value->type_name.rfind("function<(",0)==0) return resolve_type(function_return(value->type_name)); if(value->type_name=="async_function") return {TypeKind::named,0,"future<opaque>"}; }
            }
            return {TypeKind::named, 0, "opaque"};
        }
        case Expr::Kind::lambda: {
            const auto enclosing_return=current_function_return_type_;current_function_return_type_.clear();push_scope();
            if(expr.lambda){for(const auto& p:expr.lambda->parameters)declare(result,Symbol{p.name,SymbolNamespace::value,p.span,true,p.type.name.empty()?"opaque":resolved_type_name(p.type.name)});if(expr.lambda->expression_body)infer_expression(result,*expr.lambda->expression_body);analyze_statements(result,expr.lambda->body,false);}
            pop_scope();current_function_return_type_=enclosing_return;
            return {TypeKind::named, 0, expr.lambda && expr.lambda->is_async ? "async_function" : "function"};
        }
        case Expr::Kind::member: {
            if(expr.text.rfind("::",0)==0 && expr.left && expr.left->kind==Expr::Kind::identifier){auto it=enum_members_.find(expr.left->text);std::string member=expr.text.substr(2);if(it==enum_members_.end()){result.diagnostics.push_back(Diagnostic{expr.span,"unknown enum type '"+expr.left->text+"'"});return {};}if(it->second.find(member)==it->second.end())result.diagnostics.push_back(Diagnostic{expr.span,"unknown enum member '"+member+"' for "+expr.left->text});return {TypeKind::named,0,expr.left->text};}
            auto base=infer_expression(result,*expr.left);
            if (type_is(base.id,TypeNodeKind::nullable)) { result.diagnostics.push_back(Diagnostic{expr.span,"cannot access member of nullable value without ?. or null check"}); return {}; }
            const bool arrow=expr.text.rfind("->",0)==0;
            const std::string member=arrow?expr.text.substr(2):expr.text;
            const auto base_kind=type_node(base.id).kind;
            if(arrow){const bool raw=base_kind==TypeNodeKind::raw_pointer;const bool safe=base_kind==TypeNodeKind::safe_pointer;if(!raw&&!safe)result.diagnostics.push_back(Diagnostic{expr.span,"-> member access requires T* or unsafe ptr<T>"});if(raw&&unsafe_depth_==0)result.diagnostics.push_back(Diagnostic{expr.span,"ptr<T> member access requires unsafe block"});}
            TypeId owner_id=base.id;if(base_kind==TypeNodeKind::reference||base_kind==TypeNodeKind::safe_pointer||base_kind==TypeNodeKind::raw_pointer)owner_id=type_element(owner_id);if(type_is(owner_id,TypeNodeKind::const_type))owner_id=type_element(owner_id);auto owner=type_spelling(owner_id);if(const auto* field=api_field(member,owner))return resolve_type(type_spelling(field->type));auto sit=struct_fields_.find(owner);if(sit!=struct_fields_.end()){auto f=sit->second.find(member);if(f!=sit->second.end())return resolve_type(f->second);}
            return {TypeKind::named,0,"opaque"};
        }
        case Expr::Kind::safe_member: {
            auto base=infer_expression(result,*expr.left);
            if (!type_is(base.id,TypeNodeKind::nullable)) result.diagnostics.push_back(Diagnostic{expr.span,"?. requires a nullable value"});
            auto sit=struct_fields_.find(type_spelling(type_element(base.id)));if(sit!=struct_fields_.end()){auto f=sit->second.find(expr.text);if(f!=sit->second.end()){auto t=resolve_type(f->second);return type_is(t.id,TypeNodeKind::nullable)?t:resolve_type(type_spelling(t.id)+"?");}}
            return {TypeKind::named,0,"opaque?"};
        }
        case Expr::Kind::index: {
            auto base=infer_expression(result,*expr.left);
            if(base.name=="bytes"){
                auto index=infer_expression(result,*expr.right);if(index.kind!=TypeKind::signed_int&&index.kind!=TypeKind::unsigned_int)result.diagnostics.push_back(Diagnostic{expr.right->span,"bytes index must be an integer"});
                expr.inferred_type=intern_type("uint_8");return builtin_type("uint_8");
            }
            if(type_is(base.id,TypeNodeKind::tuple)){
                if(expr.right->kind!=Expr::Kind::integer_literal){result.diagnostics.push_back(Diagnostic{expr.right->span,"tuple index must be an integer literal"});return {};}
                std::size_t idx=0;try{idx=static_cast<std::size_t>(std::stoull(expr.right->text));}catch(...){return {};}
                const auto& parts=type_arguments(base.id);
                if(idx>=parts.size()){result.diagnostics.push_back(Diagnostic{expr.right->span,"tuple index out of range"});return {};}
                return resolve_type(type_spelling(parts[idx]));
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
            const TypeId destination_context=st.declared_type?st.declared_type->type_id:TypeId{};
            TypeInfo value_type = st.value ? infer_expression(result, *st.value,destination_context) : TypeInfo{};
            std::string type_name;
            if (st.declared_type) {
                auto destination = resolve_type(st.declared_type->name);
                if (!destination.valid()) {
                    result.diagnostics.push_back(Diagnostic{st.declared_type->span, "unknown type '" + st.declared_type->name + "'"});
                }
                type_name = resolved_type_name(st.declared_type->name);
                if(type_node(destination.id).kind==TypeNodeKind::generic&&type_node(destination.id).name=="atomic"&&value_type.id==destination.id)result.diagnostics.push_back(Diagnostic{st.value?st.value->span:st.span,"atomic values cannot be copied or moved; use load() and initialize a new atomic value"});
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
                const bool atomic_init=type_node(destination.id).kind==TypeNodeKind::generic&&type_node(destination.id).name=="atomic"&&!type_arguments(destination.id).empty()&&compatible(value_type,resolve_type(type_spelling(type_arguments(destination.id).front())));
                if (!literal_integer_ok && !reference_bind_ok && !atomic_init && destination.valid() && value_type.valid() && !compatible(value_type, destination)) {
                    const auto init_key="infix::=|"+normalize_operator_type(resolved_type_name(st.declared_type->name))+","+normalize_operator_type(value_type.name);
                    if(operator_returns_.find(init_key)==operator_returns_.end()) result.diagnostics.push_back(Diagnostic{st.value->span, "cannot initialize '" + st.name + "' of type " + st.declared_type->name + " from incompatible value"});
                }
            } else {
                if (!value_type.valid()) result.diagnostics.push_back(Diagnostic{st.span, "cannot infer type of '" + st.name + "'"});
                if (value_type.name == "opaque[]" && st.value && st.value->kind==Expr::Kind::array_literal) result.diagnostics.push_back(Diagnostic{st.value->span,"cannot infer element type of empty array literal\nhelp: add an explicit array type, for example `string[] " + st.name + " := []`"});
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
                } else {if(st.target->kind==Expr::Kind::index&&st.target->left&&st.target->left->kind==Expr::Kind::identifier){auto* owner=lookup(st.target->left->text,SymbolNamespace::value);if(owner&&owner->is_const)result.diagnostics.push_back(Diagnostic{st.target->span,"cannot modify const value '"+st.target->left->text+"'"});}lhs=infer_expression(result,*st.target);}
            } else {
                auto* target = lookup(st.name, SymbolNamespace::value);
                if (!target) { result.diagnostics.push_back(Diagnostic{st.span, "assignment to unknown value '" + st.name + "'"}); break; }
                if (target->is_const) result.diagnostics.push_back(Diagnostic{st.span, "cannot assign to const value '" + st.name + "'"});
                if (target->type_name.rfind("ref<",0)==0) result.diagnostics.push_back(Diagnostic{st.span,"T& bindings cannot be reassigned"});
                lhs=resolve_type(target->type_name);
            }
            if (st.value) { auto rhs=infer_expression(result,*st.value,lhs.id);const bool fitting_integer=st.value->kind==Expr::Kind::integer_literal&&(lhs.kind==TypeKind::signed_int||lhs.kind==TypeKind::unsigned_int)&&integer_literal_fits(st.value->text,lhs);if(type_node(lhs.id).kind==TypeNodeKind::generic&&type_node(lhs.id).name=="atomic"&&rhs.id==lhs.id)result.diagnostics.push_back(Diagnostic{st.value->span,"atomic values cannot be assigned; use store(value.load()) explicitly"});else if(!fitting_integer&&rhs.valid()&&lhs.valid()&&!compatible(rhs,lhs)){const auto assign_key="infix:=|"+normalize_operator_type(lhs.name)+","+normalize_operator_type(rhs.name);if(operator_returns_.find(assign_key)==operator_returns_.end())result.diagnostics.push_back(Diagnostic{st.value->span,"incompatible assignment to '"+label+"'"});} }
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
            const std::string key=operator_key(st.operator_fixity,st.op);
            auto& seen=operator_signatures_[key];if(!seen.insert(signature).second)result.diagnostics.push_back(Diagnostic{st.span,"ambiguous duplicate operator overload for '"+st.op+"' with signature ("+signature+")"});
            if(st.has_body){const auto previous_return=current_function_return_type_;current_function_return_type_=st.return_type?st.return_type->name:"void";push_scope();for(const auto& p:st.parameters)declare(result,Symbol{p.name,SymbolNamespace::value,p.span,p.type.is_const,resolved_type_name(p.type.name)});if(st.value)infer_expression(result,*st.value);else analyze_statements(result,st.body,false);pop_scope();current_function_return_type_=previous_return;}
            break;
        }
        case Stmt::Kind::function_decl: {
            if(!lookup(st.name,SymbolNamespace::function))declare(result, Symbol{st.name, SymbolNamespace::function, st.span, true, function_signature(st)});
            if(st.name=="main"&&st.owner.empty()){
                const std::string result_type=st.return_type?resolved_type_name(st.return_type->name):"void";
                const bool params_ok=st.parameters.empty()||(st.parameters.size()==2&&resolved_type_name(st.parameters[0].type.name)=="string"&&resolved_type_name(st.parameters[1].type.name)=="string[]");
                if(st.is_async||!params_ok||(result_type!="int_32"&&result_type!="void"))result.diagnostics.push_back(Diagnostic{st.span,"main must have signature function main() -> int or function main(string cmd, string[] args) -> int"});
                if(result_type=="int_32"&&st.has_body&&!block_returns(st.body))result.diagnostics.push_back(Diagnostic{st.span,"main -> int must explicitly return an integer value on every reachable path"});
            }
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
            auto returned = infer_expression(result,*st.value,intern_type(current_function_return_type_));
            auto expected=resolve_type(current_function_return_type_);
            if(!current_function_return_type_.empty()&&expected.kind==TypeKind::void_type)result.diagnostics.push_back(Diagnostic{st.span,"void function cannot return a value"});
            else if(!current_function_return_type_.empty()&&returned.valid()&&expected.valid()&&!compatible(returned,expected))result.diagnostics.push_back(Diagnostic{st.span,"return value is incompatible with function return type '"+current_function_return_type_+"'"});
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
        } else if(!current_function_return_type_.empty()&&resolve_type(current_function_return_type_).kind!=TypeKind::void_type)result.diagnostics.push_back(Diagnostic{st.span,"non-void function must return a value"});
        break;
        case Stmt::Kind::throw_stmt: {
            std::string thrown; if(st.value){if(st.value->kind==Expr::Kind::call&&st.value->left&&st.value->left->kind==Expr::Kind::identifier){thrown=st.value->left->text;for(const auto& a:st.value->arguments)infer_expression(result,*a);}else if(st.value->kind==Expr::Kind::struct_literal)thrown=st.value->text;else {auto t=infer_expression(result,*st.value);thrown=t.name;}}
            thrown=resolved_type_name(thrown);if(checked_error_types_.find(thrown)==checked_error_types_.end())result.diagnostics.push_back(Diagnostic{st.span,"'"+thrown+"' is not a declared checked-error type\nhelp: declare it with `error "+thrown+" { string message; }`"});else if(current_function_errors_.find(thrown)==current_function_errors_.end() && catch_all_depth_==0)result.diagnostics.push_back(Diagnostic{st.span,"throw of checked error "+thrown+" is not declared in the enclosing function\nhelp: add `: "+thrown+"` to the function signature, or handle it before it escapes"});break;
        }
        case Stmt::Kind::try_stmt: {
            const auto saved_errors=current_function_errors_;
            bool has_catch_all=false;
            for(const auto& c:st.catches){if(!c.type){has_catch_all=true;}else {auto caught=resolved_type_name(c.type->name);if(checked_error_types_.find(caught)==checked_error_types_.end())result.diagnostics.push_back(Diagnostic{c.type->span,"catch type '"+caught+"' is not a declared checked-error type"});current_function_errors_.insert(std::move(caught));}}
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
    SemanticResult result; scopes_.clear(); aliases_.clear(); struct_fields_.clear(); abstract_methods_.clear(); struct_bases_.clear(); enum_members_.clear(); named_types_.clear(); checked_error_types_.clear(); current_function_return_type_.clear(); current_function_errors_.clear(); function_errors_.clear(); function_candidates_.clear(); operator_signatures_.clear(); operator_returns_.clear(); extern_c_functions_.clear(); unsafe_depth_=0; catch_all_depth_=0; enforce_standard_modules_=program.enforce_standard_modules; standard_modules_.clear(); standard_modules_.insert(program.standard_modules.begin(), program.standard_modules.end());
    named_types_.insert(api_named_types().begin(),api_named_types().end());
    for(const auto& field:api_fields())struct_fields_[field.owner][field.name]=type_spelling(field.type);
    checked_error_types_.insert("Error");
    checked_error_types_.insert("IOError");
    checked_error_types_.insert("ParseError");
    for(const auto& name:api_named_types())if(name.size()>=5&&name.compare(name.size()-5,5,"Error")==0)checked_error_types_.insert(name);
    for(const auto& callable:api_callables())for(const auto& error:callable.checked_errors){checked_error_types_.insert(error);if(callable.owner.empty())function_errors_[callable.name].insert(error);}
    for(const auto& st:program.statements)if(st->kind==Stmt::Kind::struct_decl){named_types_.insert(st->name);if(st->is_error)checked_error_types_.insert(st->name);struct_bases_[st->name]=st->bases;for(const auto& m:st->body)if(m->kind==Stmt::Kind::function_decl&&!m->has_body)abstract_methods_[st->name].insert(m->name);for(const auto& m:st->body)if(m->kind==Stmt::Kind::function_decl&&m->has_body)abstract_methods_[st->name].erase(m->name);}
    for(const auto& st:program.statements)if(st->kind==Stmt::Kind::enum_decl)named_types_.insert(st->name);
    for(std::size_t pass=0;pass<program.statements.size()+1;++pass)for(const auto& st:program.statements)if(st->kind==Stmt::Kind::struct_decl){for(const auto& base:st->bases){auto it=abstract_methods_.find(base);if(it!=abstract_methods_.end())abstract_methods_[st->name].insert(it->second.begin(),it->second.end());}for(const auto& m:st->body)if(m->kind==Stmt::Kind::function_decl&&m->has_body)abstract_methods_[st->name].erase(m->name);}
    for(const auto& st:program.statements)if(st->kind==Stmt::Kind::function_decl&&!st->owner.empty()&&st->has_body)abstract_methods_[st->owner].erase(st->name);
    aliases_["int"]="int_32"; aliases_["uint"]="uint_32"; aliases_["double"]="double_32";
    for(const auto& st:program.statements)if(st->kind==Stmt::Kind::function_decl){function_candidates_[st->name].push_back(st.get());if(st->is_extern_c)extern_c_functions_.insert(st->name);auto& errs=function_errors_[st->name];for(const auto& e:st->error_types){auto name=resolved_type_name(e.name);if(checked_error_types_.find(name)==checked_error_types_.end())result.diagnostics.push_back(Diagnostic{e.span,"'"+name+"' is not a declared checked-error type\nhelp: declare it with `error "+name+" { string message; }`"});errs.insert(std::move(name));}}
    for(const auto& st:program.statements)if(st->kind==Stmt::Kind::operator_decl){std::string signature;for(std::size_t i=0;i<st->parameters.size();++i){if(i)signature+=",";signature+=normalize_operator_type(resolved_type_name(st->parameters[i].type.name));}operator_returns_[operator_key(st->operator_fixity,st->op)+"|"+signature]=st->return_type?resolved_type_name(st->return_type->name):"void";}
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
