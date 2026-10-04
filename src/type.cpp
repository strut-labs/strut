#include "strut/type.h"

#include <charconv>
#include <limits>
#include <algorithm>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace strut {
namespace {
struct TypeStore {
    std::mutex mutex;
    std::unordered_map<std::string, TypeId> ids;
    std::unordered_map<std::uint64_t, TypeNode> nodes;
    std::unordered_map<std::uint64_t, std::string> spellings;
};
TypeStore& type_store(){static TypeStore value;return value;}
std::uint64_t stable_hash(std::string_view text){std::uint64_t h=14695981039346656037ull;for(unsigned char c:text){h^=c;h*=1099511628211ull;}return h?h:1;}
std::vector<std::string> split_types(std::string_view text){std::vector<std::string> out;std::size_t start=0;int angle=0,round=0,square=0;for(std::size_t i=0;i<=text.size();++i){const char c=i<text.size()?text[i]:',';if(c=='<')++angle;else if(c=='>'&&(i==0||text[i-1]!='-'))--angle;else if(c=='(')++round;else if(c==')')--round;else if(c=='[')++square;else if(c==']')--square;else if(c==','&&angle==0&&round==0&&square==0){out.emplace_back(text.substr(start,i-start));start=i+1;}}return out;}
std::string trim(std::string_view s){while(!s.empty()&&(s.front()==' '||s.front()=='\t'))s.remove_prefix(1);while(!s.empty()&&(s.back()==' '||s.back()=='\t'))s.remove_suffix(1);return std::string(s);}
bool primitive(std::string_view n){return n=="void"||n=="bool"||n=="string"||n=="json"||n=="null"||n=="int_8"||n=="int_16"||n=="int_32"||n=="int_64"||n=="uint_8"||n=="uint_16"||n=="uint_32"||n=="uint_64"||n=="double_32"||n=="double_64";}
TypeId parse_type(std::string text);
// Parse a checked-error clause from " : (A, B)" or " : A" (leading text already consumed the ':').
std::vector<TypeId> parse_error_clause(std::string_view text){
    std::vector<TypeId> errors;
    const std::string trimmed=trim(text);
    if(trimmed.empty())return errors;
    bool parenthesized=trimmed.front()=='('&&trimmed.back()==')';
    const std::string body=parenthesized?trimmed.substr(1,trimmed.size()-2):trimmed;
    for(const auto& part:split_types(body)){auto e=parse_type(trim(part));if(e)errors.push_back(e);}
    std::sort(errors.begin(),errors.end(),[&](TypeId a,TypeId b){return type_spelling(a)<type_spelling(b);});
    errors.erase(std::unique(errors.begin(),errors.end()),errors.end());
    return errors;
}
// Build the canonical error-set spelling " : (A, B)" (or " : A" for one, empty for none).
std::string error_set_spelling(const std::vector<TypeId>& errors){
    if(errors.empty())return {};
    if(errors.size()==1)return " : "+type_spelling(errors[0]);
    std::string out=" : (";for(std::size_t i=0;i<errors.size();++i){if(i)out+=", ";out+=type_spelling(errors[i]);}out+=")";return out;
}
std::string error_set_key(const std::vector<TypeId>& errors){std::string key;for(const auto& e:errors){key+=":e"+std::to_string(e.value);}return key;}
TypeId install(TypeNode node,std::string key,std::string spelling){auto& store=type_store();auto found=store.ids.find(key);if(found!=store.ids.end())return found->second;TypeId id{stable_hash(key)};for(;;){auto it=store.spellings.find(id.value);if(it==store.spellings.end()||it->second==spelling)break;++id.value;}store.ids.emplace(std::move(key),id);store.nodes.emplace(id.value,std::move(node));store.spellings.emplace(id.value,std::move(spelling));return id;}
TypeId parse_type(std::string text){
    text=trim(text);if(text.empty())return {};
    if(text=="int")text="int_32";else if(text=="uint")text="uint_32";else if(text=="double")text="double_32";
    auto unary=[&](std::string_view head,TypeNodeKind kind)->TypeId{if(text.rfind(head,0)!=0||text.size()<=head.size()||text.back()!='>')return {};auto child=parse_type(text.substr(head.size(),text.size()-head.size()-1));if(!child)return {};auto spelling=std::string(head)+type_spelling(child)+">";return install({kind,{}, {child},{},0},"u:"+std::to_string(static_cast<int>(kind))+":"+std::to_string(child.value),spelling);};
    if(text.rfind("const ",0)==0){auto child=parse_type(text.substr(6));if(!child)return {};return install({TypeNodeKind::const_type,{}, {child},{},0},"const:"+std::to_string(child.value),"const "+type_spelling(child));}
    if(text.back()=='?'){auto child=parse_type(text.substr(0,text.size()-1));if(!child)return {};return install({TypeNodeKind::nullable,{}, {child},{},0},"nullable:"+std::to_string(child.value),type_spelling(child)+"?");}
    if(auto id=unary("ptr<",TypeNodeKind::safe_pointer))return id;
    if(auto id=unary("raw_ptr<",TypeNodeKind::raw_pointer))return id;
    if(auto id=unary("weak_ptr<",TypeNodeKind::weak_pointer))return id;
    if(auto id=unary("ref<",TypeNodeKind::reference))return id;
    if(text.size()>2&&text.compare(text.size()-2,2,"[]")==0){auto child=parse_type(text.substr(0,text.size()-2));if(!child)return {};return install({TypeNodeKind::vector,{}, {child},{},0},"vector:"+std::to_string(child.value),type_spelling(child)+"[]");}
    if(text.back()==']'){auto open=text.rfind('[');if(open!=std::string::npos&&open+1<text.size()-1){std::size_t n=0;auto [p,ec]=std::from_chars(text.data()+open+1,text.data()+text.size()-1,n);if(ec==std::errc{}&&p==text.data()+text.size()-1){auto child=parse_type(text.substr(0,open));return install({TypeNodeKind::fixed_array,{}, {child},{},n},"array:"+std::to_string(child.value)+":"+std::to_string(n),type_spelling(child)+"["+std::to_string(n)+"]");}}}
    const auto function_open=text.find("<(");if(text.rfind("function",0)==0&&function_open!=std::string::npos&&text.back()=='>'){std::size_t arrow=std::string::npos;int f_angle=0,f_round=0;for(std::size_t i=function_open+1;i+1<text.size();++i){char c=text[i];if(c=='<')++f_angle;else if(c=='>'&&(i==0||text[i-1]!='-'))--f_angle;else if(c=='(')++f_round;else if(c==')')--f_round;else if(c=='-'&&f_angle==0&&f_round==0&&i+1<text.size()&&text[i+1]=='>'){arrow=i;break;}}if(arrow!=std::string::npos){const auto prefix=text.substr(0,function_open);auto params_text=std::string_view(text).substr(function_open+2,arrow-1-(function_open+2));std::vector<TypeId> children;if(!trim(params_text).empty()&&trim(params_text)!="()"){auto parts=split_types(params_text);for(const auto& part:parts)if(!trim(part).empty())children.push_back(parse_type(part));}auto return_and_rest=text.substr(arrow+2,text.size()-arrow-2-1);std::vector<TypeId> errors;std::string return_text=return_and_rest;int angle=0,round=0;for(std::size_t i=0;i<return_and_rest.size();++i){char c=return_and_rest[i];if(c=='<')++angle;else if(c=='>'&&(i==0||return_and_rest[i-1]!='-'))--angle;else if(c=='(')++round;else if(c==')')--round;else if(c==':'&&angle==0&&round==0&&i+1<return_and_rest.size()&&return_and_rest[i+1]==' '){return_text=return_and_rest.substr(0,i);errors=parse_error_clause(std::string_view(return_and_rest).substr(i+1));break;}}children.push_back(parse_type(return_text));std::string key="fn:"+prefix,spelling=prefix+"<(";for(std::size_t i=0;i+1<children.size();++i){key+=":"+std::to_string(children[i].value);if(i)spelling+=", ";spelling+=type_spelling(children[i]);}key+="->"+std::to_string(children.back().value);spelling+=")->"+type_spelling(children.back());key+=error_set_key(errors);spelling+=error_set_spelling(errors)+">";TypeNode node{TypeNodeKind::function,prefix,children,{},0};node.error_types=errors;return install(std::move(node),key,spelling);}}
    auto open=text.find('<');if(open!=std::string::npos&&text.back()=='>'){
        auto head=text.substr(0,open);auto body=std::string_view(text).substr(open+1,text.size()-open-2);std::vector<TypeId> errors;std::string_view inner_body=body;int angle=0,round=0;for(std::size_t i=0;i<body.size();++i){char c=body[i];if(c=='<')++angle;else if(c=='>'&&(i==0||body[i-1]!='-'))--angle;else if(c=='(')++round;else if(c==')')--round;else if(c==':'&&angle==0&&round==0&&i+1<body.size()&&body[i+1]==' '){errors=parse_error_clause(body.substr(i+1));inner_body=body.substr(0,i);break;}}auto parts=split_types(inner_body);std::vector<TypeId> children;for(const auto& part:parts){auto child=parse_type(part);if(!child)return {};children.push_back(child);}
        TypeNodeKind kind=head=="vector"?TypeNodeKind::vector:head=="tuple"?TypeNodeKind::tuple:TypeNodeKind::generic;
        if(head=="vector"&&children.size()==1)return install({kind,{},children,{},0},"vector:"+std::to_string(children[0].value),type_spelling(children[0])+"[]");
        std::string key="g:"+head,spelling=head+"<";for(std::size_t i=0;i<children.size();++i){key+=":"+std::to_string(children[i].value);if(i)spelling+=", ";spelling+=type_spelling(children[i]);}key+=error_set_key(errors);spelling+=error_set_spelling(errors)+">";TypeNode node{kind,head,children,{},0};node.error_types=errors;return install(std::move(node),key,spelling);
    }
    const auto kind=primitive(text)?TypeNodeKind::primitive:TypeNodeKind::named;return install({kind,text,{},{},0},"n:"+text,text);
}
}

TypeId intern_type(std::string_view spelling){auto& store=type_store();std::lock_guard<std::mutex> lock(store.mutex);return parse_type(std::string(spelling));}
const TypeNode& type_node(TypeId id){static const TypeNode invalid{};auto& s=type_store();auto it=s.nodes.find(id.value);return it==s.nodes.end()?invalid:it->second;}
std::string type_spelling(TypeId id){auto& s=type_store();auto it=s.spellings.find(id.value);return it==s.spellings.end()?std::string():it->second;}
std::string type_debug(TypeId id){const auto& n=type_node(id);std::ostringstream out;out<<"TypeId("<<id.value<<") kind="<<static_cast<int>(n.kind)<<" spelling="<<type_spelling(id);return out.str();}
bool type_is(TypeId id,TypeNodeKind kind){return type_node(id).kind==kind;}
TypeId type_element(TypeId id){const auto& n=type_node(id);return n.children.empty()?TypeId{}:n.children.front();}
const std::vector<TypeId>& type_arguments(TypeId id){return type_node(id).children;}

TypeInfo builtin_type(std::string_view n) {
    if (n == "int") n = "int_32";
    if (n == "uint") n = "uint_32";
    if (n == "double") n = "double_32";
    if (n == "void") return {TypeKind::void_type, 0, std::string(n), intern_type(n)};
    if (n == "bool") return {TypeKind::bool_type, 0, std::string(n), intern_type(n)};
    if (n == "string") return {TypeKind::string_type, 0, std::string(n), intern_type(n)};
    if (n == "json") return {TypeKind::json_type, 0, std::string(n), intern_type(n)};
    if (n == "int_8") return {TypeKind::signed_int, 8, std::string(n),intern_type(n)};
    if (n == "int_16") return {TypeKind::signed_int, 16, std::string(n),intern_type(n)};
    if (n == "int_32") return {TypeKind::signed_int, 32, std::string(n),intern_type(n)};
    if (n == "int_64") return {TypeKind::signed_int, 64, std::string(n),intern_type(n)};
    if (n == "uint_8") return {TypeKind::unsigned_int, 8, std::string(n),intern_type(n)};
    if (n == "uint_16") return {TypeKind::unsigned_int, 16, std::string(n),intern_type(n)};
    if (n == "uint_32") return {TypeKind::unsigned_int, 32, std::string(n),intern_type(n)};
    if (n == "uint_64") return {TypeKind::unsigned_int, 64, std::string(n),intern_type(n)};
    if (n == "double_32") return {TypeKind::floating, 32, std::string(n),intern_type(n)};
    if (n == "double_64") return {TypeKind::floating, 64, std::string(n),intern_type(n)};
    return {};
}

static std::optional<std::uint64_t> parse_u64(std::string_view text) {
    std::uint64_t value = 0;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || ptr != text.data() + text.size()) return std::nullopt;
    return value;
}

TypeInfo infer_integer_literal(std::string_view text) {
    auto value = parse_u64(text);
    if (!value) return {};
    if (*value <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) return builtin_type("int_32");
    if (*value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return builtin_type("int_64");
    return builtin_type("uint_64");
}

TypeInfo infer_floating_literal(std::string_view) { return builtin_type("double_32"); }

bool integer_literal_fits(std::string_view text, const TypeInfo& destination) {
    auto value = parse_u64(text);
    if (!value) return false;
    if (destination.kind == TypeKind::unsigned_int) {
        if (destination.bits == 64) return true;
        return *value < (std::uint64_t{1} << destination.bits);
    }
    if (destination.kind == TypeKind::signed_int) {
        if (destination.bits == 64) return *value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        return *value < (std::uint64_t{1} << (destination.bits - 1));
    }
    return false;
}

std::optional<std::size_t> array_extent(std::string_view name) {
    if (name.size() < 3 || name.back() != ']') return std::nullopt;
    const auto open = name.rfind('[');
    if (open == std::string_view::npos || open + 1 == name.size() - 1) return std::nullopt;
    std::size_t value = 0;
    const auto first = name.data() + open + 1;
    const auto last = name.data() + name.size() - 1;
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) return std::nullopt;
    return value;
}

bool is_nullable_type(std::string_view name) { return !name.empty() && name.back() == '?'; }
std::string strip_nullable(std::string_view name) { return is_nullable_type(name) ? std::string(name.substr(0, name.size()-1)) : std::string(name); }

bool can_implicitly_convert(const TypeInfo& from, const TypeInfo& to) {
    if (!from.valid() || !to.valid()) return false;
    if (from.kind == to.kind && from.bits <= to.bits) return true;
    if ((from.kind == TypeKind::signed_int || from.kind == TypeKind::unsigned_int) &&
        to.kind == TypeKind::floating && to.bits >= 32) return true;
    return false;
}

} // namespace strut
