#include "strut/codegen.h"
#include "strut/runtime_components.h"
#include "strut/type.h"
#include <cstdlib>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <vector>
#include <algorithm>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif
namespace strut { namespace {
std::string cpp_type(std::string t);
std::string cpp_type(TypeId id);
std::string cpp_type_legacy(std::string t);
std::string safe_symbol(std::string s){for(char& c:s)if(!std::isalnum(static_cast<unsigned char>(c)))c='_';return s;}
std::string normalized_operator_type(std::string t){if(t.rfind("ref<",0)==0&&t.back()=='>')t=t.substr(4,t.size()-5);if(t.rfind("const ",0)==0)t=t.substr(6);return t;}
std::string operator_param_cpp(const std::string& t,const std::string& name){if(t.rfind("ref<const ",0)==0&&t.back()=='>')return "const "+cpp_type(t.substr(10,t.size()-11))+"& "+name;if(t.rfind("ref<",0)==0&&t.back()=='>')return cpp_type(t.substr(4,t.size()-5))+"& "+name;return cpp_type(t)+" "+name;}
std::string operator_return_cpp(const std::string& t){if(t.rfind("ref<const ",0)==0&&t.back()=='>')return "const "+cpp_type(t.substr(10,t.size()-11))+"&";if(t.rfind("ref<",0)==0&&t.back()=='>')return cpp_type(t.substr(4,t.size()-5))+"&";return cpp_type(t);}
std::string cpp_type(std::string t){return cpp_type(intern_type(t));}
std::string cpp_type(TypeId id){
    if(!id)return "auto";
    const auto& n=type_node(id);auto child=[&](std::size_t i){return i<n.children.size()?cpp_type(n.children[i]):std::string("auto");};
    switch(n.kind){
        case TypeNodeKind::const_type:return "const "+child(0);
        case TypeNodeKind::reference:return "strut_ref<"+child(0)+">";
        case TypeNodeKind::safe_pointer:return "std::shared_ptr<"+child(0)+">";
        case TypeNodeKind::raw_pointer:return child(0)+"*";
        case TypeNodeKind::weak_pointer:return "std::weak_ptr<"+child(0)+">";
        case TypeNodeKind::nullable:return "std::optional<"+child(0)+">";
        case TypeNodeKind::vector:return "std::vector<"+child(0)+">";
        case TypeNodeKind::fixed_array:return "std::array<"+child(0)+","+std::to_string(n.extent)+">";
        case TypeNodeKind::tuple:{std::string out="std::tuple<";for(std::size_t i=0;i<n.children.size();++i){if(i)out+=",";out+=child(i);}return out+">";}
        case TypeNodeKind::function:{if(n.children.empty())return "auto";std::string out="std::function<"+child(n.children.size()-1)+"(";for(std::size_t i=0;i+1<n.children.size();++i){if(i)out+=",";out+=child(i);}return out+")>";}
        case TypeNodeKind::generic:{
            auto generic=[&](std::string head){std::string out=std::move(head)+"<";for(std::size_t i=0;i<n.children.size();++i){if(i)out+=",";out+=child(i);}return out+">";};
            if(n.name=="map")return generic("std::unordered_map");
            if(n.name=="ordered_map")return generic("std::map");
            if(n.name=="set")return generic("std::unordered_set");
            if(n.name=="ordered_set")return generic("std::set");
            if(n.name=="queue")return generic("std::queue");
            if(n.name=="stack")return generic("std::stack");
            if(n.name=="deque")return generic("std::deque");
            if(n.name=="list")return generic("std::list");
            if(n.name=="future")return generic("strut_future");
            if(n.name=="channel")return generic("strut_channel");
            if(n.name=="atomic")return generic("strut_atomic");
            if(n.name=="priority_queue"&&!n.children.empty()){auto elem=child(0);if(n.children.size()>1&&type_spelling(n.children[1])=="min")return "std::priority_queue<"+elem+",std::vector<"+elem+">,std::greater<"+elem+">>";return "std::priority_queue<"+elem+">";}return generic(n.name);
        }
        case TypeNodeKind::primitive:case TypeNodeKind::named:return cpp_type_legacy(n.name);
        case TypeNodeKind::invalid:return "auto";
    }return "auto";
}
std::string cpp_type_legacy(std::string t){
    if(t.rfind("function<(",0)==0 && t.size()>12 && t.back()=='>'){
        auto arrow=t.rfind(")->");
        if(arrow!=std::string::npos){
            std::string args=t.substr(10,arrow-10); std::string ret=t.substr(arrow+3,t.size()-(arrow+4));
            std::string out="std::function<"+cpp_type(ret)+"("; int depth=0; std::size_t start=0; bool first=true;
            for(std::size_t i=0;i<=args.size();++i){char c=i<args.size()?args[i]:','; if(c=='<'||c=='['||c=='(')++depth; else if(c=='>'||c==']'||c==')')--depth; else if(c==','&&depth==0){auto part=args.substr(start,i-start); if(!part.empty()){if(!first)out+=",";out+=cpp_type(part);first=false;}start=i+1;}}
            return out+")>";
        }
    }
    if(t.rfind("const ",0)==0) return "const "+cpp_type(t.substr(6));
    if(!t.empty() && t.back()=='?') return "std::optional<"+cpp_type(t.substr(0,t.size()-1))+">";
    if(t.rfind("ptr<",0)==0 && t.back()=='>') return "std::shared_ptr<"+cpp_type(t.substr(4,t.size()-5))+">";
    if(t.rfind("ref<",0)==0 && t.back()=='>') return "strut_ref<"+cpp_type(t.substr(4,t.size()-5))+">";
    if(t.rfind("weak_ptr<",0)==0 && t.back()=='>') return "std::weak_ptr<"+cpp_type(t.substr(9,t.size()-10))+">";
    if(t.rfind("raw_ptr<",0)==0 && t.back()=='>') return cpp_type(t.substr(8,t.size()-9))+"*";
    if(t.rfind("tuple<",0)==0 && t.back()=='>'){auto inner=t.substr(6,t.size()-7);std::string out="std::tuple<";int depth=0;std::size_t start=0;bool first=true;for(std::size_t i=0;i<=inner.size();++i){char c=i<inner.size()?inner[i]:',';if(c=='<')++depth;else if(c=='>')--depth;else if(c==','&&depth==0){if(!first)out+=",";out+=cpp_type(inner.substr(start,i-start));first=false;start=i+1;}}return out+">";}
    auto binary_container=[&](std::string_view head,std::string_view cpp)->std::string{if(t.rfind(std::string(head),0)!=0||t.back()!='>')return {};auto inner=t.substr(head.size(),t.size()-head.size()-1);int depth=0;std::size_t comma=std::string::npos;for(std::size_t i=0;i<inner.size();++i){if(inner[i]=='<')++depth;else if(inner[i]=='>')--depth;else if(inner[i]==','&&depth==0){comma=i;break;}}if(comma==std::string::npos)return {};return std::string(cpp)+"<"+cpp_type(inner.substr(0,comma))+","+cpp_type(inner.substr(comma+1))+">";};
    if(auto v=binary_container("ordered_map<","std::map");!v.empty())return v;
    if(auto v=binary_container("map<","std::unordered_map");!v.empty())return v;
    auto unary_container=[&](std::string_view head,std::string_view cpp)->std::string{if(t.rfind(std::string(head),0)!=0||t.back()!='>')return {};return std::string(cpp)+"<"+cpp_type(t.substr(head.size(),t.size()-head.size()-1))+">";};
    if(auto v=unary_container("vector<","std::vector");!v.empty())return v;
    if(auto v=unary_container("ordered_set<","std::set");!v.empty())return v;
    if(auto v=unary_container("set<","std::unordered_set");!v.empty())return v;
    if(auto v=unary_container("queue<","std::queue");!v.empty())return v;
    if(auto v=unary_container("stack<","std::stack");!v.empty())return v;
    if(auto v=unary_container("deque<","std::deque");!v.empty())return v;
    if(auto v=unary_container("list<","std::list");!v.empty())return v;
    if(t.rfind("priority_queue<",0)==0 && t.back()=='>'){auto inner=t.substr(15,t.size()-16);int depth=0;std::size_t comma=std::string::npos;for(std::size_t i=0;i<inner.size();++i){if(inner[i]=='<')++depth;else if(inner[i]=='>')--depth;else if(inner[i]==','&&depth==0){comma=i;break;}}auto elem=comma==std::string::npos?inner:inner.substr(0,comma);auto order=comma==std::string::npos?std::string("max"):inner.substr(comma+1);if(order=="min")return "std::priority_queue<"+cpp_type(elem)+",std::vector<"+cpp_type(elem)+">,std::greater<"+cpp_type(elem)+">>";return "std::priority_queue<"+cpp_type(elem)+">";}
    if(t.size()>2 && t.compare(t.size()-2,2,"[]")==0) return "std::vector<"+cpp_type(t.substr(0,t.size()-2))+">";
    if (auto extent = array_extent(t)) { const auto open=t.rfind('['); return "std::array<"+cpp_type(t.substr(0,open))+","+std::to_string(*extent)+">"; }
    if(t=="void") return "void";
    if(t=="bool") return "bool";
    if(t=="string") return "strut_string";
    if(t=="json") return "json::Document";
    if(t=="bytes") return "std::vector<std::uint8_t>";
    if(t=="istream") return "strut_istream";
    if(t=="ostream") return "strut_ostream";
    if(t=="sstream") return "strut_sstream";
    if(t=="ifstream") return "strut_ifstream";
    if(t=="ofstream") return "strut_ofstream";
    if(t=="exec_result") return "strut_exec_result";
    if(t=="process") return "strut_process";
    if(t=="process_in") return "strut_process_in";
    if(t=="process_out") return "strut_process_out";
    if(t=="thread") return "strut_thread";
    if(t=="tcp_socket") return "strut_tcp_socket";
    if(t=="tcp_listener") return "strut_tcp_listener";
    if(t=="tls_stream") return "strut_tls_stream";
    if(t=="http_response") return "strut_http_response";
    if(t=="http_request") return "strut_server_request";
    if(t=="http_server_response") return "strut_server_response";
    if(t=="http_server") return "strut_http_server";
    if(t=="sqlite_db") return "strut_sqlite_db";
    if(t=="mutex") return "strut_mutex";
    if(t.rfind("future<",0)==0&&t.back()=='>') return "strut_future<"+cpp_type(t.substr(7,t.size()-8))+">";
    if(t.rfind("channel<",0)==0&&t.back()=='>') return "strut_channel<"+cpp_type(t.substr(8,t.size()-9))+">";
    if(t.rfind("atomic<",0)==0&&t.back()=='>') return "strut_atomic<"+cpp_type(t.substr(7,t.size()-8))+">";
    if(t=="int"||t=="int_32") return "std::int32_t";
    if(t=="int_8") return "std::int8_t";
    if(t=="int_16") return "std::int16_t";
    if(t=="int_64") return "std::int64_t";
    if(t=="uint"||t=="uint_32") return "std::uint32_t";
    if(t=="uint_8") return "std::uint8_t";
    if(t=="uint_16") return "std::uint16_t";
    if(t=="uint_64") return "std::uint64_t";
    if(t=="double"||t=="double_32") return "float";
    if(t=="double_64") return "double";
    auto generic_open=t.find('<'); if(generic_open!=std::string::npos && t.back()=='>'){std::string head=t.substr(0,generic_open);std::string inner=t.substr(generic_open+1,t.size()-generic_open-2);std::string out=head+"<";int depth=0;std::size_t start=0;bool first=true;for(std::size_t i=0;i<=inner.size();++i){char c=i<inner.size()?inner[i]:',';if(c=='<')++depth;else if(c=='>')--depth;else if(c==','&&depth==0){if(!first)out+=",";out+=cpp_type(inner.substr(start,i-start));first=false;start=i+1;}}return out+">";}
    if(t=="opaque"||t=="function") return "auto";
    return t.empty()?"auto":t;
}

std::string literal_value(const std::string& q){if(q.size()<2)return q;std::string o;for(std::size_t i=1;i+1<q.size();++i){char c=q[i];if(c=='\\'&&i+2<q.size()){char n=q[++i];if(n=='n')o+='\n';else if(n=='r')o+='\r';else if(n=='t')o+='\t';else o+=n;}else o+=c;}return o;}
std::string embedded_string_cpp(const std::string& data){std::ostringstream o;o<<"[&](){const unsigned char d[]={";for(std::size_t i=0;i<data.size();++i){if(i)o<<',';o<<static_cast<unsigned int>(static_cast<unsigned char>(data[i]));}o<<"};return strut_string(std::string(reinterpret_cast<const char*>(d),sizeof(d)));}()";return o.str();}
std::string compile_embed_file(const std::string& quoted){auto path=std::filesystem::path(literal_value(quoted));std::ifstream f(path,std::ios::binary);if(!f)return "strut_embed_file("+std::string("strut_string(")+quoted+")";std::ostringstream b;b<<f.rdbuf();return embedded_string_cpp(b.str());}
std::string compile_embed_dir(const std::string& quoted){
    auto root=std::filesystem::path(literal_value(quoted));
    if(!std::filesystem::exists(root)) return "strut_embed_dir(strut_string("+quoted+"))";
    std::vector<std::filesystem::path> files;
    for(auto& e:std::filesystem::recursive_directory_iterator(root)) if(e.is_regular_file()) files.push_back(e.path());
    std::sort(files.begin(),files.end());
    std::ostringstream o;
    o<<"[&](){std::unordered_map<strut_string,strut_string> m;";
    for(const auto& path:files){
        std::ifstream f(path,std::ios::binary); std::ostringstream b; b<<f.rdbuf();
        auto rel=std::filesystem::relative(path,root).generic_string();
        std::string esc;
        for(char c:rel){ if(c=='\\' || c=='"') esc.push_back('\\'); esc.push_back(c); }
        o<<"m[strut_string(\""<<esc<<"\")]="<<embedded_string_cpp(b.str())<<";";
    }
    o<<"return m;}()";
    return o.str();
}

thread_local std::string strut_codegen_source_path;
std::string escaped_line_path(const std::string& value){std::string out;for(char c:value){if(c=='\\'||c=='"')out.push_back('\\');out.push_back(c);}return out;}
void emit_source_line(std::ostringstream& o,const IRStmt& s){if(!strut_codegen_source_path.empty()&&s.span.begin.line>0)o<<"#line "<<s.span.begin.line<<" \""<<escaped_line_path(strut_codegen_source_path)<<"\"\n";}
std::string expr(const IRExpr& e);
void stmt(std::ostringstream& o,const IRStmt& s,int n);

std::string call_argument(const IRExpr& e) {
    if (e.kind == IRExpr::Kind::array_literal && e.type_name != "opaque[]" && e.type_name.size() > 2 && e.type_name.compare(e.type_name.size() - 2, 2, "[]") == 0) {
        return cpp_type(e.type_name) + expr(e);
    }
    return expr(e);
}

bool stmt_assigns_name(const IRStmt& s,const std::string& name){
    if(s.kind==IRStmt::Kind::assignment && !s.target && s.name==name)return true;
    auto scan=[&](const std::vector<IRStmtPtr>& body){for(const auto& child:body)if(stmt_assigns_name(*child,name))return true;return false;};
    if(scan(s.body)||scan(s.else_body))return true;
    for(const auto& c:s.switch_cases)if(scan(c.body))return true;
    for(const auto& c:s.catches)if(scan(c.body))return true;
    if(s.initializer&&stmt_assigns_name(*s.initializer,name))return true;
    return false;
}
bool body_assigns_name(const std::vector<IRStmtPtr>& body,const std::string& name){for(const auto& s:body)if(stmt_assigns_name(*s,name))return true;return false;}
std::string function_param_cpp(const IRStmt& fn,const Parameter& p){
    const auto type=cpp_type(p.type.name);
    if(!fn.is_extern_c && p.type.name.rfind("ptr<",0)==0 && !body_assigns_name(fn.body,p.name))return "[[maybe_unused]] const "+type+"& "+p.name;
    return "[[maybe_unused]] "+type+" "+p.name;
}
std::string json_expr(const IRExpr& e){
    switch(e.kind){
        case IRExpr::Kind::json_object:{std::string out="[&](){json::Document d=json::Document::make_object();";for(std::size_t i=0;i+1<e.arguments.size();i+=2){out+="d["+e.arguments[i]->text+"]="+json_expr(*e.arguments[i+1])+";";}return out+="return d;}()";}
        case IRExpr::Kind::array_literal:{std::string out="[&](){json::Document d=json::Document::make_array();";for(const auto& a:e.arguments)out+="d.push_back("+json_expr(*a)+");";return out+="return d;}()";}
        case IRExpr::Kind::string_literal:return "json::Document(std::string("+e.text+"))";
        case IRExpr::Kind::integer_literal:return "json::Document(static_cast<int>("+e.text+"))";
        case IRExpr::Kind::floating_literal:return "json::Document(static_cast<double>("+e.text+"))";
        case IRExpr::Kind::boolean_literal:return "json::Document("+e.text+")";
        case IRExpr::Kind::null_literal:return "json::Document(nullptr)";
        default:return "strut_json_value("+expr(e)+")";
    }
}
std::string expr(const IRExpr& e){
    switch(e.kind){
        case IRExpr::Kind::identifier: if(e.text=="this") return "(*this)"; return e.text;
        case IRExpr::Kind::integer_literal: case IRExpr::Kind::floating_literal: case IRExpr::Kind::boolean_literal: return e.text;
        case IRExpr::Kind::string_literal: return "strut_string("+e.text+")";
        case IRExpr::Kind::null_literal:return "strut_null";
        case IRExpr::Kind::array_literal:{std::string out="{";for(size_t i=0;i<e.arguments.size();++i){if(i)out+=",";out+=expr(*e.arguments[i]);}return out+"}";}
        case IRExpr::Kind::map_literal:{std::string out="{";for(size_t i=0;i+1<e.arguments.size();i+=2){if(i)out+=",";out+="{"+expr(*e.arguments[i])+","+expr(*e.arguments[i+1])+"}";}return out+"}";}
        case IRExpr::Kind::tuple_literal:{std::string out="std::make_tuple(";for(size_t i=0;i<e.arguments.size();++i){if(i)out+=",";out+=expr(*e.arguments[i]);}return out+")";}
        case IRExpr::Kind::json_object:return json_expr(e);
        case IRExpr::Kind::struct_literal:{std::string out="[&](){"+e.text+" __strut_value{};";for(std::size_t i=0;i<e.names.size();++i)out+="__strut_value."+e.names[i]+"="+expr(*e.arguments[i])+";";return out+"return __strut_value;}()";}
        case IRExpr::Kind::grouping:return "("+expr(*e.left)+")";
        case IRExpr::Kind::unary:if(e.text=="await")return "strut_await("+expr(*e.right)+")";if(e.text=="*"&&e.right&&e.right->type_name.rfind("ptr<",0)==0)return "strut_deref("+expr(*e.right)+",\""+escaped_line_path(strut_codegen_source_path)+"\","+std::to_string(e.span.begin.line)+")";return e.text+expr(*e.right);
        case IRExpr::Kind::postfix:return expr(*e.left)+e.text;
        case IRExpr::Kind::binary:if(e.text=="??")return "strut_coalesce("+expr(*e.left)+","+expr(*e.right)+")";return "("+expr(*e.left)+" "+e.text+" "+expr(*e.right)+")";
        case IRExpr::Kind::safe_member:return "strut_safe_member("+expr(*e.left)+",[](const auto& value){return value."+e.text+";})";
        case IRExpr::Kind::member:{if(e.text.rfind("::",0)==0)return expr(*e.left)+e.text; if(e.text.rfind("->",0)==0)return expr(*e.left)+e.text; if(e.left && !e.left->type_name.empty() && e.left->type_name.back()=='?') return "(*"+expr(*e.left)+")."+e.text; if(e.left&&e.left->kind==IRExpr::Kind::identifier&&e.left->text=="json"){if(e.text=="parse")return "strut_json_parse";if(e.text=="stringify")return "strut_json_stringify";if(e.text=="pretty")return "strut_json_pretty";if(e.text=="encode")return "strut_json_value";}std::string m=e.text;const std::string bt=e.left?e.left->type_name:std::string();if(e.left&&bt=="http_server"&&m=="static")m="serve_static";const bool seq=(bt.find("[]")!=std::string::npos||bt.rfind("vector<",0)==0||bt.rfind("deque<",0)==0||bt.rfind("list<",0)==0);const bool set_like=(bt.rfind("set<",0)==0||bt.rfind("ordered_set<",0)==0);if(m=="push"&&seq)m="push_back";else if(m=="pop"&&seq)m="pop_back";else if(m=="add"&&set_like)m="insert";else if(m=="remove")m="erase";if(m=="length"){if(e.left&&bt.rfind("ref<",0)==0)return "(*"+expr(*e.left)+").size()";if(e.left&&bt.rfind("ptr<",0)==0)return expr(*e.left)+"->size()";return expr(*e.left)+".size()";}if(e.left&&bt.rfind("ref<",0)==0)return "(*"+expr(*e.left)+")."+m;if(e.left&&bt.rfind("ptr<",0)==0)return expr(*e.left)+"->"+m;return expr(*e.left)+"."+m;}
        case IRExpr::Kind::index:{if(e.left->type_name=="json"){std::string k=(e.right->kind==IRExpr::Kind::string_literal)?e.right->text:expr(*e.right);return expr(*e.left)+"["+k+"]";}if(e.left->type_name.rfind("tuple<",0)==0)return "std::get<"+e.right->text+">("+expr(*e.left)+")";return expr(*e.left)+".at("+expr(*e.right)+")";}
        case IRExpr::Kind::call:{
            if(e.left && e.left->kind==IRExpr::Kind::member && e.left->text=="length" && e.arguments.empty()) return expr(*e.left);
            if(e.left && e.left->kind==IRExpr::Kind::member && e.left->text=="insert" && e.left->left && e.arguments.size()==2){return "strut_map_insert("+expr(*e.left->left)+","+expr(*e.arguments[0])+","+expr(*e.arguments[1])+")";}
            if(e.left && e.left->kind==IRExpr::Kind::member && e.left->text=="contains" && e.left->left && e.arguments.size()==1){return "strut_contains("+expr(*e.left->left)+","+expr(*e.arguments[0])+")";}
            if(e.left && e.left->kind==IRExpr::Kind::member && e.left->left){
                const auto& m=e.left->text; const auto base=expr(*e.left->left);
                if(m=="map"&&e.arguments.size()==1)return "strut_map("+base+","+expr(*e.arguments[0])+")";
                if(m=="filter"&&e.arguments.size()==1)return "strut_filter("+base+","+expr(*e.arguments[0])+")";
                if(m=="reduce"&&e.arguments.size()==1)return "strut_reduce("+base+","+expr(*e.arguments[0])+")";
                if(m=="reduce"&&e.arguments.size()==2)return "strut_reduce("+base+","+expr(*e.arguments[0])+","+expr(*e.arguments[1])+")";
                if(m=="any"&&e.arguments.size()==1)return "strut_any("+base+","+expr(*e.arguments[0])+")";
                if(m=="all"&&e.arguments.size()==1)return "strut_all("+base+","+expr(*e.arguments[0])+")";
                if(m=="find"&&e.arguments.size()==1)return "strut_find("+base+","+expr(*e.arguments[0])+")";
                if(m=="count"&&e.arguments.size()==1)return "strut_count("+base+","+expr(*e.arguments[0])+")";
                if(m=="sort"&&e.arguments.size()==1)return "strut_sort("+base+","+expr(*e.arguments[0])+")";
                if(m=="count_by"&&e.arguments.size()==1)return "strut_count_by("+base+","+expr(*e.arguments[0])+")";
                if(m=="index_by"&&e.arguments.size()==1)return "strut_index_by("+base+","+expr(*e.arguments[0])+")";
                if(m=="partition"&&e.arguments.size()==1)return "strut_partition("+base+","+expr(*e.arguments[0])+")";
                if(m=="pick"&&e.arguments.size()==1)return "strut_json_pick("+base+","+expr(*e.arguments[0])+")";
                if(m=="omit"&&e.arguments.size()==1)return "strut_json_omit("+base+","+expr(*e.arguments[0])+")";
                if(m=="merge_deep"&&e.arguments.size()==1)return "strut_json_merge_deep("+base+","+expr(*e.arguments[0])+")";
            }
            if(e.left&&e.left->kind==IRExpr::Kind::identifier&&e.arguments.size()==1&&e.arguments[0]->kind==IRExpr::Kind::string_literal){if(e.left->text=="embed_file")return compile_embed_file(e.arguments[0]->text);if(e.left->text=="embed_dir")return compile_embed_dir(e.arguments[0]->text);}
            std::string name=expr(*e.left); if(name=="new"&&e.arguments.size()==1)return "strut_ptr("+expr(*e.arguments[0])+")"; if(name=="ptr"&&e.arguments.size()==1)return "strut_raw("+expr(*e.arguments[0])+")"; if(name=="ref"&&e.arguments.size()==1)return "strut_make_ref("+expr(*e.arguments[0])+")"; if(name=="weak"&&e.arguments.size()==1)return "strut_weak("+expr(*e.arguments[0])+")"; if(name=="print"||name=="println"){std::string out="strut_print(";for(size_t i=0;i<e.arguments.size();++i){if(i)out+=",";out+=expr(*e.arguments[i]);}return out+")";}
            if(name=="input") name="strut_input"; else if(name=="istream") name="strut_istream"; else if(name=="ostream") name="strut_ostream"; else if(name=="sstream") name="strut_sstream"; else if(name=="ifstream") name="strut_ifstream"; else if(name=="ofstream") name="strut_ofstream"; else if(name=="join") name="strut_join"; else if(name=="to_int") name="strut_to_int"; else if(name=="to_double") name="strut_to_double"; else if(name=="to_string") name="strut_to_string"; else if(name=="exists") name="strut_fs_exists"; else if(name=="is_file") name="strut_fs_is_file"; else if(name=="is_dir") name="strut_fs_is_dir"; else if(name=="file_size") name="strut_fs_file_size"; else if(name=="modified") name="strut_fs_modified"; else if(name=="make_dir") name="strut_fs_make_dir"; else if(name=="remove") name="strut_fs_remove"; else if(name=="remove_all") name="strut_fs_remove_all"; else if(name=="copy") name="strut_fs_copy"; else if(name=="move") name="strut_fs_move"; else if(name=="touch") name="strut_fs_touch"; else if(name=="ls") name="strut_fs_ls"; else if(name=="walk") name="strut_fs_walk"; else if(name=="cwd") name="strut_fs_cwd"; else if(name=="cd") name="strut_fs_cd"; else if(name=="absolute") name="strut_fs_absolute"; else if(name=="canonical") name="strut_fs_canonical"; else if(name=="parent") name="strut_fs_parent"; else if(name=="filename") name="strut_fs_filename"; else if(name=="extension") name="strut_fs_extension"; else if(name=="stem") name="strut_fs_stem"; else if(name=="join_path") name="strut_fs_join_path"; else if(name=="read_file") name="strut_fs_read_file"; else if(name=="read_bytes") name="strut_fs_read_bytes"; else if(name=="write_file") name="strut_fs_write_file"; else if(name=="append_file") name="strut_fs_append_file"; else if(name=="env") name="strut_env"; else if(name=="set_env") name="strut_set_env"; else if(name=="unset_env") name="strut_unset_env"; else if(name=="now_ms") name="strut_now_ms"; else if(name=="unix_ms") name="strut_unix_ms"; else if(name=="sleep_ms") name="strut_sleep_ms"; else if(name=="exec") name="strut_exec"; else if(name=="exec_shell") name="strut_exec_shell"; else if(name=="process") name="strut_process"; else if(name=="pipe_exec") name="strut_pipe_exec"; else if(name=="thread") name="strut_thread"; else if(name=="mutex") name="strut_mutex"; else if(name=="http_server") name="strut_http_server"; else if(name=="http_text") name="strut_http_text"; else if(name=="http_html") name="strut_http_html"; else if(name=="http_json_response") name="strut_http_json_response"; else if(name=="sqlite_open") name="strut_sqlite_open"; else if(name=="embed_file") name="strut_embed_file"; else if(name=="embed_dir") name="strut_embed_dir";
            std::string out=name+"(";for(size_t i=0;i<e.arguments.size();++i){if(i)out+=",";out+=call_argument(*e.arguments[i]);}return out+")";
        }
        case IRExpr::Kind::lambda:{std::ostringstream o;o<<"[=](";for(std::size_t i=0;i<e.lambda_parameters.size();++i){if(i)o<<",";{const auto& tn=e.lambda_parameters[i].type.name;bool generic=!tn.empty();for(unsigned char c:tn)if(std::islower(c))generic=false;o<<"[[maybe_unused]] "<<(generic?"auto":cpp_type(tn))<<" "<<e.lambda_parameters[i].name;}}o<<")";if(e.lambda_async){o<<" { return strut_async([=]() mutable";if(e.lambda_expression)o<<" { return "<<expr(*e.lambda_expression)<<"; }); }";else{o<<" {\n";for(const auto& c:e.lambda_body)stmt(o,*c,8);o<<"    });\n}";}}else if(e.lambda_expression){o<<" { return "<<expr(*e.lambda_expression)<<"; }";}else{o<<" {\n";for(const auto& c:e.lambda_body)stmt(o,*c,4);o<<"}";}return o.str();}
    } return {};
}
void stmt(std::ostringstream& o,const IRStmt& s,int n){std::string pad(n,' ');emit_source_line(o,s);
    switch(s.kind){
        case IRStmt::Kind::declaration:o<<pad<<(s.is_const?"const ":"")<<cpp_type(s.type_name)<<" "<<s.name;if(s.value){o<<" = ";if(!s.overload_name.empty())o<<s.overload_name<<"("<<expr(*s.value)<<")";else o<<expr(*s.value);}else o<<"{}";o<<";\n";break;
        case IRStmt::Kind::assignment:{
            std::string target=s.name;
            if(s.target){
                if(s.target->kind==IRExpr::Kind::index&&s.target->left&&
                   (s.target->left->type_name.rfind("map<",0)==0||s.target->left->type_name.rfind("ordered_map<",0)==0))
                    target=expr(*s.target->left)+"["+expr(*s.target->right)+"]";
                else target=expr(*s.target);
            }
            if(!s.overload_name.empty())o<<pad<<s.overload_name<<"("<<target<<","<<expr(*s.value)<<");\n";
            else o<<pad<<target<<" "<<s.op<<" "<<expr(*s.value)<<";\n";
            break;
        }
        case IRStmt::Kind::expression:o<<pad<<expr(*s.value)<<";\n";break;
        case IRStmt::Kind::return_stmt:o<<pad<<"return"<<(s.value?" "+expr(*s.value):"")<<";\n";break;
        case IRStmt::Kind::throw_stmt:{std::string en="Error";std::string msg="\"checked error\"",code="0";if(s.value&&s.value->kind==IRExpr::Kind::call&&s.value->left&&s.value->left->kind==IRExpr::Kind::identifier){en=s.value->left->text;if(!s.value->arguments.empty())msg=(s.value->arguments[0]->kind==IRExpr::Kind::string_literal?s.value->arguments[0]->text:expr(*s.value->arguments[0])+".v");if(s.value->arguments.size()>1)code=expr(*s.value->arguments[1]);}else if(s.value&&s.value->kind==IRExpr::Kind::struct_literal){en=s.value->text;for(std::size_t i=0;i<s.value->names.size();++i){if(s.value->names[i]=="message")msg=s.value->arguments[i]->kind==IRExpr::Kind::string_literal?s.value->arguments[i]->text:expr(*s.value->arguments[i])+".v";else if(s.value->names[i]=="code")code=expr(*s.value->arguments[i]);}}o<<pad<<"throw strut_checked_error(\""<<en<<"\","<<msg<<","<<code<<");\n";break;}
        case IRStmt::Kind::try_stmt:{
            o<<pad<<"try {\n";for(const auto& c:s.body)stmt(o,*c,n+4);o<<pad<<"} catch (const strut_checked_error& __strut_error) {\n";
            bool first=true;bool catch_all=false;
            for(const auto& c:s.catches){if(c.catch_all){catch_all=true;o<<pad<<(first?"    {\n":"    else {\n");}else{o<<pad<<(first?"    if (":"    else if (")<<"__strut_error.type == \""<<c.type_name<<"\") {\n";}if(!c.name.empty())o<<pad<<"        const auto& "<<c.name<<" = __strut_error;\n";for(const auto& child:c.body)stmt(o,*child,n+8);o<<pad<<"    }\n";first=false;}
            if(!catch_all) o<<pad<<"    else { throw; }\n";
            o<<pad<<"}\n";break;}
        case IRStmt::Kind::break_stmt:o<<pad<<"break;\n";break; case IRStmt::Kind::continue_stmt:o<<pad<<"continue;\n";break;
        case IRStmt::Kind::unsafe_stmt:o<<pad<<"{ /* unsafe */\n";for(auto&c:s.body)stmt(o,*c,n+4);o<<pad<<"}\n";break;
        case IRStmt::Kind::block:o<<pad<<"{\n";for(auto&c:s.body)stmt(o,*c,n+4);o<<pad<<"}\n";break;
        case IRStmt::Kind::if_stmt:o<<pad<<"if ("<<expr(*s.condition)<<") {\n";for(auto&c:s.body)stmt(o,*c,n+4);o<<pad<<"}";if(!s.else_body.empty()){o<<" else {\n";for(auto&c:s.else_body)stmt(o,*c,n+4);o<<pad<<"}";}o<<"\n";break;
        case IRStmt::Kind::while_stmt:o<<pad<<"while ("<<expr(*s.condition)<<") {\n";for(auto&c:s.body)stmt(o,*c,n+4);o<<pad<<"}\n";break;
        case IRStmt::Kind::switch_stmt:o<<pad<<"switch ("<<expr(*s.condition)<<") {\n";for(const auto& c:s.switch_cases){o<<pad<<"    ";if(c.is_default)o<<"default";else o<<"case "<<expr(*c.value);o<<": {\n";for(const auto& child:c.body)stmt(o,*child,n+8);o<<pad<<"        break;\n"<<pad<<"    }\n";}o<<pad<<"}\n";break;
        case IRStmt::Kind::match_stmt:{bool first=true;for(const auto& c:s.switch_cases){if(c.is_default){o<<(first?pad:pad+"else ")<<"{\n";}else{o<<pad<<(first?"if (":"else if (")<<expr(*s.condition)<<" == "<<expr(*c.value)<<") {\n";}for(const auto& child:c.body)stmt(o,*child,n+4);o<<pad<<"}\n";first=false;}break;}
        case IRStmt::Kind::for_stmt:{o<<pad<<"for (";if(s.initializer){const auto& init=*s.initializer;if(init.kind==IRStmt::Kind::declaration){if(init.is_const)o<<"const ";o<<cpp_type(init.type_name)<<" "<<init.name;if(init.value)o<<" = "<<expr(*init.value);}else if(init.kind==IRStmt::Kind::assignment){if(init.target)o<<expr(*init.target);else o<<init.name;o<<" "<<init.op<<" "<<expr(*init.value);}else if(init.value)o<<expr(*init.value);}o<<"; ";if(s.condition)o<<expr(*s.condition);o<<"; ";if(s.increment)o<<expr(*s.increment);o<<") {\n";for(auto&c:s.body)stmt(o,*c,n+4);o<<pad<<"}\n";break;}
        case IRStmt::Kind::range_for:o<<pad<<"for (auto& "<<s.name<<" : "<<expr(*s.value)<<") {\n";for(auto&c:s.body)stmt(o,*c,n+4);o<<pad<<"}\n";break;
        case IRStmt::Kind::enum_decl:{o<<pad<<"enum class "<<s.name<<" : std::int32_t {";for(std::size_t i=0;i<s.enum_names.size();++i){if(i)o<<",";o<<s.enum_names[i]<<"="<<s.enum_values[i];}o<<"};\n";break;}
        case IRStmt::Kind::struct_decl:{if(!s.generic_parameters.empty()){o<<pad<<"template<";for(std::size_t i=0;i<s.generic_parameters.size();++i){if(i)o<<",";o<<"class "<<s.generic_parameters[i];}o<<">\n";}o<<pad<<"struct "<<s.name; if(!s.bases.empty()){o<<" : ";for(std::size_t i=0;i<s.bases.size();++i){if(i)o<<", ";o<<"public "<<cpp_type(s.bases[i]);}} o<<" {\n";for(const auto& f:s.fields)o<<pad<<"    "<<cpp_type(f.type.name)<<" "<<f.name<<"{};\n";for(const auto& m:s.body){o<<pad<<"    "<<cpp_type(m->return_type)<<" "<<m->name<<"(";for(std::size_t i=0;i<m->parameters.size();++i){if(i)o<<",";o<<cpp_type(m->parameters[i].type.name)<<" "<<m->parameters[i].name;}o<<")";if(!m->has_body){o<<";\n";}else{o<<" {\n";for(const auto& c:m->body)stmt(o,*c,n+8);o<<pad<<"    }\n";}}o<<pad<<"};\n";break;}
        case IRStmt::Kind::type_alias:o<<pad<<"using "<<s.name<<" = "<<cpp_type(s.alias_target_id)<<";\n";break;
        case IRStmt::Kind::operator_decl:{
            if(s.parameters.size()==2 && (s.op==":="||s.op=="=")){
                const std::string dest=normalized_operator_type(s.parameters[0].type.name),src=normalized_operator_type(s.parameters[1].type.name);
                const std::string helper="strut_op_"+(s.op==":="?std::string("init"):std::string("assign"))+"_"+safe_symbol(dest)+"_"+safe_symbol(src);
                if(s.op==":="){o<<cpp_type(dest)<<" "<<helper<<"("<<operator_param_cpp(s.parameters[1].type.name,s.parameters[1].name)<<") {\n";o<<"    "<<cpp_type(dest)<<" "<<s.parameters[0].name<<"{};\n";if(s.value&&s.value->kind==IRExpr::Kind::lambda){if(s.value->lambda_expression)o<<"    (void)"<<expr(*s.value->lambda_expression)<<";\n";else for(const auto& c:s.value->lambda_body)stmt(o,*c,4);}else for(const auto& c:s.body)stmt(o,*c,4);o<<"    return "<<s.parameters[0].name<<";\n}\n";}
                else{o<<"void "<<helper<<"("<<cpp_type(dest)<<"& "<<s.parameters[0].name<<","<<operator_param_cpp(s.parameters[1].type.name,s.parameters[1].name)<<") {\n";if(s.value&&s.value->kind==IRExpr::Kind::lambda){if(s.value->lambda_expression)o<<"    (void)"<<expr(*s.value->lambda_expression)<<";\n";else for(const auto& c:s.value->lambda_body)stmt(o,*c,4);}else for(const auto& c:s.body)stmt(o,*c,4);o<<"}\n";}break;
            }
            if(!s.generic_parameters.empty()){o<<"template<";for(std::size_t i=0;i<s.generic_parameters.size();++i){if(i)o<<",";o<<"class "<<s.generic_parameters[i];}o<<">\n";}o<<operator_return_cpp(s.return_type)<<" operator"<<s.op<<"(";for(std::size_t i=0;i<s.parameters.size();++i){if(i)o<<",";o<<operator_param_cpp(s.parameters[i].type.name,s.parameters[i].name);}if(s.operator_fixity==OperatorFixity::postfix)o<<", int";o<<")";if(!s.has_body){o<<";\n";break;}o<<" {\n";if(s.value&&s.value->kind==IRExpr::Kind::lambda){if(s.value->lambda_expression)o<<std::string(n+4,' ')<<"return "<<expr(*s.value->lambda_expression)<<";\n";else for(const auto& c:s.value->lambda_body)stmt(o,*c,n+4);}else for(const auto& c:s.body)stmt(o,*c,n+4);o<<"}\n";break;}
        case IRStmt::Kind::function_decl:{if(s.is_extern_c)o<<"extern \"C\" ";if(!s.generic_parameters.empty()){o<<"template<";for(std::size_t i=0;i<s.generic_parameters.size();++i){if(i)o<<",";o<<"class "<<s.generic_parameters[i];}o<<">\n";}const bool native_main=s.name=="main"&&s.owner.empty()&&!s.is_async;const bool main_void=native_main&&s.return_type=="void";if(native_main)o<<"#ifdef _WIN32\nint wmain(int argc,wchar_t** argv)\n#else\nint main(int argc,char** argv)\n#endif\n";else{o<<(s.is_async?("strut_future<"+cpp_type(s.return_type)+">"):cpp_type(s.return_type))<<" ";if(!s.owner.empty())o<<s.owner<<"::";o<<s.name<<"(";for(size_t i=0;i<s.parameters.size();++i){if(i)o<<",";o<<function_param_cpp(s,s.parameters[i]);}o<<")";}if(!s.has_body){o<<";\n";break;}o<<" {\n";if(native_main){if(s.parameters.size()==2){o<<"    strut_string "<<s.parameters[0].name<<"(argc > 0 ? strut_native_arg(argv[0]) : std::string());\n    std::vector<strut_string> "<<s.parameters[1].name<<";\n    "<<s.parameters[1].name<<".reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0);\n    for(int strut_i=1;strut_i<argc;++strut_i) "<<s.parameters[1].name<<".emplace_back(strut_native_arg(argv[strut_i]));\n";}else o<<"    (void)argc; (void)argv;\n";}if(s.is_async){o<<"    return strut_async([=]() mutable -> "<<cpp_type(s.return_type)<<" {\n";for(auto&c:s.body)stmt(o,*c,n+8);o<<"    });\n";}else{for(auto&c:s.body){if(main_void&&c->kind==IRStmt::Kind::return_stmt&&!c->value){o<<std::string(n+4,' ')<<"return 0;\n";}else stmt(o,*c,n+4);}}o<<"}\n";break;}
        default:break;
    }}
}
bool minimal_type_ok(const std::string& t){
    return t.find("json")==std::string::npos &&
           t.find("weak_ptr<")==std::string::npos &&
           t.find("raw_ptr<")==std::string::npos && t.find("future<")==std::string::npos &&
           t.find("channel<")==std::string::npos && t!="mutex" && t!="thread" &&
           t!="istream" && t!="ostream" && t!="sstream" && t!="ifstream" && t!="ofstream" &&
           t!="process" && t!="tcp_socket" && t!="tcp_listener" && t!="tls_stream" &&
           t!="http_response" && t!="http_request" && t!="http_server_response" && t!="http_server" && t!="sqlite_db";
}
bool minimal_stmt_ok(const IRStmt* s);
bool minimal_expr_ok(const IRExpr* e){
    if(!e)return true;
    if(e->kind==IRExpr::Kind::json_object||e->kind==IRExpr::Kind::struct_literal||e->kind==IRExpr::Kind::safe_member)return false;
    if(!minimal_type_ok(e->type_name))return false;
    if(e->lambda_async)return false;
    if(e->kind==IRExpr::Kind::member){
        const auto& m=e->text;
        if(m!="push"&&m!="pop"&&m!="length"&&m!="add"&&m!="remove"&&m!="contains"&&m!="front"&&m!="back"&&m!="top"&&m!="empty"&&m!="insert"&&m!="map"&&m!="filter"&&m!="reduce"&&m!="any"&&m!="all"&&m!="find"&&m!="count"&&m!="sort"&&m!="reserve")return false;
    }
    if(!minimal_expr_ok(e->left.get())||!minimal_expr_ok(e->right.get())||!minimal_expr_ok(e->lambda_expression.get()))return false;
    for(const auto& a:e->arguments)if(!minimal_expr_ok(a.get()))return false;
    for(const auto& st:e->lambda_body) if(!minimal_stmt_ok(st.get())) return false;
    return true;
}
bool minimal_stmt_ok(const IRStmt* s){
    if(!s)return true;
    if(s->kind==IRStmt::Kind::throw_stmt||s->kind==IRStmt::Kind::try_stmt||s->kind==IRStmt::Kind::struct_decl||s->kind==IRStmt::Kind::operator_decl||s->kind==IRStmt::Kind::unsafe_stmt)return false;
    if(s->is_async||s->is_extern_c||!minimal_type_ok(s->type_name)||!minimal_type_ok(s->return_type))return false;
    for(const auto& p:s->parameters)if(!minimal_type_ok(p.type.name))return false;
    if(!minimal_expr_ok(s->value.get())||!minimal_expr_ok(s->target.get())||!minimal_expr_ok(s->condition.get())||!minimal_expr_ok(s->increment.get()))return false;
    if(s->initializer&&!minimal_stmt_ok(s->initializer.get()))return false;
    for(const auto& c:s->body)if(!minimal_stmt_ok(c.get()))return false;
    for(const auto& c:s->else_body)if(!minimal_stmt_ok(c.get()))return false;
    for(const auto& c:s->switch_cases){if(!minimal_expr_ok(c.value.get()))return false;for(const auto& st:c.body)if(!minimal_stmt_ok(st.get()))return false;}
    return true;
}
bool program_uses_minimal_runtime(const IRProgram& p){for(const auto& s:p.statements)if(!minimal_stmt_ok(s.get()))return false;return true;}

bool light_thread_type_ok(const std::string& t){return t=="thread"||t.rfind("atomic<",0)==0||minimal_type_ok(t);}
bool light_thread_stmt_ok(const IRStmt* s);
bool light_thread_expr_ok(const IRExpr* e){
    if(!e)return true;
    if(e->kind==IRExpr::Kind::json_object||e->kind==IRExpr::Kind::struct_literal||e->kind==IRExpr::Kind::safe_member||!light_thread_type_ok(e->type_name))return false;
    if(e->lambda_async)return false;
    if(e->kind==IRExpr::Kind::member){
        const auto& m=e->text;
        if(m!="push"&&m!="pop"&&m!="length"&&m!="add"&&m!="remove"&&m!="contains"&&m!="front"&&m!="back"&&m!="top"&&m!="empty"&&m!="insert"&&m!="map"&&m!="filter"&&m!="reduce"&&m!="any"&&m!="all"&&m!="find"&&m!="count"&&m!="sort"&&m!="reserve"&&m!="join"&&m!="joinable"&&m!="load"&&m!="store"&&m!="exchange"&&m!="compare_exchange"&&m!="fetch_add"&&m!="fetch_sub")return false;
    }
    if(!light_thread_expr_ok(e->left.get())||!light_thread_expr_ok(e->right.get())||!light_thread_expr_ok(e->lambda_expression.get()))return false;
    for(const auto& a:e->arguments)if(!light_thread_expr_ok(a.get()))return false;
    for(const auto& st:e->lambda_body)if(!light_thread_stmt_ok(st.get()))return false;
    return true;
}
bool light_thread_stmt_ok(const IRStmt* s){
    if(!s)return true;
    if(s->kind==IRStmt::Kind::throw_stmt||s->kind==IRStmt::Kind::try_stmt||s->kind==IRStmt::Kind::struct_decl||s->kind==IRStmt::Kind::operator_decl||s->kind==IRStmt::Kind::unsafe_stmt)return false;
    if(s->is_async||s->is_extern_c||!light_thread_type_ok(s->type_name)||!light_thread_type_ok(s->return_type))return false;
    for(const auto& p:s->parameters)if(!light_thread_type_ok(p.type.name))return false;
    if(!light_thread_expr_ok(s->value.get())||!light_thread_expr_ok(s->target.get())||!light_thread_expr_ok(s->condition.get())||!light_thread_expr_ok(s->increment.get()))return false;
    if(s->initializer&&!light_thread_stmt_ok(s->initializer.get()))return false;
    for(const auto& c:s->body)if(!light_thread_stmt_ok(c.get()))return false;
    for(const auto& c:s->else_body)if(!light_thread_stmt_ok(c.get()))return false;
    for(const auto& c:s->switch_cases){if(!light_thread_expr_ok(c.value.get()))return false;for(const auto& st:c.body)if(!light_thread_stmt_ok(st.get()))return false;}
    return true;
}
bool program_uses_light_thread_runtime(const IRProgram& p){
    const auto components=analyze_runtime_components(p);
    if(!components.contains(RuntimeComponentId::threading)||components.contains(RuntimeComponentId::channels)||components.contains(RuntimeComponentId::mutex)||components.contains(RuntimeComponentId::async)||components.contains(RuntimeComponentId::process)||components.contains(RuntimeComponentId::networking)||components.contains(RuntimeComponentId::filesystem)||components.contains(RuntimeComponentId::json)||components.contains(RuntimeComponentId::sqlite)||components.contains(RuntimeComponentId::ffi)||!p.standard_modules.empty())return false;
    for(const auto& s:p.statements)if(!light_thread_stmt_ok(s.get()))return false;
    return true;
}

struct MinimalRuntimeFeatures {
    bool strings=false;
    bool atomics=false;
    bool pointers=false;
    bool nullable=false;
    bool function=false;
    bool vector=false;
    bool array=false;
    bool map=false;
    bool filter=false;
    bool reduce=false;
    bool any=false;
    bool all=false;
    bool find=false;
    bool count=false;
    bool sort=false;
    bool hash_map=false;
    bool ordered_map=false;
    bool hash_set=false;
    bool ordered_set=false;
    bool queue=false;
    bool stack=false;
    bool deque=false;
    bool list=false;
    bool priority_queue=false;
    bool priority_queue_min=false;
    bool tuple=false;
    bool contains=false;
    bool map_insert=false;
};
void collect_minimal_type_features(const std::string& t,MinimalRuntimeFeatures& f){
    if(t.find("string")!=std::string::npos)f.strings=true;
    if(t.rfind("atomic<",0)==0)f.atomics=true;
    if(t.find("ptr<")!=std::string::npos||t.find("ref<")!=std::string::npos)f.pointers=true;
    if(t.find("function<")!=std::string::npos)f.function=true;
    if(t.find("[]")!=std::string::npos||t.find("vector<")!=std::string::npos)f.vector=true;
    if(t.find("ordered_map<")!=std::string::npos)f.ordered_map=true; else if(t.find("map<")!=std::string::npos)f.hash_map=true;
    if(t.find("ordered_set<")!=std::string::npos)f.ordered_set=true; else if(t.find("set<")!=std::string::npos)f.hash_set=true;
    if(t.find("priority_queue<")!=std::string::npos){f.priority_queue=true;if(t.find(",min>")!=std::string::npos)f.priority_queue_min=true;}
    else if(t.find("queue<")!=std::string::npos)f.queue=true;
    if(t.find("stack<")!=std::string::npos)f.stack=true;
    if(t.find("deque<")!=std::string::npos)f.deque=true;
    if(t.find("list<")!=std::string::npos)f.list=true;
    if(t.find("tuple<")!=std::string::npos)f.tuple=true;
    if(array_extent(t).has_value())f.array=true;
    if(!t.empty()&&t.back()=='?')f.nullable=true;
}
void collect_minimal_stmt_features(const IRStmt* s,MinimalRuntimeFeatures& f);
void collect_minimal_expr_features(const IRExpr* e,MinimalRuntimeFeatures& f){
    if(!e) return;
    collect_minimal_type_features(e->type_name,f);
    if(e->kind==IRExpr::Kind::string_literal)f.strings=true;
    if(e->kind==IRExpr::Kind::null_literal)f.nullable=true;
    if(e->kind==IRExpr::Kind::array_literal)f.vector=true;
    if(e->kind==IRExpr::Kind::map_literal)f.hash_map=true;
    if(e->kind==IRExpr::Kind::call&&e->left&&e->left->kind==IRExpr::Kind::identifier&&(e->left->text=="ptr"||e->left->text=="ref"))f.pointers=true;
    if(e->kind==IRExpr::Kind::call&&e->left&&e->left->kind==IRExpr::Kind::member){
        const auto& m=e->left->text;
        const std::string base_type=e->left->left?e->left->left->type_name:std::string();
        if(base_type.find("[]")!=std::string::npos||base_type.rfind("vector<",0)==0)f.vector=true;
        if(m=="map"){f.map=true;f.vector=true;}else if(m=="filter")f.filter=true;else if(m=="reduce")f.reduce=true;else if(m=="any")f.any=true;else if(m=="all")f.all=true;else if(m=="find")f.find=true;else if(m=="count")f.count=true;else if(m=="sort")f.sort=true;else if(m=="contains")f.contains=true;else if(m=="insert")f.map_insert=true;
    }
    collect_minimal_expr_features(e->left.get(),f);collect_minimal_expr_features(e->right.get(),f);collect_minimal_expr_features(e->lambda_expression.get(),f);
    for(const auto& a:e->arguments)collect_minimal_expr_features(a.get(),f);
    for(const auto& statement:e->lambda_body)collect_minimal_stmt_features(statement.get(),f);
}
void collect_minimal_stmt_features(const IRStmt* s,MinimalRuntimeFeatures& f){
    if(!s) return;
    collect_minimal_type_features(s->type_name,f);
    collect_minimal_type_features(s->return_type,f);
    for(const auto& p:s->parameters)collect_minimal_type_features(p.type.name,f);
    collect_minimal_expr_features(s->value.get(),f);collect_minimal_expr_features(s->target.get(),f);collect_minimal_expr_features(s->condition.get(),f);collect_minimal_expr_features(s->increment.get(),f);
    if(s->initializer) collect_minimal_stmt_features(s->initializer.get(),f);
    for(const auto& c:s->body) collect_minimal_stmt_features(c.get(),f);
    for(const auto& c:s->else_body) collect_minimal_stmt_features(c.get(),f);
    for(const auto& c:s->switch_cases){collect_minimal_expr_features(c.value.get(),f);for(const auto& statement:c.body)collect_minimal_stmt_features(statement.get(),f);}
}
MinimalRuntimeFeatures minimal_features(const IRProgram& p){MinimalRuntimeFeatures f;for(const auto& s:p.statements)collect_minimal_stmt_features(s.get(),f);return f;}
void emit_minimal_runtime(std::ostringstream& o,const MinimalRuntimeFeatures& f){
    o << "#include <cstdint>\n#include <iostream>\n#include <string>\n";
    if(f.vector||f.map||f.filter||f.reduce||f.any||f.all||f.find||f.count||f.sort||f.priority_queue_min)o << "#include <vector>\n";
    if(f.hash_map)o << "#include <unordered_map>\n";
    if(f.ordered_map)o << "#include <map>\n";
    if(f.hash_set)o << "#include <unordered_set>\n";
    if(f.ordered_set)o << "#include <set>\n";
    if(f.queue||f.priority_queue)o << "#include <queue>\n";
    if(f.stack)o << "#include <stack>\n";
    if(f.deque)o << "#include <deque>\n";
    if(f.list)o << "#include <list>\n";
    if(f.priority_queue_min)o << "#include <functional>\n";
    if(f.tuple)o << "#include <tuple>\n";
    if(f.array)o << "#include <array>\n";
    if(f.function)o << "#include <functional>\n";
    if(f.nullable||f.find)o << "#include <optional>\n";
    if(f.map||f.nullable||f.pointers)o << "#include <type_traits>\n";
    if(f.strings||f.nullable||f.pointers||f.map_insert)o << "#include <utility>\n";
    if(f.sort)o << "#include <algorithm>\n";
    if(f.reduce||f.pointers)o << "#include <stdexcept>\n";
    if(f.pointers)o << "#include <memory>\n#include <cstdlib>\n#ifdef _WIN32\n#ifndef WIN32_LEAN_AND_MEAN\n#define WIN32_LEAN_AND_MEAN\n#endif\n#ifndef NOMINMAX\n#define NOMINMAX\n#endif\n#include <windows.h>\n#else\n#include <execinfo.h>\n#endif\n";
    if(f.pointers){o << R"CPP(
template<class T> class strut_ref {
public:
    explicit strut_ref(T& value):p_(&value){}
    strut_ref(const strut_ref&)=default;
    template<class U,class=std::enable_if_t<std::is_convertible_v<U*,T*>>> strut_ref(const strut_ref<U>& other):p_(other.get()){}
    strut_ref& operator=(const strut_ref&)=delete;
    T& operator*() const{return *p_;}
    T* operator->() const{return p_;}
    T* get() const{return p_;}
private:T* p_;
};
template<class T> strut_ref<T> strut_make_ref(T& value){return strut_ref<T>(value);}
template<class T> std::shared_ptr<typename std::decay<T>::type> strut_ptr(T&& value){using U=typename std::decay<T>::type;return std::make_shared<U>(std::forward<T>(value));}
inline void strut_print_stack_trace(){
#ifdef _WIN32
    void* frames[48];const auto n=CaptureStackBackTrace(0,48,frames,nullptr);std::cerr<<"stack trace:\n";for(unsigned short i=0;i<n;++i)std::cerr<<"  "<<frames[i]<<"\n";
#else
    void* frames[48];const int n=backtrace(frames,48);std::cerr<<"stack trace:\n";char** symbols=backtrace_symbols(frames,n);if(symbols){for(int i=0;i<n;++i)std::cerr<<"  "<<symbols[i]<<"\n";std::free(symbols);}
#endif
}

[[noreturn]] inline void strut_panic(const char* message){std::cerr<<"Strut panic: "<<message<<"\n";strut_print_stack_trace();throw std::runtime_error(message);}
template<class T> T& strut_deref(const std::shared_ptr<T>& value,const char* file,std::size_t line){if(!value) [[unlikely]] {std::cerr<<"at "<<file<<":"<<line<<"\n";strut_panic("null safe-pointer dereference");}return *value;}
)CPP";}
    if(f.strings){o << R"CPP(
struct strut_string {
    std::string v;
    strut_string()=default;
    strut_string(const char* s):v(s){}
    strut_string(std::string s):v(std::move(s)){}
    std::size_t size() const{return v.size();}
    bool empty() const{return v.empty();}
    char at(std::size_t i) const{return v.at(i);}
};
inline std::ostream& operator<<(std::ostream& o,const strut_string& s){return o<<s.v;}
inline strut_string operator+(const strut_string& a,const strut_string& b){return a.v+b.v;}
inline bool operator==(const strut_string& a,const strut_string& b){return a.v==b.v;}
inline bool operator!=(const strut_string& a,const strut_string& b){return !(a==b);}
inline bool operator<(const strut_string& a,const strut_string& b){return a.v<b.v;}
namespace std { template<> struct hash<strut_string>{size_t operator()(const strut_string& s) const noexcept{return std::hash<std::string>{}(s.v);}}; }
)CPP";}
    if(f.nullable){o << R"CPP(
struct strut_null_t {
    template<class T> operator std::optional<T>() const{return std::nullopt;}
)CPP"; if(f.pointers)o << "    template<class T> operator std::shared_ptr<T>() const{return {};}\n"; o << R"CPP(
};
[[maybe_unused]] constexpr strut_null_t strut_null{};
)CPP";}
    if(f.nullable)o << R"CPP(
template<class T,class F> auto strut_safe_member(const std::optional<T>& value,F f)->std::optional<typename std::decay<decltype(f(*value))>::type>{if(!value)return std::nullopt;return f(*value);}
)CPP";
    if(f.nullable||f.find)o << "template<class T> T strut_coalesce(const std::optional<T>& value,T fallback){return value?*value:std::move(fallback); }\n";
    if(f.map)o << R"CPP(
template<class C,class F> auto strut_map(const C& xs,F f)->std::vector<typename std::decay<decltype(f(*xs.begin()))>::type>{using R=typename std::decay<decltype(f(*xs.begin()))>::type;std::vector<R> out;out.reserve(xs.size());for(const auto& x:xs)out.push_back(f(x));return out;}
)CPP";
    if(f.filter)o << R"CPP(
template<class C,class F> C strut_filter(const C& xs,F f){C out;for(const auto& x:xs)if(f(x))out.push_back(x);return out;}
)CPP";
    if(f.reduce)o << R"CPP(
template<class C,class F> auto strut_reduce(const C& xs,F f)->typename C::value_type{if(xs.empty())throw std::runtime_error("reduce on empty collection");auto it=xs.begin();auto acc=*it++;for(;it!=xs.end();++it)acc=f(acc,*it);return acc;}
template<class C,class A,class F> A strut_reduce(const C& xs,A acc,F f){for(const auto& x:xs)acc=f(acc,x);return acc;}
)CPP";
    if(f.any)o << "template<class C,class F> bool strut_any(const C& xs,F f){for(const auto& x:xs)if(f(x))return true;return false;}\n";
    if(f.all)o << "template<class C,class F> bool strut_all(const C& xs,F f){for(const auto& x:xs)if(!f(x))return false;return true;}\n";
    if(f.find)o << "template<class C,class F> std::optional<typename C::value_type> strut_find(const C& xs,F f){for(const auto& x:xs)if(f(x))return x;return std::nullopt;}\n";
    if(f.count)o << "template<class C,class F> std::size_t strut_count(const C& xs,F f){std::size_t n=0;for(const auto& x:xs)if(f(x))++n;return n;}\n";
    if(f.sort)o << "template<class C,class F> void strut_sort(C& xs,F f){std::sort(xs.begin(),xs.end(),f);}\n";
    if(f.contains)o << "template<class M,class K> bool strut_contains(const M& m,const K& k){return m.find(k)!=m.end();}\n";
    if(f.map_insert)o << "template<class M,class K,class V> void strut_map_insert(M& m,K&& k,V&& v){m[std::forward<K>(k)]=std::forward<V>(v);}\n";

    o << "template<class... T> void strut_print(const T&... v){((std::cout<<v),...);std::cout<<'\\n';}\n";
}





void emit_light_thread_runtime(std::ostringstream& o,const MinimalRuntimeFeatures& f){
    emit_minimal_runtime(o,f);
    o << "#include <thread>\n#include <tuple>\n#include <exception>\n#include <functional>\n#include <memory>\n#include <utility>\n";
    if(f.atomics)o << "#include <atomic>\n#include <type_traits>\n";
    o << R"STRUT_THREAD(
struct strut_checked_error : std::runtime_error { std::string type; std::string message; std::int32_t code=0; strut_checked_error(std::string t,const std::string& m,std::int32_t c=0):std::runtime_error(m),type(std::move(t)),message(m),code(c){} };
class strut_thread {
public:
    strut_thread()=default;
    template<class F,class... A> explicit strut_thread(F&& f,A&&... a){
        error_=std::make_shared<std::exception_ptr>();auto err=error_;
        thread_=std::thread([err,fn=std::forward<F>(f),args=std::make_tuple(std::forward<A>(a)...)]() mutable {try{std::apply(fn,std::move(args));}catch(...){*err=std::current_exception();}});
    }
    strut_thread(const strut_thread&)=delete;strut_thread& operator=(const strut_thread&)=delete;
    strut_thread(strut_thread&&)=default;strut_thread& operator=(strut_thread&&)=default;
    bool joinable() const{return thread_.joinable();}
    void join(){if(!thread_.joinable())throw strut_checked_error("ThreadError","join on non-joinable thread");thread_.join();if(error_&&*error_)std::rethrow_exception(*error_);}
    ~strut_thread(){if(thread_.joinable())thread_.join();}
private:std::thread thread_;std::shared_ptr<std::exception_ptr> error_;
};
)STRUT_THREAD";
    if(f.atomics)o << R"STRUT_ATOMIC(
template<class T> class strut_atomic {
    static_assert(std::is_integral_v<T>,"atomic<T> supports integer and bool scalar types");
public:
    strut_atomic() noexcept=default;strut_atomic(T value) noexcept:value_(value){}
    strut_atomic(const strut_atomic&)=delete;strut_atomic& operator=(const strut_atomic&)=delete;
    T load() const noexcept{return value_.load();}void store(T value) noexcept{value_.store(value);}
    T exchange(T value) noexcept{return value_.exchange(value);}
    bool compare_exchange(T expected,T desired) noexcept{return value_.compare_exchange_strong(expected,desired);}
    template<class U=T> std::enable_if_t<!std::is_same_v<U,bool>,U> fetch_add(U value) noexcept{return value_.fetch_add(value);}
    template<class U=T> std::enable_if_t<!std::is_same_v<U,bool>,U> fetch_sub(U value) noexcept{return value_.fetch_sub(value);}
private:std::atomic<T> value_{};
};
)STRUT_ATOMIC";
}

void emit_light_json_runtime(std::ostringstream& o,const MinimalRuntimeFeatures& f);
bool light_sqlite_expr_ok(const IRExpr* e){
    if(!e)return true;
    if(e->lambda_async)return false;
    const auto& t=e->type_name;
    if(t.find("http_")!=std::string::npos||t.find("process")!=std::string::npos||t.find("thread")!=std::string::npos||t.find("tcp_")!=std::string::npos||t.find("tls_")!=std::string::npos)return false;
    if(e->kind==IRExpr::Kind::identifier){
        static const char* heavy[]={"ifstream","ofstream","sstream","exec","exec_shell","process","pipe_exec","thread","mutex","http_server","embed_file","embed_dir","exists","walk","read_file","write_file"};
        for(const char* h:heavy)if(e->text==h)return false;
    }
    if(!light_sqlite_expr_ok(e->left.get())||!light_sqlite_expr_ok(e->right.get())||!light_sqlite_expr_ok(e->lambda_expression.get()))return false;
    for(const auto& a:e->arguments)if(!light_sqlite_expr_ok(a.get()))return false;
    return true;
}
bool light_sqlite_stmt_ok(const IRStmt* st){
    if(!st) return true;
    if(st->is_async) return false;
    if(st->type_name.find("http_")!=std::string::npos||st->return_type.find("http_")!=std::string::npos)return false;
    if(!light_sqlite_expr_ok(st->value.get())||!light_sqlite_expr_ok(st->target.get())||!light_sqlite_expr_ok(st->condition.get())||!light_sqlite_expr_ok(st->increment.get()))return false;
    if(st->initializer&&!light_sqlite_stmt_ok(st->initializer.get()))return false;
    for(const auto& c:st->body)if(!light_sqlite_stmt_ok(c.get()))return false;
    for(const auto& c:st->else_body)if(!light_sqlite_stmt_ok(c.get()))return false;
    return true;
}
bool program_uses_light_sqlite_runtime(const IRProgram& p){
    if(!analyze_runtime_components(p).contains(RuntimeComponentId::sqlite)||analyze_runtime_components(p).contains(RuntimeComponentId::http_client))return false;
    for(const auto& m:p.standard_modules)if(m=="filesystem")return false;
    for(const auto& st:p.statements)if(!light_sqlite_stmt_ok(st.get()))return false;
    return true;
}
void emit_light_sqlite_runtime(std::ostringstream& o,const MinimalRuntimeFeatures& f){
    emit_light_json_runtime(o,f);
    o << "#define STRUT_USE_SQLITE 1\n#include <sqlite3.h>\n#include <memory>\n#include <functional>\n#include <cstdlib>\n";
    o << R"STRUT_SQLITE(
struct strut_checked_error : std::runtime_error { std::string type; std::string message; std::int32_t code=0; strut_checked_error(std::string t,const std::string& m,std::int32_t c=0):std::runtime_error(m),type(std::move(t)),message(m),code(c){} };
inline void strut_sqlite_bind(sqlite3_stmt* st,const json::Document& params){if(params.type!=json::Type::Array)return;for(std::size_t i=0;i<params.array.size();++i){const auto& v=params.array[i];int n=static_cast<int>(i+1);switch(v.type){case json::Type::Null:sqlite3_bind_null(st,n);break;case json::Type::Boolean:sqlite3_bind_int(st,n,v.boolean?1:0);break;case json::Type::Number:case json::Type::StrNumber:sqlite3_bind_double(st,n,v.is_number()?std::strtod(v.type==json::Type::StrNumber?v.string.c_str():v.dump().c_str(),nullptr):0.0);break;case json::Type::String:sqlite3_bind_text(st,n,v.string.c_str(),-1,SQLITE_TRANSIENT);break;default:{auto text=v.dump();sqlite3_bind_text(st,n,text.c_str(),-1,SQLITE_TRANSIENT);break;}}}}
struct strut_sqlite_state{sqlite3* db=nullptr;~strut_sqlite_state(){if(db)sqlite3_close(db);}};
class strut_sqlite_db {
public:
    strut_sqlite_db():s_(std::make_shared<strut_sqlite_state>()){} explicit strut_sqlite_db(sqlite3* db):s_(std::make_shared<strut_sqlite_state>()){s_->db=db;}
    void close(){if(s_&&s_->db){sqlite3_close(s_->db);s_->db=nullptr;}}
    void exec(const strut_string& sql) const{exec(sql,json::Document::make_array());}
    void exec(const strut_string& sql,const json::Document& params) const{auto db=handle();sqlite3_stmt* st=nullptr;if(sqlite3_prepare_v2(db,sql.v.c_str(),-1,&st,nullptr)!=SQLITE_OK)throw strut_checked_error("SqliteError",sqlite3_errmsg(db));strut_sqlite_bind(st,params);int rc=sqlite3_step(st);if(rc!=SQLITE_DONE&&rc!=SQLITE_ROW){std::string m=sqlite3_errmsg(db);sqlite3_finalize(st);throw strut_checked_error("SqliteError",m);}sqlite3_finalize(st);}
    json::Document query(const strut_string& sql) const{return query(sql,json::Document::make_array());}
    json::Document query(const strut_string& sql,const json::Document& params) const{auto db=handle();sqlite3_stmt* st=nullptr;if(sqlite3_prepare_v2(db,sql.v.c_str(),-1,&st,nullptr)!=SQLITE_OK)throw strut_checked_error("SqliteError",sqlite3_errmsg(db));strut_sqlite_bind(st,params);json::Document rows=json::Document::make_array();while(sqlite3_step(st)==SQLITE_ROW){json::Document row=json::Document::make_object();for(int i=0;i<sqlite3_column_count(st);++i){const char* n=sqlite3_column_name(st,i);switch(sqlite3_column_type(st,i)){case SQLITE_INTEGER:row[n]=static_cast<double>(sqlite3_column_int64(st,i));break;case SQLITE_FLOAT:row[n]=sqlite3_column_double(st,i);break;case SQLITE_TEXT:row[n]=reinterpret_cast<const char*>(sqlite3_column_text(st,i));break;case SQLITE_NULL:row[n]=json::Document(nullptr);break;default:row[n]="<blob>";}}rows.push_back(row);}sqlite3_finalize(st);return rows;}
    void transaction(std::function<void()> body){exec("BEGIN");try{body();exec("COMMIT");}catch(...){try{exec("ROLLBACK");}catch(...){ }throw;}}
private: sqlite3* handle() const{if(!s_||!s_->db)throw strut_checked_error("SqliteError","database is closed");return s_->db;} std::shared_ptr<strut_sqlite_state> s_;
};
inline strut_sqlite_db strut_sqlite_open(const strut_string& path){sqlite3* db=nullptr;if(sqlite3_open(path.v.c_str(),&db)!=SQLITE_OK){std::string m=db?sqlite3_errmsg(db):"sqlite open failed";if(db)sqlite3_close(db);throw strut_checked_error("SqliteError",m);}return strut_sqlite_db(db);})STRUT_SQLITE";
    o << "\n";
}

bool json_expr_present(const IRExpr* e){
    if(!e)return false;
    if(e->kind==IRExpr::Kind::json_object||e->type_name.find("json")!=std::string::npos)return true;
    if(e->kind==IRExpr::Kind::member&&e->left&&e->left->kind==IRExpr::Kind::identifier&&e->left->text=="json")return true;
    if(json_expr_present(e->left.get())||json_expr_present(e->right.get())||json_expr_present(e->lambda_expression.get()))return true;
    for(const auto& a:e->arguments)if(json_expr_present(a.get()))return true;
    return false;
}
bool json_stmt_present(const IRStmt* st){
    if(!st)return false;
    if(st->type_name.find("json")!=std::string::npos||st->return_type.find("json")!=std::string::npos)return true;
    if(json_expr_present(st->value.get())||json_expr_present(st->target.get())||json_expr_present(st->condition.get())||json_expr_present(st->increment.get()))return true;
    for(const auto& c:st->body)if(json_stmt_present(c.get()))return true;
    for(const auto& c:st->else_body)if(json_stmt_present(c.get()))return true;
    return false;
}
bool light_json_expr_ok(const IRExpr* e){
    if(!e)return true;
    if(e->lambda_async)return false;
    if(e->kind==IRExpr::Kind::member){
        const auto& m=e->text;
        if(m=="count_by"||m=="index_by"||m=="partition"||m=="pick"||m=="omit"||m=="merge_deep")return false;
    }
    if(e->type_name.find("sqlite_db")!=std::string::npos||e->type_name.find("http_")!=std::string::npos||e->type_name.find("process")!=std::string::npos||e->type_name.find("thread")!=std::string::npos)return false;
    if(e->kind==IRExpr::Kind::identifier){
        static const char* heavy[]={"ifstream","ofstream","sstream","exec","exec_shell","process","pipe_exec","thread","mutex","http_server","sqlite_open","embed_file","embed_dir","exists","walk","read_file","write_file"};
        for(const char* h:heavy)if(e->text==h)return false;
    }
    if(!light_json_expr_ok(e->left.get())||!light_json_expr_ok(e->right.get())||!light_json_expr_ok(e->lambda_expression.get()))return false;
    for(const auto& a:e->arguments)if(!light_json_expr_ok(a.get()))return false;
    return true;
}
bool light_json_stmt_ok(const IRStmt* st,bool& found){
    if(!st)return true;
    if(st->is_async)return false;
    if(st->type_name.find("sqlite_db")!=std::string::npos||st->return_type.find("http_")!=std::string::npos)return false;
    if(json_stmt_present(st))found=true;
    if(!light_json_expr_ok(st->value.get())||!light_json_expr_ok(st->target.get())||!light_json_expr_ok(st->condition.get())||!light_json_expr_ok(st->increment.get()))return false;
    if(st->initializer&&!light_json_stmt_ok(st->initializer.get(),found))return false;
    for(const auto& c:st->body)if(!light_json_stmt_ok(c.get(),found))return false;
    for(const auto& c:st->else_body)if(!light_json_stmt_ok(c.get(),found))return false;
    return true;
}
bool program_uses_light_json_runtime(const IRProgram& p){
    if(analyze_runtime_components(p).contains(RuntimeComponentId::sqlite)||analyze_runtime_components(p).contains(RuntimeComponentId::http_client))return false;
    for(const auto& m:p.standard_modules)if(m=="filesystem")return false;
    bool found=false;for(const auto& st:p.statements)if(!light_json_stmt_ok(st.get(),found))return false;return found;
}
void emit_light_json_runtime(std::ostringstream& o,const MinimalRuntimeFeatures& f){
    auto base=f;base.strings=false;emit_minimal_runtime(o,base);
    o << "#include \"json.h\"\n#include <string>\n#include <vector>\n#include <stdexcept>\n#include <charconv>\n#include <cctype>\n#include <utility>\n#include <functional>\n";
    o << R"STRUT_JSON_BASE(
struct strut_string {
    std::string v;
    strut_string() = default; strut_string(const char* s):v(s){} strut_string(std::string s):v(std::move(s)){}
    bool starts_with(const strut_string& s) const { return v.size()>=s.v.size() && v.compare(0,s.v.size(),s.v)==0; }
    bool ends_with(const strut_string& s) const { return v.size()>=s.v.size() && v.compare(v.size()-s.v.size(),s.v.size(),s.v)==0; }
    bool contains(const strut_string& s) const { return v.find(s.v)!=std::string::npos; }
    strut_string trim() const { auto b=v.begin(),e=v.end();while(b!=e&&std::isspace((unsigned char)*b))++b;while(e!=b&&std::isspace((unsigned char)*(e-1)))--e;return std::string(b,e); }
    strut_string replace(const strut_string& from,const strut_string& to) const { std::string r=v;if(from.v.empty())return r;std::size_t p=0;while((p=r.find(from.v,p))!=std::string::npos){r.replace(p,from.v.size(),to.v);p+=to.v.size();}return r; }
    std::vector<strut_string> split(const strut_string& sep) const { std::vector<strut_string> out;if(sep.v.empty()){for(char c:v)out.emplace_back(std::string(1,c));return out;}std::size_t p=0,n;while((n=v.find(sep.v,p))!=std::string::npos){out.emplace_back(v.substr(p,n-p));p=n+sep.v.size();}out.emplace_back(v.substr(p));return out; }
    strut_string substr(std::size_t p) const { return v.substr(p); } strut_string substr(std::size_t p,std::size_t n) const { return v.substr(p,n); }
    std::size_t size() const { return v.size(); } bool empty() const { return v.empty(); } char at(std::size_t i) const { return v.at(i); }
};
inline std::ostream& operator<<(std::ostream& o,const strut_string& s){return o<<s.v;}
inline strut_string operator+(const strut_string&a,const strut_string&b){return a.v+b.v;}
inline bool operator==(const strut_string&a,const strut_string&b){return a.v==b.v;}
inline bool operator!=(const strut_string&a,const strut_string&b){return !(a==b);}
namespace std { template<> struct hash<strut_string> { size_t operator()(const strut_string& s) const noexcept { return std::hash<std::string>{}(s.v); } }; }
inline bool operator<(const strut_string&a,const strut_string&b){return a.v<b.v;}
inline bool strut_contains(const strut_string& s,const strut_string& x){return s.contains(x);}
)STRUT_JSON_BASE";
    o << R"STRUT_JSON(
inline json::Document strut_json_value(const json::Document& d){return d;}
inline json::Document strut_json_value(const strut_string& s){return json::Document(s.v);}
inline json::Document strut_json_value(const std::string& s){return json::Document(s);}
inline json::Document strut_json_value(bool v){return json::Document(v);}
inline json::Document strut_json_value(int v){return json::Document(v);}
inline json::Document strut_json_value(double v){return json::Document(v);}
inline json::Document strut_json_value(float v){return json::Document(static_cast<double>(v));}
inline bool strut_json_equal(const json::Document&a,const json::Document&b){if(a.type!=b.type)return false;switch(a.type){case json::Type::Null:return true;case json::Type::Boolean:return a.boolean==b.boolean;case json::Type::Number:return a.num==b.num;case json::Type::StrNumber:return a.string==b.string;case json::Type::String:return a.string==b.string;case json::Type::Array:if(a.array.size()!=b.array.size())return false;for(std::size_t i=0;i<a.array.size();++i)if(!strut_json_equal(a.array[i],b.array[i]))return false;return true;case json::Type::Object:if(a.object.size()!=b.object.size())return false;for(const auto& kv:a.object){if(!b.has(kv.first)||!strut_json_equal(kv.second,b[kv.first]))return false;}return true;}return false;}
namespace json { inline bool operator==(const Document&a,const Document&b){return strut_json_equal(a,b);} inline std::ostream& operator<<(std::ostream&o,const Document&d){return o<<d.dump();} }
inline json::Document strut_json_parse(const strut_string& s){json::Document d;json::ParseDiagnostic diag;if(!json::Document::parse(s.v,d,diag))throw std::runtime_error("json.parse: "+diag.message+" at "+std::to_string(diag.line)+":"+std::to_string(diag.column));return d;}
inline void strut_json_compact_write(std::string& out,const json::Document& d){switch(d.type){case json::Type::Null:out+="null";break;case json::Type::Boolean:out+=d.boolean?"true":"false";break;case json::Type::Number:{char b[64];auto [p,e]=std::to_chars(b,b+64,d.num);if(e!=std::errc{})throw std::runtime_error("json.stringify number formatting failed");out.append(b,p);break;}case json::Type::StrNumber:out+=d.string;break;case json::Type::String:out+='"';json::Document::append_escaped_string(out,d.string);out+='"';break;case json::Type::Array:out+='[';for(std::size_t i=0;i<d.array.size();++i){if(i)out+=',';strut_json_compact_write(out,d.array[i]);}out+=']';break;case json::Type::Object:out+='{';for(std::size_t i=0;i<d.object.size();++i){if(i)out+=',';out+='"';json::Document::append_escaped_string(out,d.object[i].first);out+="\":";strut_json_compact_write(out,d.object[i].second);}out+='}';break;}}
inline strut_string strut_json_stringify(const json::Document& d){std::string out;strut_json_compact_write(out,d);return out;}
inline strut_string strut_json_pretty(const json::Document& d){return d.dump(2);}

)STRUT_JSON";
}

bool light_async_type_ok(const std::string& t){
    return t.find("string")==std::string::npos && t.find("json")==std::string::npos &&
           t.find("channel<")==std::string::npos && t!="mutex" && t!="thread" &&
           t!="istream" && t!="ostream" && t!="sstream" && t!="ifstream" && t!="ofstream" &&
           t!="process" && t!="tcp_socket" && t!="tcp_listener" && t!="tls_stream" &&
           t.find("http_")==std::string::npos && t!="sqlite_db";
}
bool light_async_expr_ok(const IRExpr* e){
    if(!e)return true;
    if(e->kind==IRExpr::Kind::string_literal||e->kind==IRExpr::Kind::json_object||!light_async_type_ok(e->type_name))return false;
    if(e->kind==IRExpr::Kind::identifier){
        static const char* heavy[]={"input","istream","ostream","sstream","ifstream","ofstream","env","set_env","unset_env","exec","exec_shell","process","pipe_exec","thread","mutex","http_server","sqlite_open","embed_file","embed_dir"};
        for(const char* h:heavy)if(e->text==h)return false;
    }
    if(!light_async_expr_ok(e->left.get())||!light_async_expr_ok(e->right.get())||!light_async_expr_ok(e->lambda_expression.get()))return false;
    for(const auto& a:e->arguments)if(!light_async_expr_ok(a.get()))return false;
    return true;
}
bool light_async_stmt_ok(const IRStmt* st,bool& found){
    if(!st)return true;
    if(st->is_async)found=true;
    if(!light_async_type_ok(st->type_name)||!light_async_type_ok(st->return_type))return false;
    for(const auto& p:st->parameters)if(!light_async_type_ok(p.type.name))return false;
    if(!light_async_expr_ok(st->value.get())||!light_async_expr_ok(st->target.get())||!light_async_expr_ok(st->condition.get())||!light_async_expr_ok(st->increment.get()))return false;
    if(st->initializer&&!light_async_stmt_ok(st->initializer.get(),found))return false;
    for(const auto& c:st->body)if(!light_async_stmt_ok(c.get(),found))return false;
    for(const auto& c:st->else_body)if(!light_async_stmt_ok(c.get(),found))return false;
    return true;
}
bool program_uses_light_async_runtime(const IRProgram& p){
    if(!p.standard_modules.empty())return false;
    bool found=false;for(const auto& st:p.statements)if(!light_async_stmt_ok(st.get(),found))return false;return found;
}
void emit_light_async_runtime(std::ostringstream& o,const MinimalRuntimeFeatures& f){
    emit_minimal_runtime(o,f);
    o << "#include <thread>\n#include <mutex>\n#include <condition_variable>\n#include <queue>\n#include <future>\n#include <functional>\n#include <memory>\n#include <type_traits>\n#include <utility>\n#include <vector>\n";
    o << R"STRUT_ASYNC(
class strut_executor {
public:
    strut_executor(){auto n=std::thread::hardware_concurrency();if(n<2)n=2;for(unsigned i=0;i<n;++i)workers_.emplace_back([this]{worker();});}
    ~strut_executor(){{std::lock_guard<std::mutex> g(m_);stopping_=true;}cv_.notify_all();for(auto& t:workers_)if(t.joinable())t.join();}
    template<class F> auto submit(F&& f)->std::future<std::invoke_result_t<F>>{using R=std::invoke_result_t<F>;auto task=std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));auto fut=task->get_future();{std::lock_guard<std::mutex> g(m_);q_.emplace([task]{(*task)();});}cv_.notify_one();return fut;}
private:
    void worker(){for(;;){std::function<void()> job;{std::unique_lock<std::mutex> l(m_);cv_.wait(l,[this]{return stopping_||!q_.empty();});if(stopping_&&q_.empty())return;job=std::move(q_.front());q_.pop();}job();}}
    std::vector<std::thread> workers_;std::queue<std::function<void()>> q_;std::mutex m_;std::condition_variable cv_;bool stopping_=false;
};
inline strut_executor& strut_global_executor(){static strut_executor ex;return ex;}
template<class T> class strut_future { public: strut_future()=default; explicit strut_future(std::future<T>&& f):f_(std::move(f)){} T get(){return f_.get();} bool valid() const{return f_.valid();} private: std::future<T> f_; };
template<> class strut_future<void> { public: strut_future()=default; explicit strut_future(std::future<void>&& f):f_(std::move(f)){} void get(){f_.get();} bool valid() const{return f_.valid();} private: std::future<void> f_; };
template<class F> auto strut_async(F&& f)->strut_future<std::invoke_result_t<F>>{using R=std::invoke_result_t<F>;return strut_future<R>(strut_global_executor().submit(std::forward<F>(f)));}
template<class T> T strut_await(strut_future<T>& f){return f.get();}
template<class T> T strut_await(strut_future<T>&& f){return f.get();}
inline void strut_await(strut_future<void>& f){f.get();}
inline void strut_await(strut_future<void>&& f){f.get();}

)STRUT_ASYNC";
}



bool expr_uses_async_http_client(const IRExpr* e){
    if(!e)return false;
    if(e->kind==IRExpr::Kind::identifier&&(e->text=="http_get_async"||e->text=="http_request_async"||e->text.rfind("tls_",0)==0))return true;
    if(expr_uses_async_http_client(e->left.get())||expr_uses_async_http_client(e->right.get())||expr_uses_async_http_client(e->lambda_expression.get()))return true;
    for(const auto& a:e->arguments)if(expr_uses_async_http_client(a.get()))return true;
    for(const auto& st:e->lambda_body){if(st->is_async||expr_uses_async_http_client(st->value.get())||expr_uses_async_http_client(st->condition.get()))return true;}
    return false;
}
bool stmt_uses_async_http_client(const IRStmt* s){if(!s)return false;if(s->is_async||expr_uses_async_http_client(s->value.get())||expr_uses_async_http_client(s->target.get())||expr_uses_async_http_client(s->condition.get())||expr_uses_async_http_client(s->increment.get()))return true;if(s->initializer&&stmt_uses_async_http_client(s->initializer.get()))return true;for(const auto& c:s->body)if(stmt_uses_async_http_client(c.get()))return true;for(const auto& c:s->else_body)if(stmt_uses_async_http_client(c.get()))return true;return false;}
bool stmt_uses_non_http_client_runtime(const IRStmt* s);
bool expr_uses_non_http_client_runtime(const IRExpr* e){
    if(!e)return false;
    if(e->kind==IRExpr::Kind::identifier){
        static const char* helpers[]={"input","istream","ostream","sstream","ifstream","ofstream","exists","is_file","is_dir","file_size","modified","make_dir","remove","remove_all","copy","move","touch","ls","walk","cwd","cd","absolute","canonical","parent","filename","extension","stem","join_path","read_file","read_bytes","write_file","append_file","env","set_env","unset_env","now_ms","unix_ms","sleep_ms","exec","exec_shell","process","pipe_exec","thread","mutex","tcp_connect","tcp_connect_async","tcp_listen","tls_connect","http_server","http_text","http_html","http_json_response","sqlite_open","embed_file","embed_dir","new","ptr","ref","weak"};
        for(const char* helper:helpers)if(e->text==helper)return true;
    }
    if(expr_uses_non_http_client_runtime(e->left.get())||expr_uses_non_http_client_runtime(e->right.get())||expr_uses_non_http_client_runtime(e->lambda_expression.get()))return true;
    for(const auto& a:e->arguments)if(expr_uses_non_http_client_runtime(a.get()))return true;
    for(const auto& st:e->lambda_body)if(stmt_uses_non_http_client_runtime(st.get()))return true;
    return false;
}
bool stmt_uses_non_http_client_runtime(const IRStmt* s){if(!s)return false;if(expr_uses_non_http_client_runtime(s->value.get())||expr_uses_non_http_client_runtime(s->target.get())||expr_uses_non_http_client_runtime(s->condition.get())||expr_uses_non_http_client_runtime(s->increment.get()))return true;if(s->initializer&&stmt_uses_non_http_client_runtime(s->initializer.get()))return true;for(const auto& c:s->body)if(stmt_uses_non_http_client_runtime(c.get()))return true;for(const auto& c:s->else_body)if(stmt_uses_non_http_client_runtime(c.get()))return true;for(const auto& c:s->switch_cases){if(expr_uses_non_http_client_runtime(c.value.get()))return true;for(const auto& st:c.body)if(stmt_uses_non_http_client_runtime(st.get()))return true;}for(const auto& c:s->catches)for(const auto& st:c.body)if(stmt_uses_non_http_client_runtime(st.get()))return true;return false;}
bool program_uses_light_http_client_runtime(const IRProgram& p){
    if(!analyze_runtime_components(p).contains(RuntimeComponentId::http_client)||analyze_runtime_components(p).contains(RuntimeComponentId::sqlite))return false;
    for(const auto& m:p.standard_modules)if(m=="filesystem")return false;
    for(const auto& st:p.statements)if(stmt_uses_async_http_client(st.get())||stmt_uses_non_http_client_runtime(st.get()))return false;
    return true;
}
void emit_light_http_client_runtime(std::ostringstream& o,const MinimalRuntimeFeatures& f){
    emit_light_json_runtime(o,f);
    o << "#define STRUT_USE_CURL 1\n#include <curl/curl.h>\n#include <unordered_map>\n";
    o << R"STRHTTPCLI(
struct strut_checked_error : std::runtime_error { std::string type; std::string message; std::int32_t code=0; strut_checked_error(std::string t,const std::string& m,std::int32_t c=0):std::runtime_error(m),type(std::move(t)),message(m),code(c){} };
struct strut_curl_global{strut_curl_global(){if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)throw strut_checked_error("HttpError","libcurl global initialization failed");}~strut_curl_global(){curl_global_cleanup();}};
inline void strut_curl_init(){static strut_curl_global g;(void)g;}
struct strut_http_response {
    std::int32_t status=0; strut_string body; std::unordered_map<strut_string,strut_string> headers;
    json::Document json() const { json::Document d;json::ParseDiagnostic diag;if(!json::Document::parse(body.v,d,diag))throw strut_checked_error("HttpError","response body is not valid JSON");return d; }
};
inline size_t strut_http_write_cb(char* p,size_t size,size_t nmemb,void* u){auto* body=static_cast<std::string*>(u);body->append(p,size*nmemb);return size*nmemb;}
inline size_t strut_http_header_cb(char* p,size_t size,size_t nmemb,void* u){const size_t n=size*nmemb;std::string line(p,n);auto* headers=static_cast<std::unordered_map<strut_string,strut_string>*>(u);auto colon=line.find(':');if(colon!=std::string::npos){std::string k=line.substr(0,colon),v=line.substr(colon+1);while(!v.empty()&&(v.front()==' '||v.front()=='\t'))v.erase(v.begin());while(!v.empty()&&(v.back()=='\r'||v.back()=='\n'||v.back()==' '||v.back()=='\t'))v.pop_back();(*headers)[strut_string(k)]=strut_string(v);}return n;}
inline strut_http_response http_request(const strut_string& method,const strut_string& url,const json::Document& options){strut_curl_init();CURL* c=curl_easy_init();if(!c)throw strut_checked_error("HttpError","curl_easy_init failed");strut_http_response response;curl_slist* list=nullptr;std::string body;long timeout=30000;bool follow=true;if(options.type!=json::Type::Null&&options.type!=json::Type::Object){curl_easy_cleanup(c);throw strut_checked_error("HttpError","HTTP options must be JSON object or null");}if(options.type==json::Type::Object){if(options.has("timeout_ms")&&options["timeout_ms"].type==json::Type::Number)timeout=static_cast<long>(options["timeout_ms"].num);if(options.has("follow_redirects")&&options["follow_redirects"].type==json::Type::Boolean)follow=options["follow_redirects"].boolean;if(options.has("body")){const auto& b=options["body"];body=b.type==json::Type::String?b.string:b.dump();}if(options.has("headers")){const auto& h=options["headers"];if(h.type!=json::Type::Object){curl_easy_cleanup(c);throw strut_checked_error("HttpError","headers must be JSON object");}for(const auto& kv:h.object){if(kv.second.type!=json::Type::String){curl_easy_cleanup(c);throw strut_checked_error("HttpError","header values must be strings");}const std::string line=kv.first+": "+kv.second.string;list=curl_slist_append(list,line.c_str());}}}
curl_easy_setopt(c,CURLOPT_URL,url.v.c_str());curl_easy_setopt(c,CURLOPT_CUSTOMREQUEST,method.v.c_str());curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,follow?1L:0L);curl_easy_setopt(c,CURLOPT_MAXREDIRS,10L);curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,timeout);curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,strut_http_write_cb);curl_easy_setopt(c,CURLOPT_WRITEDATA,&response.body.v);curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,strut_http_header_cb);curl_easy_setopt(c,CURLOPT_HEADERDATA,&response.headers);if(list)curl_easy_setopt(c,CURLOPT_HTTPHEADER,list);if(!body.empty()){curl_easy_setopt(c,CURLOPT_POSTFIELDS,body.data());curl_easy_setopt(c,CURLOPT_POSTFIELDSIZE,static_cast<long>(body.size()));}auto rc=curl_easy_perform(c);if(rc==CURLE_OK){long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);response.status=static_cast<std::int32_t>(status);}if(list)curl_slist_free_all(list);curl_easy_cleanup(c);if(rc!=CURLE_OK)throw strut_checked_error("HttpError",curl_easy_strerror(rc));return response;}
inline strut_http_response http_request(const strut_string& method,const strut_string& url){return http_request(method,url,json::Document(nullptr));}
inline strut_http_response http_get(const strut_string& url){return http_request(strut_string("GET"),url);}
inline strut_http_response http_get_ca(const strut_string& url,const strut_string& ca_file){strut_curl_init();CURL* c=curl_easy_init();if(!c)throw strut_checked_error("HttpError","curl_easy_init failed");strut_http_response response;curl_easy_setopt(c,CURLOPT_URL,url.v.c_str());curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_CAINFO,ca_file.v.c_str());curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,30000L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,strut_http_write_cb);curl_easy_setopt(c,CURLOPT_WRITEDATA,&response.body.v);curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,strut_http_header_cb);curl_easy_setopt(c,CURLOPT_HEADERDATA,&response.headers);auto rc=curl_easy_perform(c);if(rc==CURLE_OK){long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);response.status=static_cast<std::int32_t>(status);}curl_easy_cleanup(c);if(rc!=CURLE_OK)throw strut_checked_error("HttpError",curl_easy_strerror(rc));return response;}
inline json::Document http_get_json(const strut_string& url){return http_get(url).json();}
)STRHTTPCLI";
}

void emit_http_server_lifecycle(std::ostringstream& o,bool async_handlers,bool tls){
    if(tls)o<<"#define STRUT_USE_SERVER_TLS 1\n#include <openssl/ssl.h>\n#include <openssl/err.h>\n";
    o << R"STRUT_SERVER(
#include <atomic>
#include <csignal>
class strut_http_server {
public:
    using handler=std::function<strut_server_response(strut_server_request)>;
    strut_http_server():s_(std::make_shared<state>()){}
    void get(const strut_string& path,handler h) const{add_route("GET",path,std::move(h));}
    void post(const strut_string& path,handler h) const{add_route("POST",path,std::move(h));}
)STRUT_SERVER";
    if(async_handlers)o << R"STRUT_SERVER(
    void get_async(const strut_string& path,std::function<strut_future<strut_server_response>(strut_server_request)> h) const{get(path,[h=std::move(h)](strut_server_request r){return strut_await(h(std::move(r)));});}
    void post_async(const strut_string& path,std::function<strut_future<strut_server_response>(strut_server_request)> h) const{post(path,[h=std::move(h)](strut_server_request r){return strut_await(h(std::move(r)));});}
)STRUT_SERVER";
    o << R"STRUT_SERVER(
    void serve_static(const strut_string& prefix,const std::unordered_map<strut_string,strut_string>& files,const strut_string& fallback=strut_string()) const{require_stopped("configure static files");s_->static_prefix=prefix.v;s_->static_files=files;s_->static_fallback=fallback.v;}
    void timeouts(std::int32_t read_ms,std::int32_t write_ms,std::int32_t idle_ms,std::int32_t shutdown_ms) const{require_stopped("configure timeouts");if(read_ms<=0||write_ms<=0||idle_ms<=0||shutdown_ms<0)throw strut_checked_error("NetworkError","HTTP timeouts must be positive (shutdown may be zero)");s_->read_timeout_ms=read_ms;s_->write_timeout_ms=write_ms;s_->idle_timeout_ms=idle_ms;s_->shutdown_timeout_ms=shutdown_ms;}
    void limits(std::int64_t body_bytes,std::int64_t header_bytes,std::int32_t header_count,std::int32_t connections) const{require_stopped("configure limits");if(body_bytes<0||header_bytes<1024||header_count<=0||connections<=0)throw strut_checked_error("NetworkError","invalid HTTP server limits");s_->max_body_bytes=static_cast<std::size_t>(body_bytes);s_->max_header_bytes=static_cast<std::size_t>(header_bytes);s_->max_header_count=header_count;s_->max_connections=connections;}
    bool running() const{return s_->running.load();}
    void stop() const{auto s=s_;if(!s->running.load())return;s->stopping.store(true);s->listener.close();std::unique_lock<std::mutex> lock(s->mutex);if(!s->cv.wait_for(lock,std::chrono::milliseconds(s->shutdown_timeout_ms),[&]{return s->active==0;})){for(auto& socket:s->active_sockets)socket.close();}}
    void listen(const strut_string& host,std::int32_t port,std::int32_t max_requests=0) const{
#ifndef _WIN32
        std::signal(SIGPIPE,SIG_IGN);
#endif
        auto s=s_;bool expected=false;if(!s->running.compare_exchange_strong(expected,true))throw strut_checked_error("NetworkError","HTTP server is already running");
        {std::lock_guard<std::mutex> lock(s->mutex);if(s->active!=0){s->running.store(false);throw strut_checked_error("NetworkError","HTTP server shutdown is still in progress");}}
        s->stopping.store(false);try{s->listener=tcp_listen(host,port);}catch(...){s->running.store(false);throw;}
        std::vector<std::thread> workers;std::int32_t served=0;
        try{while(!s->stopping.load()&&(max_requests<=0||served<max_requests)){
            strut_tcp_socket socket;try{socket=s->listener.accept();}catch(...){if(s->stopping.load())break;throw;}
            bool saturated=false;{std::lock_guard<std::mutex> lock(s->mutex);saturated=s->active>=s->max_connections;if(!saturated){++s->active;s->active_sockets.push_back(socket);}}
            if(saturated){send_error(socket,503,"Service Unavailable");continue;}
            workers.emplace_back([s,socket]() mutable{serve_one(s,socket);{std::lock_guard<std::mutex> lock(s->mutex);--s->active;}s->cv.notify_all();});++served;
        }}catch(...){s->stopping.store(true);s->listener.close();finish_workers(s,workers);s->running.store(false);throw;}
        s->stopping.store(true);s->listener.close();finish_workers(s,workers);s->running.store(false);s->stopping.store(false);
    }
)STRUT_SERVER";
    if(tls)o << R"STRUT_SERVER(
private:
    class tls_socket{public:tls_socket(strut_tcp_socket socket,SSL* ssl):socket_(std::move(socket)),ssl_(ssl){}tls_socket(const tls_socket&)=delete;tls_socket& operator=(const tls_socket&)=delete;tls_socket(tls_socket&& other) noexcept:socket_(std::move(other.socket_)),ssl_(other.ssl_){other.ssl_=nullptr;}~tls_socket(){close();}strut_socket_handle native_handle() const{return socket_.native_handle();}strut_string read(std::int64_t max_bytes=4096){if(max_bytes<=0)return {};std::string out(static_cast<std::size_t>(max_bytes),'\0');int n=SSL_read(ssl_,out.data(),static_cast<int>(out.size()));if(n<=0)throw strut_checked_error("TlsError","TLS request read failed");out.resize(static_cast<std::size_t>(n));return strut_string(std::move(out));}void write(const strut_string& data){std::size_t offset=0;while(offset<data.v.size()){int n=SSL_write(ssl_,data.v.data()+offset,static_cast<int>(data.v.size()-offset));if(n<=0)throw strut_checked_error("TlsError","TLS response write failed");offset+=static_cast<std::size_t>(n);}}void close(){if(ssl_){SSL_shutdown(ssl_);SSL_free(ssl_);ssl_=nullptr;}socket_.close();}private:strut_tcp_socket socket_;SSL* ssl_=nullptr;};
public:
    void listen_tls(const strut_string& host,std::int32_t port,const strut_string& certificate,const strut_string& private_key,std::int32_t max_requests=0) const{
#ifndef _WIN32
        std::signal(SIGPIPE,SIG_IGN);
#endif
        SSL_CTX* raw=SSL_CTX_new(TLS_server_method());if(!raw)throw strut_checked_error("TlsError","unable to initialize the TLS server context");std::shared_ptr<SSL_CTX> context(raw,SSL_CTX_free);SSL_CTX_set_min_proto_version(raw,TLS1_2_VERSION);
        if(SSL_CTX_use_certificate_chain_file(raw,certificate.v.c_str())!=1)throw strut_checked_error("TlsError","unable to load TLS certificate chain from '"+certificate.v+"'");
        if(SSL_CTX_use_PrivateKey_file(raw,private_key.v.c_str(),SSL_FILETYPE_PEM)!=1)throw strut_checked_error("TlsError","unable to load TLS private key from '"+private_key.v+"'");
        if(SSL_CTX_check_private_key(raw)!=1)throw strut_checked_error("TlsError","TLS certificate and private key do not match");
        auto s=s_;bool expected=false;if(!s->running.compare_exchange_strong(expected,true))throw strut_checked_error("NetworkError","HTTP server is already running");{std::lock_guard<std::mutex> lock(s->mutex);if(s->active!=0){s->running.store(false);throw strut_checked_error("NetworkError","HTTP server shutdown is still in progress");}}s->stopping.store(false);try{s->listener=tcp_listen(host,port);}catch(...){s->running.store(false);throw;}
        std::vector<std::thread> workers;std::int32_t served=0;try{while(!s->stopping.load()&&(max_requests<=0||served<max_requests)){strut_tcp_socket socket;try{socket=s->listener.accept();}catch(...){if(s->stopping.load())break;throw;}bool saturated=false;{std::lock_guard<std::mutex> lock(s->mutex);saturated=s->active>=s->max_connections;if(!saturated){++s->active;s->active_sockets.push_back(socket);}}if(saturated){socket.close();continue;}workers.emplace_back([s,socket,context]() mutable{SSL* ssl=SSL_new(context.get());if(ssl){SSL_set_fd(ssl,static_cast<int>(socket.native_handle()));if(SSL_accept(ssl)==1){tls_socket secure(std::move(socket),ssl);serve_one(s,std::move(secure));ssl=nullptr;}if(ssl)SSL_free(ssl);}socket.close();{std::lock_guard<std::mutex> lock(s->mutex);--s->active;}s->cv.notify_all();});++served;}}catch(...){s->stopping.store(true);s->listener.close();finish_workers(s,workers);s->running.store(false);throw;}s->stopping.store(true);s->listener.close();finish_workers(s,workers);s->running.store(false);s->stopping.store(false);
    }
)STRUT_SERVER";
    o << R"STRUT_SERVER(
private:
    struct route{std::string method,path;handler fn;};
    struct state{std::vector<route> routes;std::string static_prefix,static_fallback;std::unordered_map<strut_string,strut_string> static_files;std::atomic<bool> running{false},stopping{false};strut_tcp_listener listener;std::mutex mutex;std::condition_variable cv;std::int32_t active=0;std::vector<strut_tcp_socket> active_sockets;std::int32_t read_timeout_ms=30000,write_timeout_ms=30000,idle_timeout_ms=5000,shutdown_timeout_ms=5000,max_header_count=100,max_connections=1024;std::size_t max_body_bytes=1024*1024,max_header_bytes=64*1024;};
    std::shared_ptr<state> s_;
    void require_stopped(const char* action) const{if(s_->running.load())throw strut_checked_error("NetworkError",std::string("cannot ")+action+" while HTTP server is running");}
    void add_route(const char* method,const strut_string& path,handler h) const{require_stopped("register routes");s_->routes.push_back({method,path.v,std::move(h)});}
    static void finish_workers(const std::shared_ptr<state>& s,std::vector<std::thread>& workers){bool drained;{std::unique_lock<std::mutex> lock(s->mutex);drained=s->cv.wait_for(lock,std::chrono::milliseconds(s->shutdown_timeout_ms),[&]{return s->active==0;});if(!drained)for(auto& socket:s->active_sockets)socket.close();s->active_sockets.clear();}for(auto& worker:workers){if(!worker.joinable())continue;if(drained)worker.join();else worker.detach();}}
    template<class Socket> static void set_socket_timeouts(const Socket& socket,std::int32_t read_ms,std::int32_t write_ms){
#ifdef _WIN32
        DWORD read=static_cast<DWORD>(read_ms),write=static_cast<DWORD>(write_ms);setsockopt(socket.native_handle(),SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&read),sizeof(read));setsockopt(socket.native_handle(),SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&write),sizeof(write));
#else
        timeval read{read_ms/1000,(read_ms%1000)*1000},write{write_ms/1000,(write_ms%1000)*1000};setsockopt(socket.native_handle(),SOL_SOCKET,SO_RCVTIMEO,&read,sizeof(read));setsockopt(socket.native_handle(),SOL_SOCKET,SO_SNDTIMEO,&write,sizeof(write));
#endif
    }
    template<class Socket> static void send_error(Socket& socket,std::int32_t status,const char* message){try{std::string body=message;std::ostringstream out;out<<"HTTP/1.1 "<<status<<' '<<message<<"\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: "<<body.size()<<"\r\nConnection: close\r\n\r\n"<<body;socket.write(strut_string(out.str()));socket.close();}catch(...){socket.close();}}
    static strut_string mime(const std::string& p){auto dot=p.rfind('.');auto e=dot==std::string::npos?std::string():p.substr(dot);if(e==".html")return "text/html; charset=utf-8";if(e==".css")return "text/css; charset=utf-8";if(e==".js")return "application/javascript";if(e==".json")return "application/json";if(e==".svg")return "image/svg+xml";if(e==".png")return "image/png";return "application/octet-stream";}
    static std::string etag(const std::string& data){std::uint64_t h=1469598103934665603ull;for(unsigned char c:data){h^=c;h*=1099511628211ull;}std::ostringstream out;out<<'"'<<std::hex<<h<<'"';return out.str();}
    template<class Socket> static void serve_one(const std::shared_ptr<state>& s,Socket socket){
        try{set_socket_timeouts(socket,std::min(s->read_timeout_ms,s->idle_timeout_ms),s->write_timeout_ms);std::string raw;std::size_t header_end=std::string::npos;for(;;){auto chunk=socket.read(4096).v;if(chunk.empty()){send_error(socket,400,"Bad Request");return;}raw+=chunk;if(raw.size()>s->max_header_bytes){send_error(socket,431,"Request Header Fields Too Large");return;}header_end=raw.find("\r\n\r\n");if(header_end!=std::string::npos)break;}
            strut_server_request req;auto line_end=raw.find("\r\n");if(line_end==std::string::npos){send_error(socket,400,"Bad Request");return;}std::istringstream first(raw.substr(0,line_end));std::string target,version,extra;if(!(first>>req.method.v>>target>>version)||(first>>extra)||version.rfind("HTTP/",0)!=0){send_error(socket,400,"Bad Request");return;}auto q=target.find('?');req.path=strut_string(target.substr(0,q));if(q!=std::string::npos)strut_parse_query(target.substr(q+1),req.query);
            std::size_t p=line_end+2,content_len=0;std::int32_t count=0;while(p<header_end){auto e=raw.find("\r\n",p);if(e==std::string::npos||e>header_end){send_error(socket,400,"Bad Request");return;}auto line=raw.substr(p,e-p);auto colon=line.find(':');if(colon==std::string::npos||++count>s->max_header_count){send_error(socket,count>s->max_header_count?431:400,count>s->max_header_count?"Request Header Fields Too Large":"Bad Request");return;}auto key=strut_trim_ascii(line.substr(0,colon)),value=strut_trim_ascii(line.substr(colon+1));req.headers[strut_string(key)]=strut_string(value);if(key=="Content-Length"){if(value.empty()||!std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isdigit(c); })){send_error(socket,400,"Bad Request");return;}content_len=static_cast<std::size_t>(std::strtoull(value.c_str(),nullptr,10));if(content_len>s->max_body_bytes){send_error(socket,413,"Payload Too Large");return;}}p=e+2;}
            req.body=strut_string(raw.substr(header_end+4));if(req.body.v.size()>content_len)req.body.v.resize(content_len);while(req.body.v.size()<content_len){auto more=socket.read(static_cast<std::int64_t>(content_len-req.body.v.size())).v;if(more.empty()){send_error(socket,400,"Bad Request");return;}req.body.v+=more;}
            strut_server_response response;bool found=false,method_mismatch=false;for(auto& route:s->routes){req.params.clear();if(!strut_route_match(route.path,req.path.v,req.params))continue;if(route.method!=req.method.v){method_mismatch=true;continue;}try{response=route.fn(req);}catch(const std::exception&){send_error(socket,500,"Internal Server Error");return;}catch(...){send_error(socket,500,"Internal Server Error");return;}found=true;break;}
            if(!found&&!s->static_files.empty()&&req.method.v=="GET"){std::string key=req.path.v;if(!s->static_prefix.empty()&&key.rfind(s->static_prefix,0)==0)key=key.substr(s->static_prefix.size());while(!key.empty()&&key.front()=='/')key.erase(key.begin());if(key.empty())key="index.html";if(key.find("..")!=std::string::npos){response.status=400;response.body="Bad Request";found=true;}else{auto it=s->static_files.find(strut_string(key));if(it==s->static_files.end()&&!s->static_fallback.empty())it=s->static_files.find(strut_string(s->static_fallback));if(it!=s->static_files.end()){response.status=200;response.body=it->second;response.content_type=mime(key);response.headers[strut_string("ETag")]=strut_string(etag(response.body.v));response.headers[strut_string("Cache-Control")]=strut_string("public, max-age=0, must-revalidate");found=true;}}}
            if(!found){response.status=method_mismatch?405:404;response.body=method_mismatch?"Method Not Allowed":"Not Found";}std::ostringstream out;out<<"HTTP/1.1 "<<response.status<<' '<<(response.status==200?"OK":response.status==404?"Not Found":response.status==405?"Method Not Allowed":"Response")<<"\r\nContent-Type: "<<response.content_type.v<<"\r\nContent-Length: "<<response.body.v.size()<<"\r\nConnection: close\r\n";for(auto& header:response.headers)out<<header.first.v<<": "<<header.second.v<<"\r\n";out<<"\r\n"<<response.body.v;socket.write(strut_string(out.str()));socket.close();
        }catch(...){socket.close();}
    }
};
)STRUT_SERVER";
}

bool light_http_expr_ok(const IRExpr* e,bool& found){
    if(!e)return true;
    const auto& t=e->type_name;
    if(t.find("http_")!=std::string::npos)found=true;
    if(e->lambda_async||t.find("json")!=std::string::npos||t.find("sqlite_db")!=std::string::npos||t.find("process")!=std::string::npos||t.find("tls_")!=std::string::npos)return false;
    if(e->kind==IRExpr::Kind::identifier){
        if(e->text=="http_server"||e->text=="http_text"||e->text=="http_html")found=true;
        static const char* heavy[]={"http_json_response","http_get","http_request","http_get_json","http_get_async","http_request_async","sqlite_open","exec","exec_shell","process","pipe_exec","embed_file","embed_dir"};
        for(const char* h:heavy)if(e->text==h)return false;
    }
    if(e->kind==IRExpr::Kind::member&&(e->text=="get_async"||e->text=="post_async"||e->text=="listen_tls"||e->text=="json"))return false;
    if(!light_http_expr_ok(e->left.get(),found)||!light_http_expr_ok(e->right.get(),found)||!light_http_expr_ok(e->lambda_expression.get(),found))return false;
    for(const auto& a:e->arguments)if(!light_http_expr_ok(a.get(),found))return false;
    for(const auto& st:e->lambda_body){
        if(st->is_async)return false;
        if(!light_http_expr_ok(st->value.get(),found)||!light_http_expr_ok(st->condition.get(),found))return false;
    }
    return true;
}
bool light_http_stmt_ok(const IRStmt* st,bool& found){
    if(!st)return true;
    if(st->is_async||st->type_name.find("json")!=std::string::npos||st->return_type.find("json")!=std::string::npos)return false;
    if(st->type_name.find("http_")!=std::string::npos||st->return_type.find("http_")!=std::string::npos)found=true;
    for(const auto& p:st->parameters){if(p.type.name.find("json")!=std::string::npos)return false;if(p.type.name.find("http_")!=std::string::npos)found=true;}
    if(!light_http_expr_ok(st->value.get(),found)||!light_http_expr_ok(st->target.get(),found)||!light_http_expr_ok(st->condition.get(),found)||!light_http_expr_ok(st->increment.get(),found))return false;
    if(st->initializer&&!light_http_stmt_ok(st->initializer.get(),found))return false;
    for(const auto& c:st->body)if(!light_http_stmt_ok(c.get(),found))return false;
    for(const auto& c:st->else_body)if(!light_http_stmt_ok(c.get(),found))return false;
    for(const auto& c:st->catches)for(const auto& x:c.body)if(!light_http_stmt_ok(x.get(),found))return false;
    return true;
}
bool program_uses_light_http_runtime(const IRProgram& p){
    const auto components=analyze_runtime_components(p);
    if(components.contains(RuntimeComponentId::sqlite)||components.contains(RuntimeComponentId::http_client)||components.contains(RuntimeComponentId::time)||components.contains(RuntimeComponentId::filesystem)||components.contains(RuntimeComponentId::process)||components.contains(RuntimeComponentId::async))return false;
    for(const auto& m:p.standard_modules)if(m=="filesystem")return false;
    bool found=false;for(const auto& st:p.statements)if(!light_http_stmt_ok(st.get(),found))return false;return found;
}
void emit_light_http_runtime(std::ostringstream& o,const MinimalRuntimeFeatures& f){
    auto base=f;base.strings=false;emit_minimal_runtime(o,base);
    o << "#include <string>\n#include <vector>\n#include <unordered_map>\n#include <functional>\n#include <memory>\n#include <sstream>\n#include <stdexcept>\n#include <utility>\n#include <cstdlib>\n#include <algorithm>\n#include <cctype>\n#include <atomic>\n#include <thread>\n#include <mutex>\n#include <condition_variable>\n#include <chrono>\n";
    o << "#ifdef _WIN32\n#include <winsock2.h>\n#include <ws2tcpip.h>\n#else\n#include <sys/types.h>\n#include <sys/socket.h>\n#include <netdb.h>\n#include <unistd.h>\n#endif\n";
    o << R"STRUT_HTTP(
struct strut_checked_error : std::runtime_error { std::string type; std::string message; std::int32_t code=0; strut_checked_error(std::string t,const std::string& m,std::int32_t c=0):std::runtime_error(m),type(std::move(t)),message(m),code(c){} };
struct strut_string {
    std::string v;
    strut_string()=default;strut_string(const char* s):v(s){}strut_string(std::string s):v(std::move(s)){}
    std::size_t size() const{return v.size();}bool empty() const{return v.empty();}
};
inline std::ostream& operator<<(std::ostream& o,const strut_string& s){return o<<s.v;}
inline bool operator==(const strut_string&a,const strut_string&b){return a.v==b.v;}
inline bool operator!=(const strut_string&a,const strut_string&b){return !(a==b);}
inline bool operator<(const strut_string&a,const strut_string&b){return a.v<b.v;}
namespace std { template<> struct hash<strut_string>{size_t operator()(const strut_string& s) const noexcept{return std::hash<std::string>{}(s.v);}}; }
#ifdef _WIN32
using strut_socket_handle=SOCKET; constexpr strut_socket_handle strut_invalid_socket=INVALID_SOCKET;
inline void strut_socket_close(strut_socket_handle h){if(h!=strut_invalid_socket){shutdown(h,SD_BOTH);closesocket(h);}}
struct strut_winsock_runtime{strut_winsock_runtime(){WSADATA d{};if(WSAStartup(MAKEWORD(2,2),&d)!=0)throw strut_checked_error("NetworkError","WSAStartup failed");}~strut_winsock_runtime(){WSACleanup();}};
inline void strut_socket_init(){static strut_winsock_runtime runtime;(void)runtime;}
#else
using strut_socket_handle=int; constexpr strut_socket_handle strut_invalid_socket=-1;
inline void strut_socket_close(strut_socket_handle h){if(h!=strut_invalid_socket){::shutdown(h,SHUT_RDWR);::close(h);}}
inline void strut_socket_init(){}
#endif
struct strut_socket_state{strut_socket_handle handle=strut_invalid_socket;~strut_socket_state(){strut_socket_close(handle);}};
class strut_tcp_socket {
public:
    strut_tcp_socket():s_(std::make_shared<strut_socket_state>()){} explicit strut_tcp_socket(strut_socket_handle h):s_(std::make_shared<strut_socket_state>()){s_->handle=h;}
    bool is_open() const{return s_&&s_->handle!=strut_invalid_socket;}void close(){if(is_open()){strut_socket_close(s_->handle);s_->handle=strut_invalid_socket;}}
    void write(const strut_string& data){if(!is_open())throw strut_checked_error("NetworkError","write on closed socket");std::size_t off=0;while(off<data.v.size()){
#ifdef _WIN32
        int n=::send(s_->handle,data.v.data()+off,static_cast<int>(data.v.size()-off),0);
#else
        ssize_t n=::send(s_->handle,data.v.data()+off,data.v.size()-off,0);
#endif
        if(n<=0)throw strut_checked_error("NetworkError","socket write failed");off+=static_cast<std::size_t>(n);}}
    strut_string read(std::int64_t max_bytes=4096){if(!is_open())throw strut_checked_error("NetworkError","read on closed socket");if(max_bytes<=0)return {};std::string out(static_cast<std::size_t>(max_bytes),'\0');
#ifdef _WIN32
        int n=::recv(s_->handle,out.data(),static_cast<int>(out.size()),0);
#else
        ssize_t n=::recv(s_->handle,out.data(),out.size(),0);
#endif
        if(n<0)throw strut_checked_error("NetworkError","socket read failed");out.resize(static_cast<std::size_t>(n));return strut_string(std::move(out));}
    strut_socket_handle native_handle() const{return s_->handle;}
private:std::shared_ptr<strut_socket_state> s_;
};
class strut_tcp_listener {
public:
    strut_tcp_listener():s_(std::make_shared<strut_socket_state>()){} explicit strut_tcp_listener(strut_socket_handle h):s_(std::make_shared<strut_socket_state>()){s_->handle=h;}
    bool is_open() const{return s_&&s_->handle!=strut_invalid_socket;}void close(){if(is_open()){strut_socket_close(s_->handle);s_->handle=strut_invalid_socket;}}
    strut_tcp_socket accept(){if(!is_open())throw strut_checked_error("NetworkError","accept on closed listener");auto h=::accept(s_->handle,nullptr,nullptr);if(h==strut_invalid_socket)throw strut_checked_error("NetworkError","TCP accept failed");return strut_tcp_socket(h);}
private:std::shared_ptr<strut_socket_state> s_;
};
inline strut_tcp_listener tcp_listen(const strut_string& host,std::int32_t port,std::int32_t backlog=128){strut_socket_init();if(port<1||port>65535)throw strut_checked_error("NetworkError","invalid TCP port");addrinfo hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_flags=AI_PASSIVE;addrinfo* list=nullptr;const std::string service=std::to_string(port);const char* node=host.v.empty()?nullptr:host.v.c_str();if(getaddrinfo(node,service.c_str(),&hints,&list)!=0)throw strut_checked_error("NetworkError","listen address resolution failed");strut_socket_handle h=strut_invalid_socket;for(addrinfo* p=list;p;p=p->ai_next){h=::socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(h==strut_invalid_socket)continue;int yes=1;setsockopt(h,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&yes),sizeof(yes));if(::bind(h,p->ai_addr,static_cast<int>(p->ai_addrlen))==0&&::listen(h,backlog)==0)break;strut_socket_close(h);h=strut_invalid_socket;}freeaddrinfo(list);if(h==strut_invalid_socket)throw strut_checked_error("NetworkError","TCP listen failed: address unavailable or port already in use");return strut_tcp_listener(h);}
struct strut_server_request {strut_string method,path,body;std::unordered_map<strut_string,strut_string> headers,query,params;};
struct strut_server_response {std::int32_t status=200;strut_string body;strut_string content_type="text/plain; charset=utf-8";std::unordered_map<strut_string,strut_string> headers;};
inline strut_server_response strut_http_text(const strut_string& s){return {200,s,"text/plain; charset=utf-8",{}};}
inline strut_server_response strut_http_html(const strut_string& s){return {200,s,"text/html; charset=utf-8",{}};}
inline std::string strut_trim_ascii(std::string s){while(!s.empty()&&(s.back()=='\r'||s.back()==' '||s.back()=='\t'))s.pop_back();std::size_t i=0;while(i<s.size()&&(s[i]==' '||s[i]=='\t'))++i;return s.substr(i);}
inline void strut_parse_query(const std::string& raw,std::unordered_map<strut_string,strut_string>& out){std::size_t p=0;while(p<=raw.size()){auto amp=raw.find('&',p);auto part=raw.substr(p,amp==std::string::npos?std::string::npos:amp-p);auto eq=part.find('=');out[strut_string(part.substr(0,eq))]=strut_string(eq==std::string::npos?"":part.substr(eq+1));if(amp==std::string::npos)break;p=amp+1;}}
inline bool strut_route_match(const std::string& pattern,const std::string& path,std::unordered_map<strut_string,strut_string>& params){std::stringstream a(pattern),b(path);std::string x,y;while(true){bool ax=static_cast<bool>(std::getline(a,x,'/')),by=static_cast<bool>(std::getline(b,y,'/'));if(!ax||!by)return ax==by;if(x.empty()&&y.empty())continue;if(!x.empty()&&x[0]==':')params[strut_string(x.substr(1))]=strut_string(y);else if(x!=y)return false;}}
class strut_http_server_legacy {
public:
    using handler=std::function<strut_server_response(strut_server_request)>;
    void get(const strut_string& path,handler h){routes_.push_back({"GET",path.v,std::move(h)});}void post(const strut_string& path,handler h){routes_.push_back({"POST",path.v,std::move(h)});}
    void serve_static(const strut_string& prefix,const std::unordered_map<strut_string,strut_string>& files,const strut_string& fallback=strut_string()){static_prefix_=prefix.v;static_files_=files;static_fallback_=fallback.v;}
    void listen(const strut_string& host,std::int32_t port,std::int32_t max_requests=0){auto l=tcp_listen(host,port);std::int32_t served=0;while(max_requests<=0||served<max_requests){auto c=l.accept();serve_one(c);++served;}l.close();}
private:
    struct route{std::string method,path;handler fn;};std::vector<route> routes_;std::string static_prefix_,static_fallback_;std::unordered_map<strut_string,strut_string> static_files_;
    static strut_string mime(const std::string& p){auto dot=p.rfind('.');auto e=dot==std::string::npos?std::string():p.substr(dot);if(e==".html")return "text/html; charset=utf-8";if(e==".css")return "text/css; charset=utf-8";if(e==".js")return "application/javascript";if(e==".json")return "application/json";if(e==".svg")return "image/svg+xml";if(e==".png")return "image/png";return "application/octet-stream";}
    static std::string etag(const std::string& data){std::uint64_t h=1469598103934665603ull;for(unsigned char c:data){h^=c;h*=1099511628211ull;}std::ostringstream o;o<<'"'<<std::hex<<h<<'"';return o.str();}
    void serve_one(strut_tcp_socket& sock){std::string raw;for(;;){auto chunk=sock.read(4096).v;if(chunk.empty())break;raw+=chunk;if(raw.find("\r\n\r\n")!=std::string::npos)break;}strut_server_request req;auto line_end=raw.find("\r\n");if(line_end==std::string::npos)return;std::istringstream first(raw.substr(0,line_end));std::string target,version;first>>req.method.v>>target>>version;auto q=target.find('?');req.path=strut_string(target.substr(0,q));if(q!=std::string::npos)strut_parse_query(target.substr(q+1),req.query);auto header_end=raw.find("\r\n\r\n");std::size_t p=line_end+2,content_len=0;while(p<header_end){auto e=raw.find("\r\n",p);auto ln=raw.substr(p,e-p);auto colon=ln.find(':');if(colon!=std::string::npos){auto k=strut_trim_ascii(ln.substr(0,colon));auto v=strut_trim_ascii(ln.substr(colon+1));req.headers[strut_string(k)]=strut_string(v);if(k=="Content-Length")content_len=static_cast<std::size_t>(std::strtoull(v.c_str(),nullptr,10));}p=e+2;}if(header_end!=std::string::npos){req.body=strut_string(raw.substr(header_end+4));while(req.body.v.size()<content_len){auto more=sock.read(static_cast<std::int64_t>(content_len-req.body.v.size())).v;if(more.empty())break;req.body.v+=more;}}strut_server_response res;bool found=false;for(auto& r:routes_){if(r.method!=req.method.v)continue;req.params.clear();if(strut_route_match(r.path,req.path.v,req.params)){res=r.fn(req);found=true;break;}}if(!found&&!static_files_.empty()&&req.method.v=="GET"){std::string key=req.path.v;if(!static_prefix_.empty()&&key.rfind(static_prefix_,0)==0)key=key.substr(static_prefix_.size());while(!key.empty()&&key.front()=='/')key.erase(key.begin());if(key.empty())key="index.html";if(key.find("..")!=std::string::npos){res.status=400;res.body="Bad Request";found=true;}else{auto it=static_files_.find(strut_string(key));if(it==static_files_.end()&&!static_fallback_.empty())it=static_files_.find(strut_string(static_fallback_));if(it!=static_files_.end()){res.status=200;res.body=it->second;res.content_type=mime(key);res.headers[strut_string("ETag")]=strut_string(etag(res.body.v));res.headers[strut_string("Cache-Control")]=strut_string("public, max-age=0, must-revalidate");found=true;}}}if(!found){res.status=404;res.body="Not Found";}std::ostringstream out;out<<"HTTP/1.1 "<<res.status<<" "<<(res.status==200?"OK":res.status==404?"Not Found":"Response")<<"\r\nContent-Type: "<<res.content_type.v<<"\r\nContent-Length: "<<res.body.v.size()<<"\r\nConnection: close\r\n";for(auto& h:res.headers)out<<h.first.v<<": "<<h.second.v<<"\r\n";out<<"\r\n"<<res.body.v;sock.write(strut_string(out.str()));sock.close();}
};
)STRUT_HTTP";
    emit_http_server_lifecycle(o,false,false);
}

bool light_filesystem_type_ok(const std::string& t){
    static const char* heavy[]={"json","istream","ostream","sstream","ifstream","ofstream","process","thread","mutex","future<","channel<","tcp_socket","tcp_listener","tls_stream","http_","sqlite_db"};
    for(const char* h:heavy)if(t.find(h)!=std::string::npos)return false;
    return true;
}
bool light_filesystem_expr_ok(const IRExpr* e){
    if(!e)return true;
    if(e->kind==IRExpr::Kind::json_object||!light_filesystem_type_ok(e->type_name)||e->lambda_async)return false;
    if(e->kind==IRExpr::Kind::identifier){
        static const char* heavy[]={"input","istream","ostream","sstream","ifstream","ofstream","env","set_env","unset_env","now_ms","unix_ms","sleep_ms","exec","exec_shell","process","pipe_exec","thread","mutex","http_server","http_text","http_html","http_json_response","sqlite_open","embed_file","embed_dir"};
        for(const char* h:heavy)if(e->text==h)return false;
    }
    if(!light_filesystem_expr_ok(e->left.get())||!light_filesystem_expr_ok(e->right.get())||!light_filesystem_expr_ok(e->lambda_expression.get()))return false;
    for(const auto& a:e->arguments)if(!light_filesystem_expr_ok(a.get()))return false;
    for(const auto& st:e->lambda_body){
        if(st->value&&!light_filesystem_expr_ok(st->value.get()))return false;
        if(st->condition&&!light_filesystem_expr_ok(st->condition.get()))return false;
    }
    return true;
}
bool light_filesystem_stmt_ok(const IRStmt* st){
    if(!st)return true;
    if(st->is_async||!light_filesystem_type_ok(st->type_name)||!light_filesystem_type_ok(st->return_type))return false;
    for(const auto& p:st->parameters)if(!light_filesystem_type_ok(p.type.name))return false;
    if(!light_filesystem_expr_ok(st->value.get())||!light_filesystem_expr_ok(st->target.get())||!light_filesystem_expr_ok(st->condition.get())||!light_filesystem_expr_ok(st->increment.get()))return false;
    if(st->initializer&&!light_filesystem_stmt_ok(st->initializer.get()))return false;
    for(const auto& c:st->body)if(!light_filesystem_stmt_ok(c.get()))return false;
    for(const auto& c:st->else_body)if(!light_filesystem_stmt_ok(c.get()))return false;
    for(const auto& c:st->catches)for(const auto& x:c.body)if(!light_filesystem_stmt_ok(x.get()))return false;
    return true;
}
bool program_uses_light_filesystem_runtime(const IRProgram& p){
    bool filesystem=false;
    for(const auto& m:p.standard_modules){
        if(m=="filesystem")filesystem=true;
        else if(m!="vector"&&m!="deque"&&m!="list"&&m!="map"&&m!="set"&&m!="ordered_map"&&m!="ordered_set"&&m!="queue"&&m!="stack"&&m!="priority_queue"&&m!="tuple")return false;
    }
    if(!filesystem||analyze_runtime_components(p).contains(RuntimeComponentId::http_client)||analyze_runtime_components(p).contains(RuntimeComponentId::sqlite))return false;
    for(const auto& st:p.statements)if(!light_filesystem_stmt_ok(st.get()))return false;
    return true;
}
struct FilesystemRuntimeFeatures {
    bool metadata=false, mutate=false, list=false, path=false, io=false;
};
void collect_filesystem_expr_features(const IRExpr* e,FilesystemRuntimeFeatures& f){
    if(!e)return;
    if(e->kind==IRExpr::Kind::call&&e->left&&e->left->kind==IRExpr::Kind::identifier){
        const auto& n=e->left->text;
        if(n=="exists"||n=="is_file"||n=="is_dir"||n=="file_size"||n=="modified")f.metadata=true;
        else if(n=="make_dir"||n=="remove"||n=="remove_all"||n=="copy"||n=="move"||n=="touch")f.mutate=true;
        else if(n=="ls"||n=="walk")f.list=true;
        else if(n=="cwd"||n=="cd"||n=="absolute"||n=="canonical"||n=="parent"||n=="filename"||n=="extension"||n=="stem"||n=="join_path")f.path=true;
        else if(n=="read_file"||n=="read_bytes"||n=="write_file"||n=="append_file")f.io=true;
    }
    collect_filesystem_expr_features(e->left.get(),f);collect_filesystem_expr_features(e->right.get(),f);collect_filesystem_expr_features(e->lambda_expression.get(),f);
    for(const auto& a:e->arguments)collect_filesystem_expr_features(a.get(),f);
    for(const auto& st:e->lambda_body){collect_filesystem_expr_features(st->value.get(),f);collect_filesystem_expr_features(st->condition.get(),f);}
}
void collect_filesystem_stmt_features(const IRStmt* st,FilesystemRuntimeFeatures& f){
    if(!st)return;
    collect_filesystem_expr_features(st->value.get(),f);collect_filesystem_expr_features(st->target.get(),f);collect_filesystem_expr_features(st->condition.get(),f);collect_filesystem_expr_features(st->increment.get(),f);
    if(st->initializer)collect_filesystem_stmt_features(st->initializer.get(),f);
    for(const auto& c:st->body)collect_filesystem_stmt_features(c.get(),f);
    for(const auto& c:st->else_body)collect_filesystem_stmt_features(c.get(),f);
    for(const auto& c:st->catches)for(const auto& x:c.body)collect_filesystem_stmt_features(x.get(),f);
}
FilesystemRuntimeFeatures filesystem_features(const IRProgram& p){FilesystemRuntimeFeatures f;for(const auto& st:p.statements)collect_filesystem_stmt_features(st.get(),f);return f;}
void emit_light_filesystem_runtime(std::ostringstream& o,const MinimalRuntimeFeatures& f,const FilesystemRuntimeFeatures& fs){
    auto base=f;base.strings=false;emit_minimal_runtime(o,base);
    o << "#include <string>\n#include <filesystem>\n#include <stdexcept>\n#include <utility>\n";
    if(fs.metadata)o << "#include <chrono>\n";
    if(fs.mutate||fs.list)o << "#include <algorithm>\n#include <vector>\n";
    if(fs.io||fs.mutate)o << "#include <fstream>\n";
    if(fs.io)o << "#include <vector>\n";
    o << R"STRUT_FS_BASE(
struct strut_checked_error : std::runtime_error { std::string type; std::string message; std::int32_t code=0; strut_checked_error(std::string t,const std::string& m,std::int32_t c=0):std::runtime_error(m),type(std::move(t)),message(m),code(c){} };
struct strut_string {
    std::string v;
    strut_string()=default;strut_string(const char* s):v(s){}strut_string(std::string s):v(std::move(s)){}
    bool starts_with(const strut_string& s) const{return v.size()>=s.v.size()&&v.compare(0,s.v.size(),s.v)==0;}
    bool ends_with(const strut_string& s) const{return v.size()>=s.v.size()&&v.compare(v.size()-s.v.size(),s.v.size(),s.v)==0;}
    bool contains(const strut_string& s) const{return v.find(s.v)!=std::string::npos;}
    std::size_t size() const{return v.size();}bool empty() const{return v.empty();}char at(std::size_t i) const{return v.at(i);}
};
inline std::ostream& operator<<(std::ostream& o,const strut_string& s){return o<<s.v;}
inline strut_string operator+(const strut_string&a,const strut_string&b){return a.v+b.v;}
inline bool operator==(const strut_string&a,const strut_string&b){return a.v==b.v;}
inline bool operator!=(const strut_string&a,const strut_string&b){return !(a==b);}
inline bool operator<(const strut_string&a,const strut_string&b){return a.v<b.v;}
namespace std { template<> struct hash<strut_string>{size_t operator()(const strut_string& s) const noexcept{return std::hash<std::string>{}(s.v);}}; }
inline std::filesystem::path strut_fs_path(const strut_string& s){return std::filesystem::path(s.v);}
inline void strut_fs_fail(const char* op,const std::error_code& ec){if(ec)throw strut_checked_error("FilesystemError",std::string(op)+": "+ec.message());}
)STRUT_FS_BASE";
    if(fs.metadata)o << R"STRUT_FS_META(
inline bool strut_fs_exists(const strut_string& p){std::error_code ec;bool v=std::filesystem::exists(strut_fs_path(p),ec);strut_fs_fail("exists",ec);return v;}
inline bool strut_fs_is_file(const strut_string& p){std::error_code ec;bool v=std::filesystem::is_regular_file(strut_fs_path(p),ec);strut_fs_fail("is_file",ec);return v;}
inline bool strut_fs_is_dir(const strut_string& p){std::error_code ec;bool v=std::filesystem::is_directory(strut_fs_path(p),ec);strut_fs_fail("is_dir",ec);return v;}
inline std::int64_t strut_fs_file_size(const strut_string& p){std::error_code ec;auto n=std::filesystem::file_size(strut_fs_path(p),ec);strut_fs_fail("file_size",ec);return static_cast<std::int64_t>(n);}
inline std::int64_t strut_fs_modified(const strut_string& p){std::error_code ec;auto t=std::filesystem::last_write_time(strut_fs_path(p),ec);strut_fs_fail("modified",ec);auto system_time=std::chrono::system_clock::now()+(t-decltype(t)::clock::now());return std::chrono::duration_cast<std::chrono::milliseconds>(system_time.time_since_epoch()).count();}
)STRUT_FS_META";
    if(fs.path)o << R"STRUT_FS_PATH(
inline strut_string strut_fs_cwd(){std::error_code ec;auto p=std::filesystem::current_path(ec);strut_fs_fail("cwd",ec);return p.string();}
inline void strut_fs_cd(const strut_string& p){std::error_code ec;std::filesystem::current_path(strut_fs_path(p),ec);strut_fs_fail("cd",ec);}
inline strut_string strut_fs_absolute(const strut_string& p){std::error_code ec;auto v=std::filesystem::absolute(strut_fs_path(p),ec);strut_fs_fail("absolute",ec);return v.lexically_normal().string();}
inline strut_string strut_fs_canonical(const strut_string& p){std::error_code ec;auto v=std::filesystem::canonical(strut_fs_path(p),ec);strut_fs_fail("canonical",ec);return v.string();}
inline strut_string strut_fs_parent(const strut_string& p){return strut_fs_path(p).parent_path().string();}
inline strut_string strut_fs_filename(const strut_string& p){return strut_fs_path(p).filename().string();}
inline strut_string strut_fs_extension(const strut_string& p){return strut_fs_path(p).extension().string();}
inline strut_string strut_fs_stem(const strut_string& p){return strut_fs_path(p).stem().string();}
template<class... Rest> inline strut_string strut_fs_join_path(const strut_string& a,const Rest&... rest){std::filesystem::path p=strut_fs_path(a);((p/=strut_fs_path(rest)),...);return p.lexically_normal().string();}
)STRUT_FS_PATH";
    if(fs.list)o << R"STRUT_FS_LIST(
inline std::vector<strut_string> strut_fs_ls(const strut_string& p){std::error_code ec;std::vector<strut_string> out;for(std::filesystem::directory_iterator it(strut_fs_path(p),ec),end;!ec&&it!=end;it.increment(ec))out.emplace_back(it->path().filename().string());strut_fs_fail("ls",ec);std::sort(out.begin(),out.end());return out;}
inline std::vector<strut_string> strut_fs_walk(const strut_string& p){std::error_code ec;std::vector<strut_string> out;auto root=strut_fs_path(p);for(std::filesystem::recursive_directory_iterator it(root,ec),end;!ec&&it!=end;it.increment(ec))out.emplace_back(it->path().lexically_relative(root).generic_string());strut_fs_fail("walk",ec);std::sort(out.begin(),out.end());return out;}
)STRUT_FS_LIST";
    if(fs.io)o << R"STRUT_FS_IO(
inline strut_string strut_fs_read_file(const strut_string& p){std::ifstream f(strut_fs_path(p),std::ios::binary|std::ios::ate);if(!f)throw strut_checked_error("FilesystemError","read_file: unable to open path");auto end=f.tellg();if(end<0)throw strut_checked_error("FilesystemError","read_file: unable to determine file size");std::string out(static_cast<std::size_t>(end),'\0');f.seekg(0,std::ios::beg);if(!out.empty()&&!f.read(out.data(),static_cast<std::streamsize>(out.size())))throw strut_checked_error("FilesystemError","read_file: short read");return strut_string(std::move(out));}
inline std::vector<std::uint8_t> strut_fs_read_bytes(const strut_string& p){std::ifstream f(strut_fs_path(p),std::ios::binary|std::ios::ate);if(!f)throw strut_checked_error("FilesystemError","read_bytes: unable to open path");auto end=f.tellg();if(end<0)throw strut_checked_error("FilesystemError","read_bytes: unable to determine file size");std::vector<std::uint8_t> out(static_cast<std::size_t>(end));f.seekg(0,std::ios::beg);if(!out.empty()&&!f.read(reinterpret_cast<char*>(out.data()),static_cast<std::streamsize>(out.size())))throw strut_checked_error("FilesystemError","read_bytes: short read");return out;}
inline void strut_fs_write_file(const strut_string& p,const strut_string& data){std::ofstream f(strut_fs_path(p),std::ios::binary|std::ios::trunc);if(!f||(!data.v.empty()&&!f.write(data.v.data(),static_cast<std::streamsize>(data.v.size()))))throw strut_checked_error("FilesystemError","write_file failed");}
inline void strut_fs_write_file(const strut_string& p,const std::vector<std::uint8_t>& data){std::ofstream f(strut_fs_path(p),std::ios::binary|std::ios::trunc);if(!f||(!data.empty()&&!f.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()))))throw strut_checked_error("FilesystemError","write_file failed");}
inline void strut_fs_append_file(const strut_string& p,const strut_string& data){std::ofstream f(strut_fs_path(p),std::ios::binary|std::ios::app);if(!f||(!data.v.empty()&&!f.write(data.v.data(),static_cast<std::streamsize>(data.v.size()))))throw strut_checked_error("FilesystemError","append_file failed");}
inline void strut_fs_append_file(const strut_string& p,const std::vector<std::uint8_t>& data){std::ofstream f(strut_fs_path(p),std::ios::binary|std::ios::app);if(!f||(!data.empty()&&!f.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()))))throw strut_checked_error("FilesystemError","append_file failed");}
)STRUT_FS_IO";
    if(fs.mutate)o << R"STRUT_FS_MUT(
inline void strut_fs_make_dir(const strut_string& p){std::error_code ec;std::filesystem::create_directories(strut_fs_path(p),ec);strut_fs_fail("make_dir",ec);}
inline bool strut_fs_has_wildcards(const strut_string& p){return p.v.find('*')!=std::string::npos||p.v.find('?')!=std::string::npos;}
inline bool strut_fs_wildcard_match(const std::string& pat,const std::string& text){std::size_t p=0,t=0,star=std::string::npos,mark=0;while(t<text.size()){if(p<pat.size()&&(pat[p]=='?'||pat[p]==text[t])){++p;++t;}else if(p<pat.size()&&pat[p]=='*'){star=p++;mark=t;}else if(star!=std::string::npos){p=star+1;t=++mark;}else return false;}while(p<pat.size()&&pat[p]=='*')++p;return p==pat.size();}
inline void strut_fs_glob_walk(const std::filesystem::path& cur,const std::vector<std::string>& parts,std::size_t i,std::vector<strut_string>& out){if(i==parts.size()){std::error_code ec;if(std::filesystem::exists(cur,ec)&&!ec)out.emplace_back(cur.string());return;}const auto& part=parts[i];if(part=="**"){strut_fs_glob_walk(cur,parts,i+1,out);std::error_code ec;for(std::filesystem::directory_iterator it(cur,ec),end;!ec&&it!=end;it.increment(ec))if(it->is_directory(ec)&&!ec)strut_fs_glob_walk(it->path(),parts,i,out);return;}if(part.find('*')==std::string::npos&&part.find('?')==std::string::npos){strut_fs_glob_walk(cur/part,parts,i+1,out);return;}std::error_code ec;for(std::filesystem::directory_iterator it(cur,ec),end;!ec&&it!=end;it.increment(ec))if(strut_fs_wildcard_match(part,it->path().filename().string()))strut_fs_glob_walk(it->path(),parts,i+1,out);}
inline std::vector<strut_string> strut_fs_expand(const strut_string& pattern){if(!strut_fs_has_wildcards(pattern))return {pattern};std::filesystem::path p=pattern.v;std::filesystem::path root=p.is_absolute()?p.root_path():std::filesystem::current_path();std::vector<std::string> parts;auto rel=p.is_absolute()?p.relative_path():p;for(const auto& x:rel)parts.push_back(x.string());std::vector<strut_string> out;strut_fs_glob_walk(root,parts,0,out);std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.v<b.v;});return out;}
template<class C> inline std::vector<strut_string> strut_fs_expand_all(const C& paths){std::vector<strut_string> out;for(const auto& p:paths){auto xs=strut_fs_expand(strut_string(p));out.insert(out.end(),xs.begin(),xs.end());}return out;}
inline void strut_fs_remove_exact(const strut_string& p){std::error_code ec;std::filesystem::remove(strut_fs_path(p),ec);strut_fs_fail("remove",ec);}inline void strut_fs_remove(const strut_string& p){for(const auto& x:strut_fs_expand(p))strut_fs_remove_exact(x);}template<class C> inline void strut_fs_remove(const C& ps){for(const auto& p:strut_fs_expand_all(ps))strut_fs_remove_exact(p);}
inline void strut_fs_remove_all_exact(const strut_string& p){std::error_code ec;std::filesystem::remove_all(strut_fs_path(p),ec);strut_fs_fail("remove_all",ec);}inline void strut_fs_remove_all(const strut_string& p){for(const auto& x:strut_fs_expand(p))strut_fs_remove_all_exact(x);}inline void strut_fs_remove_all(const std::vector<strut_string>& ps){for(const auto& p:strut_fs_expand_all(ps))strut_fs_remove_all_exact(p);}
inline void strut_fs_copy_exact(const strut_string&a,const strut_string&b){std::error_code ec;std::filesystem::copy(strut_fs_path(a),strut_fs_path(b),std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing,ec);strut_fs_fail("copy",ec);}inline void strut_fs_copy(const strut_string&a,const strut_string&b){if(!strut_fs_has_wildcards(a)){strut_fs_copy_exact(a,b);return;}auto ps=strut_fs_expand(a);if(ps.empty())throw strut_checked_error("FilesystemError","copy: wildcard matched no paths");auto d=strut_fs_path(b);std::error_code ec;if(!std::filesystem::is_directory(d,ec)||ec)throw strut_checked_error("FilesystemError","copy: wildcard destination must be an existing directory");for(const auto&p:ps)strut_fs_copy_exact(p,strut_string((d/strut_fs_path(p).filename()).string()));}template<class C> inline void strut_fs_copy(const C& ps,const strut_string&d0){auto d=strut_fs_path(d0);std::error_code ec;if(!std::filesystem::is_directory(d,ec)||ec)throw strut_checked_error("FilesystemError","copy: bulk destination must be an existing directory");auto xs=strut_fs_expand_all(ps);if(xs.empty()&&ps.begin()!=ps.end())throw strut_checked_error("FilesystemError","copy: wildcard matched no paths");for(const auto&p:xs)strut_fs_copy_exact(p,strut_string((d/strut_fs_path(p).filename()).string()));}template<class A,class B> inline void strut_fs_copy(const A&a0,const B&b0){auto a=a0.begin();auto ae=a0.end();auto b=b0.begin();auto be=b0.end();for(;a!=ae&&b!=be;++a,++b)strut_fs_copy(strut_string(*a),strut_string(*b));if(a!=ae||b!=be)throw strut_checked_error("FilesystemError","copy: paired collections must have equal lengths");}
inline void strut_fs_move_exact(const strut_string&a,const strut_string&b){std::error_code ec;std::filesystem::rename(strut_fs_path(a),strut_fs_path(b),ec);strut_fs_fail("move",ec);}inline void strut_fs_move(const strut_string&a,const strut_string&b){if(!strut_fs_has_wildcards(a)){strut_fs_move_exact(a,b);return;}auto ps=strut_fs_expand(a);if(ps.empty())throw strut_checked_error("FilesystemError","move: wildcard matched no paths");auto d=strut_fs_path(b);std::error_code ec;if(!std::filesystem::is_directory(d,ec)||ec)throw strut_checked_error("FilesystemError","move: wildcard destination must be an existing directory");for(const auto&p:ps)strut_fs_move_exact(p,strut_string((d/strut_fs_path(p).filename()).string()));}template<class C> inline void strut_fs_move(const C& ps,const strut_string&d0){auto d=strut_fs_path(d0);std::error_code ec;if(!std::filesystem::is_directory(d,ec)||ec)throw strut_checked_error("FilesystemError","move: bulk destination must be an existing directory");auto xs=strut_fs_expand_all(ps);if(xs.empty()&&ps.begin()!=ps.end())throw strut_checked_error("FilesystemError","move: wildcard matched no paths");for(const auto&p:xs)strut_fs_move_exact(p,strut_string((d/strut_fs_path(p).filename()).string()));}template<class A,class B> inline void strut_fs_move(const A&a0,const B&b0){auto a=a0.begin();auto ae=a0.end();auto b=b0.begin();auto be=b0.end();for(;a!=ae&&b!=be;++a,++b)strut_fs_move(strut_string(*a),strut_string(*b));if(a!=ae||b!=be)throw strut_checked_error("FilesystemError","move: paired collections must have equal lengths");}
inline void strut_fs_touch(const strut_string& p){std::ofstream f(strut_fs_path(p),std::ios::app|std::ios::binary);if(!f)throw strut_checked_error("FilesystemError","touch: unable to open path");}
)STRUT_FS_MUT";
}

void emit_entry_support(std::ostringstream& o){o<<R"CPP(
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
inline std::string strut_native_arg(const wchar_t* value){if(!value)return {};const int size=WideCharToMultiByte(CP_UTF8,0,value,-1,nullptr,0,nullptr,nullptr);if(size<=1)return {};std::string text(static_cast<std::size_t>(size),'\0');WideCharToMultiByte(CP_UTF8,0,value,-1,text.data(),size,nullptr,nullptr);text.pop_back();return text;}
#else
inline std::string strut_native_arg(const char* value){return value?value:"";}
#endif
)CPP";}

CodegenResult CppBackend::generate(const IRProgram& p) const {CodegenResult r;strut_codegen_source_path=p.source_path;std::ostringstream o;const auto components=analyze_runtime_components(p);if(!components.ok()){r.error=components.error;return r;}const auto has=[&](RuntimeComponentId id){return components.contains(id);};const bool complex=has(RuntimeComponentId::filesystem)||has(RuntimeComponentId::environment)||has(RuntimeComponentId::time)||has(RuntimeComponentId::process)||has(RuntimeComponentId::threading)||has(RuntimeComponentId::channels)||has(RuntimeComponentId::mutex)||has(RuntimeComponentId::async)||has(RuntimeComponentId::networking)||has(RuntimeComponentId::http_client)||has(RuntimeComponentId::http_server)||has(RuntimeComponentId::sqlite)||has(RuntimeComponentId::embedded_assets)||has(RuntimeComponentId::ffi);if(!complex&&program_uses_minimal_runtime(p)){emit_minimal_runtime(o,minimal_features(p));emit_entry_support(o);for(auto&s:p.statements)stmt(o,*s,0);r.cpp=o.str();return r;}if(has(RuntimeComponentId::threading)&&program_uses_light_thread_runtime(p)){emit_light_thread_runtime(o,minimal_features(p));emit_entry_support(o);for(auto&s:p.statements)stmt(o,*s,0);r.cpp=o.str();return r;}if(has(RuntimeComponentId::sqlite)&&program_uses_light_sqlite_runtime(p)){emit_light_sqlite_runtime(o,minimal_features(p));emit_entry_support(o);for(auto&s:p.statements)stmt(o,*s,0);r.cpp=o.str();return r;}if(has(RuntimeComponentId::http_client)&&program_uses_light_http_client_runtime(p)){emit_light_http_client_runtime(o,minimal_features(p));emit_entry_support(o);for(auto&s:p.statements)stmt(o,*s,0);r.cpp=o.str();return r;}if(has(RuntimeComponentId::json)&&program_uses_light_json_runtime(p)){emit_light_json_runtime(o,minimal_features(p));emit_entry_support(o);for(auto&s:p.statements)stmt(o,*s,0);r.cpp=o.str();return r;}if(has(RuntimeComponentId::async)&&program_uses_light_async_runtime(p)){emit_light_async_runtime(o,minimal_features(p));emit_entry_support(o);for(auto&s:p.statements)stmt(o,*s,0);r.cpp=o.str();return r;}if(has(RuntimeComponentId::http_server)&&program_uses_light_http_runtime(p)){emit_light_http_runtime(o,minimal_features(p));emit_entry_support(o);for(auto&s:p.statements)stmt(o,*s,0);r.cpp=o.str();return r;}if(has(RuntimeComponentId::filesystem)&&program_uses_light_filesystem_runtime(p)){emit_light_filesystem_runtime(o,minimal_features(p),filesystem_features(p));emit_entry_support(o);for(auto&s:p.statements)stmt(o,*s,0);r.cpp=o.str();return r;}const bool use_curl=has(RuntimeComponentId::http_client);const bool use_sqlite=has(RuntimeComponentId::sqlite);if(use_curl)o<<"#define STRUT_USE_CURL 1\n#include <curl/curl.h>\n";if(use_sqlite)o<<"#define STRUT_USE_SQLITE 1\n#include <sqlite3.h>\n";o<<"#include \"json.h\"\n#include <cstdint>\n#include <iostream>\n#include <string>\n#include <vector>\n#include <array>\n#include <map>\n#include <unordered_map>\n#include <set>\n#include <unordered_set>\n#include <stack>\n#include <deque>\n#include <list>\n#include <tuple>\n#include <stdexcept>\n#include <charconv>\n#include <algorithm>\n#include <cctype>\n#include <utility>\n#include <optional>\n#include <memory>\n#include <optional>\n#include <type_traits>\n#include <functional>\n#include <filesystem>\n#include <fstream>\n#include <sstream>\n#include <chrono>\n#include <thread>\n#include <mutex>\n#include <condition_variable>\n#include <queue>\n#include <future>\n#include <cerrno>\n#include <cstring>\n#ifdef _WIN32\n#include <windows.h>\n#include <dbghelp.h>\n#include <winsock2.h>\n#include <ws2tcpip.h>\n#else\n#include <sys/types.h>\n#include <sys/wait.h>\n#include <sys/socket.h>\n#include <netdb.h>\n#include <arpa/inet.h>\n#include <netinet/in.h>\n#include <unistd.h>\n#include <execinfo.h>\n#endif\n";
o<<R"CPP(
template<class T> class strut_ref {
public:
    strut_ref(T& value):p_(&value){}
    strut_ref(const strut_ref&)=default;
    template<class U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    strut_ref(const strut_ref<U>& other):p_(other.get()){}
    strut_ref& operator=(const strut_ref&)=delete;
    T& operator*() const { return *p_; }
    T* operator->() const { return p_; }
    T* get() const { return p_; }
private:
    T* p_;
};
template<class T> strut_ref<T> strut_make_ref(T& value){return strut_ref<T>(value);}
struct strut_checked_error : std::runtime_error { std::string type; std::string message; std::int32_t code=0; strut_checked_error(std::string t,const std::string& m,std::int32_t c=0):std::runtime_error(m),type(std::move(t)),message(m),code(c){} };
struct strut_null_t {
    template<class T> operator std::optional<T>() const { return std::nullopt; }
    template<class T> operator std::shared_ptr<T>() const { return {}; }
    template<class T> operator std::weak_ptr<T>() const { return {}; }
    template<class T> operator T*() const { return nullptr; }
};
[[maybe_unused]] constexpr strut_null_t strut_null{};
template<class T> bool operator==(const std::optional<T>& v,strut_null_t){return !v;}
template<class T> bool operator!=(const std::optional<T>& v,strut_null_t){return static_cast<bool>(v);}
template<class T> bool operator==(strut_null_t,const std::optional<T>& v){return !v;}
template<class T> bool operator!=(strut_null_t,const std::optional<T>& v){return static_cast<bool>(v);}
template<class T> bool operator==(const std::shared_ptr<T>& v,strut_null_t){return !v;}
template<class T> bool operator!=(const std::shared_ptr<T>& v,strut_null_t){return static_cast<bool>(v);}
template<class T> bool operator==(strut_null_t,const std::shared_ptr<T>& v){return !v;}
template<class T> bool operator!=(strut_null_t,const std::shared_ptr<T>& v){return static_cast<bool>(v);}
template<class T> std::shared_ptr<typename std::decay<T>::type> strut_ptr(T&& value){using U=typename std::decay<T>::type;return std::make_shared<U>(std::forward<T>(value));}
template<class T> std::weak_ptr<T> strut_weak(const std::shared_ptr<T>& value){return std::weak_ptr<T>(value);}
template<class T> T* strut_raw(const std::shared_ptr<T>& value){return value.get();}
inline void strut_print_stack_trace(){
#ifdef _WIN32
    void* frames[48]; const auto n=CaptureStackBackTrace(0,48,frames,nullptr); std::cerr<<"stack trace:\n"; for(unsigned short i=0;i<n;++i)std::cerr<<"  "<<frames[i]<<"\n";
#else
    void* frames[48]; const int n=backtrace(frames,48); std::cerr<<"stack trace:\n"; char** symbols=backtrace_symbols(frames,n); if(symbols){for(int i=0;i<n;++i)std::cerr<<"  "<<symbols[i]<<"\n";std::free(symbols);}
#endif
}
[[noreturn]] inline void strut_panic(const std::string& message){std::cerr<<"Strut panic: "<<message<<"\n";strut_print_stack_trace();throw std::runtime_error(message);}
template<class T> T& strut_deref(const std::shared_ptr<T>& value,const char* file,std::size_t line){if(!value) [[unlikely]] {std::cerr<<"at "<<file<<":"<<line<<"\n";strut_panic("null safe-pointer dereference");}return *value;}
template<class T,class F> auto strut_safe_member(const std::optional<T>& value,F f) -> std::optional<typename std::decay<decltype(f(*value))>::type> { if(!value)return std::nullopt; return f(*value); }
template<class T> T strut_coalesce(const std::optional<T>& value,T fallback){return value?*value:std::move(fallback);}

struct strut_string {
    std::string v;
    strut_string() = default; strut_string(const char* s):v(s){} strut_string(std::string s):v(std::move(s)){}
    bool starts_with(const strut_string& s) const { return v.size()>=s.v.size() && v.compare(0,s.v.size(),s.v)==0; }
    bool ends_with(const strut_string& s) const { return v.size()>=s.v.size() && v.compare(v.size()-s.v.size(),s.v.size(),s.v)==0; }
    bool contains(const strut_string& s) const { return v.find(s.v)!=std::string::npos; }
    strut_string trim() const { auto b=v.begin(),e=v.end();while(b!=e&&std::isspace((unsigned char)*b))++b;while(e!=b&&std::isspace((unsigned char)*(e-1)))--e;return std::string(b,e); }
    strut_string replace(const strut_string& from,const strut_string& to) const { std::string r=v;if(from.v.empty())return r;std::size_t p=0;while((p=r.find(from.v,p))!=std::string::npos){r.replace(p,from.v.size(),to.v);p+=to.v.size();}return r; }
    std::vector<strut_string> split(const strut_string& sep) const { std::vector<strut_string> out;if(sep.v.empty()){for(char c:v)out.emplace_back(std::string(1,c));return out;}std::size_t p=0,n;while((n=v.find(sep.v,p))!=std::string::npos){out.emplace_back(v.substr(p,n-p));p=n+sep.v.size();}out.emplace_back(v.substr(p));return out; }
    strut_string substr(std::size_t p) const { return v.substr(p); } strut_string substr(std::size_t p,std::size_t n) const { return v.substr(p,n); }
    std::size_t size() const { return v.size(); } bool empty() const { return v.empty(); } char at(std::size_t i) const { return v.at(i); }
};
inline std::ostream& operator<<(std::ostream& o,const strut_string& s){return o<<s.v;}
inline strut_string operator+(const strut_string&a,const strut_string&b){return a.v+b.v;}
inline bool operator==(const strut_string&a,const strut_string&b){return a.v==b.v;}
inline bool operator!=(const strut_string&a,const strut_string&b){return !(a==b);}
namespace std { template<> struct hash<strut_string> { size_t operator()(const strut_string& s) const noexcept { return std::hash<std::string>{}(s.v); } }; }
inline bool operator<(const strut_string&a,const strut_string&b){return a.v<b.v;}

class strut_ostream {
public:
    strut_ostream()=default; explicit strut_ostream(std::ostream& s):p_(&s){}
    template<class T> strut_ostream& write_value(const T& v){if(!p_)throw strut_checked_error("StreamError","output stream is not open");(*p_)<<v;return *this;}
    strut_ostream& write_value(std::int8_t v){return write_value(static_cast<std::int32_t>(v));}
    strut_ostream& write_value(std::uint8_t v){return write_value(static_cast<std::uint32_t>(v));}
    void write(const strut_string& s){write_value(s);}
    void flush(){if(p_)p_->flush();}
protected: std::ostream* p_=nullptr;
};
class strut_istream {
public:
    strut_istream()=default; explicit strut_istream(std::istream& s):p_(&s){}
    template<class T> strut_istream& read_value(T& v){if(!p_)throw strut_checked_error("StreamError","input stream is not open");(*p_)>>v;return *this;}
protected: std::istream* p_=nullptr;
};
class strut_ofstream : public strut_ostream {
public:
    strut_ofstream()=default; explicit strut_ofstream(const strut_string& path){open(path);} strut_ofstream(const strut_string& path,bool binary){open(path,binary);}
    void open(const strut_string& path,bool binary=false){close();auto mode=std::ios::out|(binary?std::ios::binary:std::ios::openmode(0));file_.open(path.v,mode);if(!file_)throw strut_checked_error("StreamError","ofstream.open failed");p_=&file_;}
    void close(){if(file_.is_open()){file_.close();if(file_.fail())throw strut_checked_error("StreamError","ofstream.close failed");}p_=nullptr;}
    bool is_open() const{return file_.is_open();}
    ~strut_ofstream(){if(file_.is_open())file_.close();}
private: std::ofstream file_;
};
class strut_ifstream : public strut_istream {
public:
    strut_ifstream()=default; explicit strut_ifstream(const strut_string& path){open(path);} strut_ifstream(const strut_string& path,bool binary){open(path,binary);}
    void open(const strut_string& path,bool binary=false){close();auto mode=std::ios::in|(binary?std::ios::binary:std::ios::openmode(0));file_.open(path.v,mode);if(!file_)throw strut_checked_error("StreamError","ifstream.open failed");p_=&file_;}
    void close(){if(file_.is_open())file_.close();p_=nullptr;}
    bool is_open() const{return file_.is_open();}
)CPP" << R"CPP(    strut_string read_all(){if(!file_.is_open())throw strut_checked_error("StreamError","ifstream.read_all on closed stream");std::ostringstream out;out<<file_.rdbuf();if(file_.bad())throw strut_checked_error("StreamError","ifstream.read_all failed");return strut_string(out.str());}
    ~strut_ifstream(){if(file_.is_open())file_.close();}
private: std::ifstream file_;
};
class strut_sstream : public strut_ostream {
public:
    strut_sstream(){p_=&stream_;}
    void write(const strut_string& s){stream_<<s.v;}
    strut_string str() const{return stream_.str();}
    strut_string read_all() const{return stream_.str();}
    bool is_open() const{return true;}
    void close() const{}
private: std::stringstream stream_;
};

struct strut_endl_t{};
[[maybe_unused]] static strut_endl_t endl{};
template<class T> strut_ostream& operator<<(strut_ostream& s,const T& value){return s.write_value(value);}
inline strut_ostream& operator<<(strut_ostream& s,strut_endl_t){s.write_value('\n');s.flush();return s;}
template<class T> strut_istream& operator>>(strut_istream& s,T& value){return s.read_value(value);}
inline strut_istream& operator>>(strut_istream& s,strut_string& value){std::string tmp;s.read_value(tmp);value=strut_string(tmp);return s;}
static strut_istream in{std::cin};
static strut_ostream out{std::cout};
static strut_ostream err{std::cerr};
inline strut_string strut_input(){std::string value;std::getline(std::cin,value);return value;}
template<class T> void strut_input(T& value){std::cin>>value;}
inline void strut_input(strut_string& value){std::string tmp;std::cin>>tmp;value=strut_string(tmp);}
inline json::Document strut_json_value(const json::Document& d){return d;}
inline json::Document strut_json_value(const strut_string& s){return json::Document(s.v);}
inline json::Document strut_json_value(const std::string& s){return json::Document(s);}
inline json::Document strut_json_value(bool v){return json::Document(v);}
inline json::Document strut_json_value(int v){return json::Document(v);}
inline json::Document strut_json_value(double v){return json::Document(v);}
inline json::Document strut_json_value(float v){return json::Document(static_cast<double>(v));}
inline bool strut_json_equal(const json::Document&a,const json::Document&b){if(a.type!=b.type)return false;switch(a.type){case json::Type::Null:return true;case json::Type::Boolean:return a.boolean==b.boolean;case json::Type::Number:return a.num==b.num;case json::Type::StrNumber:return a.string==b.string;case json::Type::String:return a.string==b.string;case json::Type::Array:if(a.array.size()!=b.array.size())return false;for(std::size_t i=0;i<a.array.size();++i)if(!strut_json_equal(a.array[i],b.array[i]))return false;return true;case json::Type::Object:if(a.object.size()!=b.object.size())return false;for(const auto& kv:a.object){if(!b.has(kv.first)||!strut_json_equal(kv.second,b[kv.first]))return false;}return true;}return false;}
namespace json { inline bool operator==(const Document&a,const Document&b){return strut_json_equal(a,b);} inline std::ostream& operator<<(std::ostream&o,const Document&d){return o<<d.dump();} }
inline json::Document strut_json_parse(const strut_string& s){json::Document d;json::ParseDiagnostic diag;if(!json::Document::parse(s.v,d,diag))throw std::runtime_error("json.parse: "+diag.message+" at "+std::to_string(diag.line)+":"+std::to_string(diag.column));return d;}
inline void strut_json_compact_write(std::string& out,const json::Document& d){switch(d.type){case json::Type::Null:out+="null";break;case json::Type::Boolean:out+=d.boolean?"true":"false";break;case json::Type::Number:{char b[64];auto [p,e]=std::to_chars(b,b+64,d.num);if(e!=std::errc{})throw std::runtime_error("json.stringify number formatting failed");out.append(b,p);break;}case json::Type::StrNumber:out+=d.string;break;case json::Type::String:out+='"';json::Document::append_escaped_string(out,d.string);out+='"';break;case json::Type::Array:out+='[';for(std::size_t i=0;i<d.array.size();++i){if(i)out+=',';strut_json_compact_write(out,d.array[i]);}out+=']';break;case json::Type::Object:out+='{';for(std::size_t i=0;i<d.object.size();++i){if(i)out+=',';out+='"';json::Document::append_escaped_string(out,d.object[i].first);out+="\":";strut_json_compact_write(out,d.object[i].second);}out+='}';break;}}
inline strut_string strut_json_stringify(const json::Document& d){std::string out;strut_json_compact_write(out,d);return out;}
inline strut_string strut_json_pretty(const json::Document& d){return d.dump(2);}

inline strut_string strut_join(const std::vector<strut_string>& xs,const strut_string& sep){std::string r;for(std::size_t i=0;i<xs.size();++i){if(i)r+=sep.v;r+=xs[i].v;}return r;}
inline std::int32_t strut_to_int(const strut_string& s){std::int32_t x{};auto [p,e]=std::from_chars(s.v.data(),s.v.data()+s.v.size(),x);if(e!=std::errc{}||p!=s.v.data()+s.v.size())throw std::runtime_error("invalid int");return x;}
inline float strut_to_double(const strut_string& s){float x{};auto [p,e]=std::from_chars(s.v.data(),s.v.data()+s.v.size(),x);if(e!=std::errc{}||p!=s.v.data()+s.v.size())throw std::runtime_error("invalid double");return x;}
template<class T> strut_string strut_to_string(T x){return std::to_string(x);}

template<class C,class F> auto strut_map(const C& xs,F f) -> std::vector<typename std::decay<decltype(f(*xs.begin()))>::type>{using R=typename std::decay<decltype(f(*xs.begin()))>::type;std::vector<R> out;out.reserve(xs.size());for(const auto& x:xs)out.push_back(f(x));return out;}
template<class C,class F> C strut_filter(const C& xs,F f){C out;for(const auto& x:xs)if(f(x))out.push_back(x);return out;}
template<class C,class F> auto strut_reduce(const C& xs,F f) -> typename C::value_type {if(xs.empty())throw std::runtime_error("reduce on empty collection");auto it=xs.begin();auto acc=*it++;for(;it!=xs.end();++it)acc=f(acc,*it);return acc;}
template<class C,class A,class F> A strut_reduce(const C& xs,A acc,F f){for(const auto& x:xs)acc=f(acc,x);return acc;}
template<class C,class F> bool strut_any(const C& xs,F f){for(const auto& x:xs)if(f(x))return true;return false;}
template<class C,class F> bool strut_all(const C& xs,F f){for(const auto& x:xs)if(!f(x))return false;return true;}
template<class C,class F> std::optional<typename C::value_type> strut_find(const C& xs,F f){for(const auto& x:xs)if(f(x))return x;return std::nullopt;}
template<class C,class F> std::size_t strut_count(const C& xs,F f){std::size_t n=0;for(const auto& x:xs)if(f(x))++n;return n;}
template<class C,class F> void strut_sort(C& xs,F f){std::sort(xs.begin(),xs.end(),f);}

template<class C,class F> auto strut_count_by(const C& xs,F f) -> std::unordered_map<typename std::decay<decltype(f(*xs.begin()))>::type,std::size_t>{using K=typename std::decay<decltype(f(*xs.begin()))>::type;std::unordered_map<K,std::size_t> out;for(const auto& x:xs)++out[f(x)];return out;}
template<class C,class F> auto strut_index_by(const C& xs,F f) -> std::unordered_map<typename std::decay<decltype(f(*xs.begin()))>::type,typename C::value_type>{using K=typename std::decay<decltype(f(*xs.begin()))>::type;std::unordered_map<K,typename C::value_type> out;for(const auto& x:xs){auto k=f(x);if(out.find(k)!=out.end())throw std::runtime_error("index_by duplicate key");out.emplace(std::move(k),x);}return out;}
template<class T> struct strut_partition_result{std::vector<T> matched;std::vector<T> unmatched;};
template<class C,class F> auto strut_partition(const C& xs,F f) -> strut_partition_result<typename C::value_type>{strut_partition_result<typename C::value_type> out;for(const auto& x:xs)(f(x)?out.matched:out.unmatched).push_back(x);return out;}
)CPP" << R"CPP(inline json::Document strut_json_pick(const json::Document& d,const std::vector<strut_string>& keys){json::Document out=json::Document::make_object();if(d.type!=json::Type::Object)return out;for(const auto& k:keys)if(d.has(k.v))out[k.v]=d[k.v];return out;}
inline json::Document strut_json_omit(const json::Document& d,const std::vector<strut_string>& keys){json::Document out=json::Document::make_object();if(d.type!=json::Type::Object)return out;for(const auto& kv:d.object){bool omit=false;for(const auto& k:keys)if(k.v==kv.first){omit=true;break;}if(!omit)out[kv.first]=kv.second;}return out;}
inline json::Document strut_json_merge_deep(const json::Document& a,const json::Document& b){if(a.type!=json::Type::Object||b.type!=json::Type::Object)return b;json::Document out=a;for(const auto& kv:b.object){if(out.has(kv.first)&&out[kv.first].type==json::Type::Object&&kv.second.type==json::Type::Object)out[kv.first]=strut_json_merge_deep(out[kv.first],kv.second);else out[kv.first]=kv.second;}return out;}
inline bool strut_contains(const strut_string& s,const strut_string& x){return s.contains(x);}
template<class M,class K> bool strut_contains(const M& m,const K& k){return m.find(k)!=m.end();}
template<class M,class K,class V> void strut_map_insert(M& m,K&& k,V&& v){m[std::forward<K>(k)]=std::forward<V>(v);}
inline std::filesystem::path strut_fs_path(const strut_string& s){return std::filesystem::path(s.v);}
inline void strut_fs_fail(const char* op,const std::error_code& ec){if(ec)throw strut_checked_error("FilesystemError",std::string(op)+": "+ec.message());}
inline bool strut_fs_exists(const strut_string& p){std::error_code ec;bool v=std::filesystem::exists(strut_fs_path(p),ec);strut_fs_fail("exists",ec);return v;}
inline bool strut_fs_is_file(const strut_string& p){std::error_code ec;bool v=std::filesystem::is_regular_file(strut_fs_path(p),ec);strut_fs_fail("is_file",ec);return v;}
inline bool strut_fs_is_dir(const strut_string& p){std::error_code ec;bool v=std::filesystem::is_directory(strut_fs_path(p),ec);strut_fs_fail("is_dir",ec);return v;}
inline std::int64_t strut_fs_file_size(const strut_string& p){std::error_code ec;auto n=std::filesystem::file_size(strut_fs_path(p),ec);strut_fs_fail("file_size",ec);return static_cast<std::int64_t>(n);}
inline std::int64_t strut_fs_modified(const strut_string& p){std::error_code ec;auto t=std::filesystem::last_write_time(strut_fs_path(p),ec);strut_fs_fail("modified",ec);auto system_time=std::chrono::system_clock::now()+(t-decltype(t)::clock::now());return std::chrono::duration_cast<std::chrono::milliseconds>(system_time.time_since_epoch()).count();}
inline void strut_fs_make_dir(const strut_string& p){std::error_code ec;std::filesystem::create_directories(strut_fs_path(p),ec);strut_fs_fail("make_dir",ec);}
inline bool strut_fs_has_wildcards(const strut_string& p){return p.v.find('*')!=std::string::npos||p.v.find('?')!=std::string::npos;}
inline bool strut_fs_wildcard_match(const std::string& pat,const std::string& text){std::size_t p=0,t=0,star=std::string::npos,mark=0;while(t<text.size()){if(p<pat.size()&&(pat[p]=='?'||pat[p]==text[t])){++p;++t;}else if(p<pat.size()&&pat[p]=='*'){star=p++;mark=t;}else if(star!=std::string::npos){p=star+1;t=++mark;}else return false;}while(p<pat.size()&&pat[p]=='*')++p;return p==pat.size();}
inline void strut_fs_glob_walk(const std::filesystem::path& cur,const std::vector<std::string>& parts,std::size_t i,std::vector<strut_string>& out){if(i==parts.size()){std::error_code ec;if(std::filesystem::exists(cur,ec)&&!ec)out.emplace_back(cur.string());return;}const auto& part=parts[i];if(part=="**"){strut_fs_glob_walk(cur,parts,i+1,out);std::error_code ec;for(std::filesystem::directory_iterator it(cur,ec),end;!ec&&it!=end;it.increment(ec))if(it->is_directory(ec)&&!ec)strut_fs_glob_walk(it->path(),parts,i,out);return;}if(part.find('*')==std::string::npos&&part.find('?')==std::string::npos){strut_fs_glob_walk(cur/part,parts,i+1,out);return;}std::error_code ec;for(std::filesystem::directory_iterator it(cur,ec),end;!ec&&it!=end;it.increment(ec))if(strut_fs_wildcard_match(part,it->path().filename().string()))strut_fs_glob_walk(it->path(),parts,i+1,out);}
inline std::vector<strut_string> strut_fs_expand(const strut_string& pattern){if(!strut_fs_has_wildcards(pattern))return {pattern};std::filesystem::path p=pattern.v;std::filesystem::path root=p.is_absolute()?p.root_path():std::filesystem::current_path();std::vector<std::string> parts;auto rel=p.is_absolute()?p.relative_path():p;for(const auto& x:rel)parts.push_back(x.string());std::vector<strut_string> out;strut_fs_glob_walk(root,parts,0,out);std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.v<b.v;});return out;}
template<class C> inline std::vector<strut_string> strut_fs_expand_all(const C& paths){std::vector<strut_string> out;for(const auto& p:paths){auto xs=strut_fs_expand(strut_string(p));out.insert(out.end(),xs.begin(),xs.end());}return out;}
inline void strut_fs_remove_exact(const strut_string& p){std::error_code ec;std::filesystem::remove(strut_fs_path(p),ec);strut_fs_fail("remove",ec);}
inline void strut_fs_remove(const strut_string& p){for(const auto& x:strut_fs_expand(p))strut_fs_remove_exact(x);}
template<class C> inline void strut_fs_remove(const C& paths){for(const auto& p:strut_fs_expand_all(paths))strut_fs_remove_exact(p);}
inline void strut_fs_remove_all_exact(const strut_string& p){std::error_code ec;std::filesystem::remove_all(strut_fs_path(p),ec);strut_fs_fail("remove_all",ec);}
inline void strut_fs_remove_all(const strut_string& p){for(const auto& x:strut_fs_expand(p))strut_fs_remove_all_exact(x);}
inline void strut_fs_remove_all(const std::vector<strut_string>& paths){for(const auto& p:strut_fs_expand_all(paths))strut_fs_remove_all_exact(p);}
inline void strut_fs_copy_exact(const strut_string& a,const strut_string& b){std::error_code ec;std::filesystem::copy(strut_fs_path(a),strut_fs_path(b),std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing,ec);strut_fs_fail("copy",ec);}
inline void strut_fs_copy(const strut_string& a,const strut_string& b){if(!strut_fs_has_wildcards(a)){strut_fs_copy_exact(a,b);return;}auto paths=strut_fs_expand(a);if(paths.empty())throw strut_checked_error("FilesystemError","copy: wildcard matched no paths");const auto d=strut_fs_path(b);std::error_code ec;if(!std::filesystem::is_directory(d,ec)||ec)throw strut_checked_error("FilesystemError","copy: wildcard destination must be an existing directory");for(const auto& p:paths)strut_fs_copy_exact(p,strut_string((d/strut_fs_path(p).filename()).string()));}
template<class C> inline void strut_fs_copy(const C& paths,const strut_string& dest){const auto d=strut_fs_path(dest);std::error_code ec;if(!std::filesystem::is_directory(d,ec)||ec)throw strut_checked_error("FilesystemError","copy: bulk destination must be an existing directory");auto expanded=strut_fs_expand_all(paths);if(expanded.empty()&&paths.begin()!=paths.end())throw strut_checked_error("FilesystemError","copy: wildcard matched no paths");for(const auto& p:expanded)strut_fs_copy_exact(p,strut_string((d/strut_fs_path(p).filename()).string()));}
template<class A,class B> inline void strut_fs_copy(const A& sources,const B& destinations){auto a=sources.begin(),ae=sources.end(),b=destinations.begin(),be=destinations.end();for(;a!=ae&&b!=be;++a,++b)strut_fs_copy(strut_string(*a),strut_string(*b));if(a!=ae||b!=be)throw strut_checked_error("FilesystemError","copy: paired collections must have equal lengths");}
inline void strut_fs_move_exact(const strut_string& a,const strut_string& b){std::error_code ec;std::filesystem::rename(strut_fs_path(a),strut_fs_path(b),ec);strut_fs_fail("move",ec);}
inline void strut_fs_move(const strut_string& a,const strut_string& b){if(!strut_fs_has_wildcards(a)){strut_fs_move_exact(a,b);return;}auto paths=strut_fs_expand(a);if(paths.empty())throw strut_checked_error("FilesystemError","move: wildcard matched no paths");const auto d=strut_fs_path(b);std::error_code ec;if(!std::filesystem::is_directory(d,ec)||ec)throw strut_checked_error("FilesystemError","move: wildcard destination must be an existing directory");for(const auto& p:paths)strut_fs_move_exact(p,strut_string((d/strut_fs_path(p).filename()).string()));}
)CPP" << R"CPP(template<class C> inline void strut_fs_move(const C& paths,const strut_string& dest){const auto d=strut_fs_path(dest);std::error_code ec;if(!std::filesystem::is_directory(d,ec)||ec)throw strut_checked_error("FilesystemError","move: bulk destination must be an existing directory");auto expanded=strut_fs_expand_all(paths);if(expanded.empty()&&paths.begin()!=paths.end())throw strut_checked_error("FilesystemError","move: wildcard matched no paths");for(const auto& p:expanded)strut_fs_move_exact(p,strut_string((d/strut_fs_path(p).filename()).string()));}
template<class A,class B> inline void strut_fs_move(const A& sources,const B& destinations){auto a=sources.begin(),ae=sources.end(),b=destinations.begin(),be=destinations.end();for(;a!=ae&&b!=be;++a,++b)strut_fs_move(strut_string(*a),strut_string(*b));if(a!=ae||b!=be)throw strut_checked_error("FilesystemError","move: paired collections must have equal lengths");}
inline void strut_fs_touch(const strut_string& p){std::ofstream f(strut_fs_path(p),std::ios::app|std::ios::binary);if(!f)throw strut_checked_error("FilesystemError","touch: unable to open path");}
inline std::vector<strut_string> strut_fs_ls(const strut_string& p){std::error_code ec;std::vector<strut_string> out;for(std::filesystem::directory_iterator it(strut_fs_path(p),ec),end;!ec&&it!=end;it.increment(ec))out.emplace_back(it->path().filename().string());strut_fs_fail("ls",ec);std::sort(out.begin(),out.end());return out;}
inline std::vector<strut_string> strut_fs_walk(const strut_string& p){std::error_code ec;std::vector<strut_string> out;auto root=strut_fs_path(p);for(std::filesystem::recursive_directory_iterator it(root,ec),end;!ec&&it!=end;it.increment(ec))out.emplace_back(it->path().lexically_relative(root).generic_string());strut_fs_fail("walk",ec);std::sort(out.begin(),out.end());return out;}
inline strut_string strut_fs_cwd(){std::error_code ec;auto p=std::filesystem::current_path(ec);strut_fs_fail("cwd",ec);return p.string();}
inline void strut_fs_cd(const strut_string& p){std::error_code ec;std::filesystem::current_path(strut_fs_path(p),ec);strut_fs_fail("cd",ec);}
inline strut_string strut_fs_absolute(const strut_string& p){std::error_code ec;auto v=std::filesystem::absolute(strut_fs_path(p),ec);strut_fs_fail("absolute",ec);return v.lexically_normal().string();}
inline strut_string strut_fs_canonical(const strut_string& p){std::error_code ec;auto v=std::filesystem::canonical(strut_fs_path(p),ec);strut_fs_fail("canonical",ec);return v.string();}
inline strut_string strut_fs_parent(const strut_string& p){return strut_fs_path(p).parent_path().string();}
inline strut_string strut_fs_filename(const strut_string& p){return strut_fs_path(p).filename().string();}
inline strut_string strut_fs_extension(const strut_string& p){return strut_fs_path(p).extension().string();}
inline strut_string strut_fs_stem(const strut_string& p){return strut_fs_path(p).stem().string();}
template<class... Rest> inline strut_string strut_fs_join_path(const strut_string& a,const Rest&... rest){std::filesystem::path p=strut_fs_path(a);((p/=strut_fs_path(rest)),...);return p.lexically_normal().string();}
inline strut_string strut_fs_read_file(const strut_string& p){std::ifstream f(strut_fs_path(p),std::ios::binary|std::ios::ate);if(!f)throw strut_checked_error("FilesystemError","read_file: unable to open path");auto end=f.tellg();if(end<0)throw strut_checked_error("FilesystemError","read_file: unable to determine file size");std::string out(static_cast<std::size_t>(end),'\0');f.seekg(0,std::ios::beg);if(!out.empty()&&!f.read(out.data(),static_cast<std::streamsize>(out.size())))throw strut_checked_error("FilesystemError","read_file: short read");return strut_string(std::move(out));}
inline std::vector<std::uint8_t> strut_fs_read_bytes(const strut_string& p){std::ifstream f(strut_fs_path(p),std::ios::binary|std::ios::ate);if(!f)throw strut_checked_error("FilesystemError","read_bytes: unable to open path");auto end=f.tellg();if(end<0)throw strut_checked_error("FilesystemError","read_bytes: unable to determine file size");std::vector<std::uint8_t> out(static_cast<std::size_t>(end));f.seekg(0,std::ios::beg);if(!out.empty()&&!f.read(reinterpret_cast<char*>(out.data()),static_cast<std::streamsize>(out.size())))throw strut_checked_error("FilesystemError","read_bytes: short read");return out;}
inline void strut_fs_write_file(const strut_string& p,const strut_string& data){std::ofstream f(strut_fs_path(p),std::ios::binary|std::ios::trunc);if(!f||(!data.v.empty()&&!f.write(data.v.data(),static_cast<std::streamsize>(data.v.size()))))throw strut_checked_error("FilesystemError","write_file failed");}
inline void strut_fs_write_file(const strut_string& p,const std::vector<std::uint8_t>& data){std::ofstream f(strut_fs_path(p),std::ios::binary|std::ios::trunc);if(!f||(!data.empty()&&!f.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()))))throw strut_checked_error("FilesystemError","write_file failed");}
inline void strut_fs_append_file(const strut_string& p,const strut_string& data){std::ofstream f(strut_fs_path(p),std::ios::binary|std::ios::app);if(!f||(!data.v.empty()&&!f.write(data.v.data(),static_cast<std::streamsize>(data.v.size()))))throw strut_checked_error("FilesystemError","append_file failed");}
inline void strut_fs_append_file(const strut_string& p,const std::vector<std::uint8_t>& data){std::ofstream f(strut_fs_path(p),std::ios::binary|std::ios::app);if(!f||(!data.empty()&&!f.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()))))throw strut_checked_error("FilesystemError","append_file failed");}
inline std::optional<strut_string> strut_env(const strut_string& name){const char* v=std::getenv(name.v.c_str());if(!v)return std::nullopt;return strut_string(v);}
inline void strut_set_env(const strut_string& name,const strut_string& value){
#ifdef _WIN32
    if(_putenv_s(name.v.c_str(),value.v.c_str())!=0)throw strut_checked_error("EnvironmentError","set_env failed");
#else
    if(::setenv(name.v.c_str(),value.v.c_str(),1)!=0)throw strut_checked_error("EnvironmentError","set_env failed");
#endif
}
inline void strut_unset_env(const strut_string& name){
#ifdef _WIN32
    if(_putenv_s(name.v.c_str(),"")!=0)throw strut_checked_error("EnvironmentError","unset_env failed");
#else
    if(::unsetenv(name.v.c_str())!=0)throw strut_checked_error("EnvironmentError","unset_env failed");
#endif
}
inline std::int64_t strut_now_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline std::int64_t strut_unix_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
inline void strut_sleep_ms(std::int64_t value){if(value<0)throw strut_checked_error("TimeError","sleep_ms requires a non-negative duration");std::this_thread::sleep_for(std::chrono::milliseconds(value));}
#include <csignal>
inline volatile std::sig_atomic_t strut_shutdown_signal_flag=0;
inline void strut_shutdown_signal_handler(int){strut_shutdown_signal_flag=1;}
inline void wait_for_shutdown_signal(){strut_shutdown_signal_flag=0;auto old_int=std::signal(SIGINT,strut_shutdown_signal_handler);auto old_term=std::signal(SIGTERM,strut_shutdown_signal_handler);while(!strut_shutdown_signal_flag)std::this_thread::sleep_for(std::chrono::milliseconds(25));std::signal(SIGINT,old_int);std::signal(SIGTERM,old_term);}
#ifdef _WIN32
#undef stdout
#undef stderr
#endif
struct strut_exec_result { std::int32_t exit_code=0; strut_string stdout; strut_string stderr; };
struct strut_exec_options { bool capture=true; bool inherit_stdio=false; strut_string cwd; std::vector<std::pair<std::string,std::string>> env; };
inline strut_exec_options strut_parse_exec_options(const json::Document& d){strut_exec_options o;if(d.type==json::Type::Null)return o;if(d.type!=json::Type::Object)throw strut_checked_error("ExecError","exec options must be a JSON object");if(d.has("capture")&&d["capture"].type==json::Type::Boolean)o.capture=d["capture"].boolean;if(d.has("inherit_stdio")&&d["inherit_stdio"].type==json::Type::Boolean)o.inherit_stdio=d["inherit_stdio"].boolean;if(d.has("cwd")&&d["cwd"].type==json::Type::String)o.cwd=d["cwd"].string;if(d.has("env")){const auto& e=d["env"];if(e.type!=json::Type::Object)throw strut_checked_error("ExecError","exec env option must be an object");for(const auto& kv:e.object){if(kv.second.type!=json::Type::String)throw strut_checked_error("ExecError","exec environment values must be strings");o.env.emplace_back(kv.first,kv.second.string);}}if(o.inherit_stdio)o.capture=false;return o;}
#ifndef _WIN32
#include <signal.h>
inline strut_exec_result strut_exec_impl(const strut_string& program,const std::vector<strut_string>& args,const strut_exec_options& options){
)CPP" << R"CPP(    int out_pipe[2]={-1,-1},err_pipe[2]={-1,-1}; if(options.capture&&(pipe(out_pipe)!=0||pipe(err_pipe)!=0))throw strut_checked_error("ExecError",std::string("pipe failed: ")+std::strerror(errno));
    pid_t pid=fork(); if(pid<0)throw strut_checked_error("ExecError",std::string("fork failed: ")+std::strerror(errno));
    if(pid==0){
        if(options.capture){close(out_pipe[0]);close(err_pipe[0]);dup2(out_pipe[1],STDOUT_FILENO);dup2(err_pipe[1],STDERR_FILENO);close(out_pipe[1]);close(err_pipe[1]);}
        if(!options.cwd.v.empty()&&chdir(options.cwd.v.c_str())!=0)_exit(126);
        for(const auto& kv:options.env)if(setenv(kv.first.c_str(),kv.second.c_str(),1)!=0)_exit(126);
        std::vector<std::string> storage;storage.reserve(args.size()+1);storage.push_back(program.v);for(const auto& a:args)storage.push_back(a.v);std::vector<char*> argv;argv.reserve(storage.size()+1);for(auto& a:storage)argv.push_back(a.data());argv.push_back(nullptr);execvp(program.v.c_str(),argv.data());_exit(errno==ENOENT?127:126);
    }
    if(options.capture){close(out_pipe[1]);close(err_pipe[1]);}
    strut_exec_result result; std::string sout,serr;
    auto read_fd=[](int fd,std::string& dst){char b[4096];for(;;){ssize_t n=read(fd,b,sizeof(b));if(n>0)dst.append(b,static_cast<std::size_t>(n));else break;}close(fd);};
    std::thread t1,t2;if(options.capture){t1=std::thread(read_fd,out_pipe[0],std::ref(sout));t2=std::thread(read_fd,err_pipe[0],std::ref(serr));}
    int status=0;while(waitpid(pid,&status,0)<0){if(errno!=EINTR)throw strut_checked_error("ExecError",std::string("waitpid failed: ")+std::strerror(errno));}
    if(options.capture){t1.join();t2.join();}if(WIFEXITED(status))result.exit_code=WEXITSTATUS(status);else if(WIFSIGNALED(status))result.exit_code=128+WTERMSIG(status);else result.exit_code=-1;result.stdout=strut_string(std::move(sout));result.stderr=strut_string(std::move(serr));return result;
}
#else
inline std::string strut_win_quote(const std::string& a){if(a.find_first_of(" \t\"")==std::string::npos)return a;std::string r="\"";std::size_t slashes=0;for(char c:a){if(c=='\\'){++slashes;continue;}if(c=='\"'){r.append(slashes*2+1,'\\');r+='\"';slashes=0;continue;}r.append(slashes,'\\');slashes=0;r+=c;}r.append(slashes*2,'\\');r+='\"';return r;}
inline strut_exec_result strut_exec_impl(const strut_string& program,const std::vector<strut_string>& args,const strut_exec_options& options){
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE out_r=nullptr,out_w=nullptr,err_r=nullptr,err_w=nullptr;if(options.capture){if(!CreatePipe(&out_r,&out_w,&sa,0)||!CreatePipe(&err_r,&err_w,&sa,0))throw strut_checked_error("ExecError","CreatePipe failed");SetHandleInformation(out_r,HANDLE_FLAG_INHERIT,0);SetHandleInformation(err_r,HANDLE_FLAG_INHERIT,0);}
    std::string cmd=strut_win_quote(program.v);for(const auto& a:args){cmd+=' ';cmd+=strut_win_quote(a.v);}std::vector<char> mutable_cmd(cmd.begin(),cmd.end());mutable_cmd.push_back('\0');
    STARTUPINFOA si{};si.cb=sizeof(si);if(options.capture||options.inherit_stdio){si.dwFlags|=STARTF_USESTDHANDLES;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);si.hStdOutput=options.capture?out_w:GetStdHandle(STD_OUTPUT_HANDLE);si.hStdError=options.capture?err_w:GetStdHandle(STD_ERROR_HANDLE);}
    std::vector<char> env_block;LPVOID env_ptr=nullptr;if(!options.env.empty()){LPCH raw=GetEnvironmentStringsA();std::map<std::string,std::string> env;if(raw){for(LPCH p=raw;*p;){std::string entry=p;p+=entry.size()+1;auto eq=entry.find('=');if(eq!=std::string::npos&&eq>0)env[entry.substr(0,eq)]=entry.substr(eq+1);}FreeEnvironmentStringsA(raw);}for(const auto& kv:options.env)env[kv.first]=kv.second;for(const auto& kv:env){auto line=kv.first+"="+kv.second;env_block.insert(env_block.end(),line.begin(),line.end());env_block.push_back('\0');}env_block.push_back('\0');env_ptr=env_block.data();}
    PROCESS_INFORMATION pi{};BOOL ok=CreateProcessA(nullptr,mutable_cmd.data(),nullptr,nullptr,options.capture||options.inherit_stdio,0,env_ptr,options.cwd.v.empty()?nullptr:options.cwd.v.c_str(),&si,&pi);if(options.capture){CloseHandle(out_w);CloseHandle(err_w);}if(!ok)throw strut_checked_error("ExecError","CreateProcess failed");
    std::string sout,serr;auto read_handle=[](HANDLE h,std::string& dst){char b[4096];DWORD n=0;while(ReadFile(h,b,sizeof(b),&n,nullptr)&&n)dst.append(b,n);CloseHandle(h);};std::thread t1,t2;if(options.capture){t1=std::thread(read_handle,out_r,std::ref(sout));t2=std::thread(read_handle,err_r,std::ref(serr));}WaitForSingleObject(pi.hProcess,INFINITE);DWORD code=0;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);if(options.capture){t1.join();t2.join();auto normalize=[](std::string& value){std::size_t pos=0;while((pos=value.find("\r\n",pos))!=std::string::npos)value.erase(pos,1);};normalize(sout);normalize(serr);}strut_exec_result result;result.exit_code=static_cast<std::int32_t>(code);result.stdout=strut_string(std::move(sout));result.stderr=strut_string(std::move(serr));return result;
}
#endif
inline strut_exec_result strut_exec(const strut_string& program,const std::vector<strut_string>& args){return strut_exec_impl(program,args,{});}inline strut_exec_result strut_exec(const strut_string& program,const std::vector<strut_string>& args,const json::Document& options){return strut_exec_impl(program,args,strut_parse_exec_options(options));}
inline strut_exec_result strut_exec_shell(const strut_string& command){
#ifdef _WIN32
    return strut_exec(strut_string("cmd.exe"),std::vector<strut_string>{strut_string("/C"),command});
#else
    return strut_exec(strut_string("/bin/sh"),std::vector<strut_string>{strut_string("-c"),command});
#endif
}
class strut_executor {
public:
    strut_executor(){auto n=std::thread::hardware_concurrency();if(n<2)n=2;for(unsigned i=0;i<n;++i)workers_.emplace_back([this]{worker();});}
    ~strut_executor(){{std::lock_guard<std::mutex> g(m_);stopping_=true;}cv_.notify_all();for(auto& t:workers_)if(t.joinable())t.join();}
    template<class F> auto submit(F&& f)->std::future<std::invoke_result_t<F>>{using R=std::invoke_result_t<F>;auto task=std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));auto fut=task->get_future();{std::lock_guard<std::mutex> g(m_);q_.emplace([task]{(*task)();});}cv_.notify_one();return fut;}
private:
    void worker(){for(;;){std::function<void()> job;{std::unique_lock<std::mutex> l(m_);cv_.wait(l,[this]{return stopping_||!q_.empty();});if(stopping_&&q_.empty())return;job=std::move(q_.front());q_.pop();}job();}}
    std::vector<std::thread> workers_;std::queue<std::function<void()>> q_;std::mutex m_;std::condition_variable cv_;bool stopping_=false;
};
inline strut_executor& strut_global_executor(){static strut_executor ex;return ex;}
template<class T> class strut_future { public: strut_future()=default; explicit strut_future(std::future<T>&& f):f_(std::move(f)){} T get(){return f_.get();} bool valid() const{return f_.valid();} private: std::future<T> f_; };
template<> class strut_future<void> { public: strut_future()=default; explicit strut_future(std::future<void>&& f):f_(std::move(f)){} void get(){f_.get();} bool valid() const{return f_.valid();} private: std::future<void> f_; };
template<class F> auto strut_async(F&& f)->strut_future<std::invoke_result_t<F>>{using R=std::invoke_result_t<F>;return strut_future<R>(strut_global_executor().submit(std::forward<F>(f)));}
template<class T> T strut_await(strut_future<T>& f){return f.get();}
template<class T> T strut_await(strut_future<T>&& f){return f.get();}
inline void strut_await(strut_future<void>& f){f.get();}
inline void strut_await(strut_future<void>&& f){f.get();}

#include <atomic>
template<class T> class strut_atomic {
    static_assert(std::is_integral_v<T>,"atomic<T> supports integer and bool scalar types");
public:
    strut_atomic() noexcept=default;
    strut_atomic(T value) noexcept:value_(value){}
    strut_atomic(const strut_atomic&)=delete;
    strut_atomic& operator=(const strut_atomic&)=delete;
    T load() const noexcept{return value_.load();}
    void store(T value) noexcept{value_.store(value);}
    T exchange(T value) noexcept{return value_.exchange(value);}
    bool compare_exchange(T expected,T desired) noexcept{return value_.compare_exchange_strong(expected,desired);}
    template<class U=T> std::enable_if_t<!std::is_same_v<U,bool>,U> fetch_add(U value) noexcept{return value_.fetch_add(value);}
    template<class U=T> std::enable_if_t<!std::is_same_v<U,bool>,U> fetch_sub(U value) noexcept{return value_.fetch_sub(value);}
private: std::atomic<T> value_{};
};

class strut_mutex {
public:
    strut_mutex():m_(std::make_shared<std::mutex>()){}
    void lock() const{m_->lock();}
    void unlock() const{m_->unlock();}
    template<class F> auto lock(F&& f) const -> decltype(f()) { std::lock_guard<std::mutex> guard(*m_); return f(); }
private: std::shared_ptr<std::mutex> m_;
};

template<class T> class strut_channel {
    struct state { mutable std::mutex m; std::condition_variable cv; std::queue<T> q; bool closed=false; };
public:
    strut_channel():s_(std::make_shared<state>()){}
)CPP" << R"CPP(    void send(T value) const{std::lock_guard<std::mutex> g(s_->m);if(s_->closed)throw std::runtime_error("send on closed channel");s_->q.push(std::move(value));s_->cv.notify_one();}
    std::optional<T> receive() const{std::unique_lock<std::mutex> g(s_->m);s_->cv.wait(g,[&]{return s_->closed||!s_->q.empty();});if(s_->q.empty())return std::nullopt;T value=std::move(s_->q.front());s_->q.pop();return value;}
    void close() const{std::lock_guard<std::mutex> g(s_->m);s_->closed=true;s_->cv.notify_all();}
    bool closed() const{std::lock_guard<std::mutex> g(s_->m);return s_->closed;}
private: std::shared_ptr<state> s_;
};

class strut_thread {
public:
    strut_thread()=default;
    template<class F,class... A> explicit strut_thread(F&& f,A&&... a){
        error_=std::make_shared<std::exception_ptr>();
        auto err=error_;
        thread_=std::thread([err,fn=std::forward<F>(f),args=std::make_tuple(std::forward<A>(a)...)]() mutable {
            try { std::apply(fn,std::move(args)); } catch(...) { *err=std::current_exception(); }
        });
    }
    strut_thread(const strut_thread&)=delete;strut_thread& operator=(const strut_thread&)=delete;
    strut_thread(strut_thread&&)=default;strut_thread& operator=(strut_thread&&)=default;
    bool joinable() const{return thread_.joinable();}
    void join(){if(!thread_.joinable())throw strut_checked_error("ThreadError","join on non-joinable thread");thread_.join();if(error_&&*error_)std::rethrow_exception(*error_);}
    ~strut_thread(){if(thread_.joinable())thread_.join();}
private:
    std::thread thread_;std::shared_ptr<std::exception_ptr> error_;
};
class strut_process_in {
public:
#ifdef _WIN32
    HANDLE h=nullptr;
#else
    int fd=-1;
#endif
    strut_process_in()=default;strut_process_in(const strut_process_in&)=delete;strut_process_in& operator=(const strut_process_in&)=delete;
    strut_process_in(strut_process_in&& other) noexcept {
#ifdef _WIN32
        h=other.h;other.h=nullptr;
#else
        fd=other.fd;other.fd=-1;
#endif
    }
    strut_process_in& operator=(strut_process_in&& other) noexcept {if(this!=&other){close();
#ifdef _WIN32
        h=other.h;other.h=nullptr;
#else
        fd=other.fd;other.fd=-1;
#endif
    }return *this;}
    void write(const strut_string& s){
#ifdef _WIN32
        if(!h)throw strut_checked_error("ExecError","process stdin is closed");DWORD n=0;if(!WriteFile(h,s.v.data(),static_cast<DWORD>(s.v.size()),&n,nullptr)||n!=s.v.size())throw strut_checked_error("ExecError","process stdin write failed");
#else
        if(fd<0)throw strut_checked_error("ExecError","process stdin is closed");const char* p=s.v.data();std::size_t left=s.v.size();while(left){ssize_t n=::write(fd,p,left);if(n<0&&errno==EINTR)continue;if(n<=0)throw strut_checked_error("ExecError","process stdin write failed");p+=n;left-=static_cast<std::size_t>(n);}
#endif
    }
    void write_line(const strut_string& s){write(strut_string(s.v+"\n"));}
    void close(){
#ifdef _WIN32
        if(h){CloseHandle(h);h=nullptr;}
#else
        if(fd>=0){::close(fd);fd=-1;}
#endif
    }
    ~strut_process_in(){close();}
};
class strut_process_out {
public:
#ifdef _WIN32
    HANDLE h=nullptr;
#else
    int fd=-1;
#endif
    bool ended=false;
    strut_process_out()=default;strut_process_out(const strut_process_out&)=delete;strut_process_out& operator=(const strut_process_out&)=delete;
    strut_process_out(strut_process_out&& other) noexcept : ended(other.ended) {
#ifdef _WIN32
        h=other.h;other.h=nullptr;
#else
        fd=other.fd;other.fd=-1;
#endif
        other.ended=true;
    }
    strut_process_out& operator=(strut_process_out&& other) noexcept {if(this!=&other){close();ended=other.ended;
#ifdef _WIN32
        h=other.h;other.h=nullptr;
#else
        fd=other.fd;other.fd=-1;
#endif
        other.ended=true;}return *this;}
    strut_string read(std::int32_t count){if(count<0)throw strut_checked_error("ExecError","negative process read size");std::string out;out.resize(static_cast<std::size_t>(count));
#ifdef _WIN32
        if(!h)return strut_string();DWORD n=0;if(!ReadFile(h,out.data(),static_cast<DWORD>(out.size()),&n,nullptr)){if(GetLastError()==ERROR_BROKEN_PIPE){ended=true;return strut_string();}throw strut_checked_error("ExecError","process pipe read failed");}out.resize(n);if(n==0)ended=true;
#else
        if(fd<0)return strut_string();ssize_t n=::read(fd,out.data(),out.size());if(n<0&&errno==EINTR)return read(count);if(n<0)throw strut_checked_error("ExecError","process pipe read failed");out.resize(static_cast<std::size_t>(n));if(n==0)ended=true;
#endif
        return strut_string(std::move(out));}
    strut_string read_line(){std::string out;for(;;){auto c=read(1);if(c.v.empty())break;if(c.v[0]=='\n')break;if(c.v[0]!='\r')out+=c.v[0];}return strut_string(std::move(out));}
    strut_string read_all(){std::string out;for(;;){auto chunk=read(4096);if(chunk.v.empty())break;out+=chunk.v;}return strut_string(std::move(out));}
    bool eof() const{return ended;}
    void close(){
#ifdef _WIN32
        if(h){CloseHandle(h);h=nullptr;}
#else
        if(fd>=0){::close(fd);fd=-1;}
#endif
        ended=true;}
    ~strut_process_out(){close();}
};
class strut_process {
public:
    strut_process_in in;strut_process_out out;strut_process_out err;
    strut_process()=default;
    strut_process(const strut_string& program,const std::vector<strut_string>& args){start(program,args);}
    strut_process(const strut_process&)=delete;strut_process& operator=(const strut_process&)=delete;
    strut_process(strut_process&& other) noexcept : in(std::move(other.in)),out(std::move(other.out)),err(std::move(other.err)),
#ifdef _WIN32
        pi_(other.pi_),
#else
        pid_(other.pid_),
#endif
        running_(other.running_),exit_code_(other.exit_code_){other.running_=false;
#ifdef _WIN32
        other.pi_={};
#else
        other.pid_=-1;
#endif
    }
    strut_process& operator=(strut_process&&)=delete;
    void start(const strut_string& program,const std::vector<strut_string>& args){
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE child_in=nullptr,out_w=nullptr,err_w=nullptr;if(!CreatePipe(&child_in,&in.h,&sa,0)||!CreatePipe(&out.h,&out_w,&sa,0)||!CreatePipe(&err.h,&err_w,&sa,0))throw strut_checked_error("ExecError","CreatePipe failed");SetHandleInformation(in.h,HANDLE_FLAG_INHERIT,0);SetHandleInformation(out.h,HANDLE_FLAG_INHERIT,0);SetHandleInformation(err.h,HANDLE_FLAG_INHERIT,0);std::string cmd=strut_win_quote(program.v);for(const auto& a:args){cmd+=' ';cmd+=strut_win_quote(a.v);}std::vector<char> mc(cmd.begin(),cmd.end());mc.push_back('\0');STARTUPINFOA si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=child_in;si.hStdOutput=out_w;si.hStdError=err_w;BOOL ok=CreateProcessA(nullptr,mc.data(),nullptr,nullptr,TRUE,0,nullptr,nullptr,&si,&pi_);CloseHandle(child_in);CloseHandle(out_w);CloseHandle(err_w);if(!ok)throw strut_checked_error("ExecError","CreateProcess failed");running_=true;
#else
        int pin[2],pout[2],perr[2];if(pipe(pin)||pipe(pout)||pipe(perr))throw strut_checked_error("ExecError",std::string("pipe failed: ")+std::strerror(errno));pid_=fork();if(pid_<0)throw strut_checked_error("ExecError",std::string("fork failed: ")+std::strerror(errno));if(pid_==0){dup2(pin[0],STDIN_FILENO);dup2(pout[1],STDOUT_FILENO);dup2(perr[1],STDERR_FILENO);::close(pin[0]);::close(pin[1]);::close(pout[0]);::close(pout[1]);::close(perr[0]);::close(perr[1]);std::vector<std::string> storage;storage.push_back(program.v);for(const auto& a:args)storage.push_back(a.v);std::vector<char*> av;for(auto& a:storage)av.push_back(a.data());av.push_back(nullptr);execvp(program.v.c_str(),av.data());_exit(errno==ENOENT?127:126);}::close(pin[0]);::close(pout[1]);::close(perr[1]);in.fd=pin[1];out.fd=pout[0];err.fd=perr[0];running_=true;
#endif
    }
    std::int32_t wait(){if(!running_)return exit_code_;
#ifdef _WIN32
        WaitForSingleObject(pi_.hProcess,INFINITE);DWORD c=0;GetExitCodeProcess(pi_.hProcess,&c);CloseHandle(pi_.hThread);CloseHandle(pi_.hProcess);exit_code_=static_cast<std::int32_t>(c);
#else
        int status=0;while(waitpid(pid_,&status,0)<0){if(errno!=EINTR)throw strut_checked_error("ExecError","waitpid failed");}exit_code_=WIFEXITED(status)?WEXITSTATUS(status):(WIFSIGNALED(status)?128+WTERMSIG(status):-1);
#endif
        running_=false;return exit_code_;}
    void terminate(){if(!running_)return;
#ifdef _WIN32
        if(!TerminateProcess(pi_.hProcess,1))throw strut_checked_error("ExecError","TerminateProcess failed");
#else
        if(::kill(pid_,SIGTERM)!=0&&errno!=ESRCH)throw strut_checked_error("ExecError","kill failed");
#endif
    }
    bool running(){return running_;}std::int32_t exit_code(){return running_?-1:exit_code_;}void close_input(){in.close();}
    ~strut_process(){if(running_){terminate();try{wait();}catch(...){}}}
private:
#ifdef _WIN32
    PROCESS_INFORMATION pi_{};
#else
    pid_t pid_=-1;
#endif
    bool running_=false;std::int32_t exit_code_=-1;
};
inline strut_exec_result strut_pipe_exec(const strut_string& first,const std::vector<strut_string>& first_args,const strut_string& second,const std::vector<strut_string>& second_args){
)CPP" << R"CPP(    strut_process a(first,first_args);strut_process b(second,second_args);std::thread pump([&](){for(;;){auto chunk=a.out.read(4096);if(chunk.v.empty())break;b.in.write(chunk);}b.in.close();});auto aerr=std::thread([&](){a.err.read_all();});auto berr=std::thread([&](){b.err.read_all();});auto code_b=b.wait();auto stdout_b=b.out.read_all();pump.join();auto code_a=a.wait();(void)code_a;aerr.join();berr.join();strut_exec_result r;r.exit_code=code_b;r.stdout=std::move(stdout_b);return r;
}

#ifdef _WIN32
using strut_socket_handle=SOCKET; constexpr strut_socket_handle strut_invalid_socket=INVALID_SOCKET;
inline void strut_socket_close(strut_socket_handle h){if(h!=strut_invalid_socket){shutdown(h,SD_BOTH);closesocket(h);}}
struct strut_winsock_runtime{strut_winsock_runtime(){WSADATA d{};if(WSAStartup(MAKEWORD(2,2),&d)!=0)throw strut_checked_error("NetworkError","WSAStartup failed");}~strut_winsock_runtime(){WSACleanup();}};
inline void strut_socket_init(){static strut_winsock_runtime runtime;(void)runtime;}
#else
using strut_socket_handle=int; constexpr strut_socket_handle strut_invalid_socket=-1;
inline void strut_socket_close(strut_socket_handle h){if(h!=strut_invalid_socket){::shutdown(h,SHUT_RDWR);::close(h);}}
inline void strut_socket_init(){}
#endif
struct strut_socket_state{strut_socket_handle handle=strut_invalid_socket;~strut_socket_state(){strut_socket_close(handle);}};
class strut_tcp_socket {
public:
    strut_tcp_socket():s_(std::make_shared<strut_socket_state>()){}
    explicit strut_tcp_socket(strut_socket_handle h):s_(std::make_shared<strut_socket_state>()){s_->handle=h;}
    bool is_open() const{return s_&&s_->handle!=strut_invalid_socket;}
    void close(){if(is_open()){strut_socket_close(s_->handle);s_->handle=strut_invalid_socket;}}
    void write(const strut_string& data){if(!is_open())throw strut_checked_error("NetworkError","write on closed socket");std::size_t off=0;while(off<data.v.size()){
#ifdef _WIN32
        int n=::send(s_->handle,data.v.data()+off,static_cast<int>(data.v.size()-off),0);
#else
        ssize_t n=::send(s_->handle,data.v.data()+off,data.v.size()-off,0);
#endif
        if(n<=0)throw strut_checked_error("NetworkError","socket write failed");off+=static_cast<std::size_t>(n);}}
    strut_string read(std::int64_t max_bytes=4096){if(!is_open())throw strut_checked_error("NetworkError","read on closed socket");if(max_bytes<=0)return strut_string();std::string out(static_cast<std::size_t>(max_bytes),'\0');
#ifdef _WIN32
        int n=::recv(s_->handle,out.data(),static_cast<int>(out.size()),0);
#else
        ssize_t n=::recv(s_->handle,out.data(),out.size(),0);
#endif
        if(n<0)throw strut_checked_error("NetworkError","socket read failed");out.resize(static_cast<std::size_t>(n));return strut_string(std::move(out));}
    strut_socket_handle native_handle() const{return s_->handle;}
private: std::shared_ptr<strut_socket_state> s_;
};
inline strut_tcp_socket tcp_connect(const strut_string& host,std::int32_t port){strut_socket_init();if(port<1||port>65535)throw strut_checked_error("NetworkError","invalid TCP port");addrinfo hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;addrinfo* list=nullptr;const std::string service=std::to_string(port);if(getaddrinfo(host.v.c_str(),service.c_str(),&hints,&list)!=0)throw strut_checked_error("NetworkError","host resolution failed");strut_socket_handle h=strut_invalid_socket;for(addrinfo* p=list;p;p=p->ai_next){h=::socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(h==strut_invalid_socket)continue;if(::connect(h,p->ai_addr,static_cast<int>(p->ai_addrlen))==0)break;strut_socket_close(h);h=strut_invalid_socket;}freeaddrinfo(list);if(h==strut_invalid_socket)throw strut_checked_error("NetworkError","TCP connect failed");return strut_tcp_socket(h);}
inline strut_future<strut_tcp_socket> tcp_connect_async(const strut_string& host,std::int32_t port){return strut_async([host,port]{return tcp_connect(host,port);});}
class strut_tcp_listener {
public:
    strut_tcp_listener():s_(std::make_shared<strut_socket_state>()){}
    explicit strut_tcp_listener(strut_socket_handle h):s_(std::make_shared<strut_socket_state>()){s_->handle=h;}
    bool is_open() const{return s_&&s_->handle!=strut_invalid_socket;} void close(){if(is_open()){strut_socket_close(s_->handle);s_->handle=strut_invalid_socket;}}
    strut_tcp_socket accept(){if(!is_open())throw strut_checked_error("NetworkError","accept on closed listener");auto h=::accept(s_->handle,nullptr,nullptr);if(h==strut_invalid_socket)throw strut_checked_error("NetworkError","TCP accept failed");return strut_tcp_socket(h);}
    strut_future<strut_tcp_socket> accept_async(){auto copy=*this;return strut_async([copy]() mutable{return copy.accept();});}
private: std::shared_ptr<strut_socket_state> s_;
};
inline strut_tcp_listener tcp_listen(const strut_string& host,std::int32_t port,std::int32_t backlog=128){strut_socket_init();if(port<1||port>65535)throw strut_checked_error("NetworkError","invalid TCP port");addrinfo hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_flags=AI_PASSIVE;addrinfo* list=nullptr;const std::string service=std::to_string(port);const char* node=host.v.empty()?nullptr:host.v.c_str();if(getaddrinfo(node,service.c_str(),&hints,&list)!=0)throw strut_checked_error("NetworkError","listen address resolution failed");strut_socket_handle h=strut_invalid_socket;for(addrinfo* p=list;p;p=p->ai_next){h=::socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(h==strut_invalid_socket)continue;int yes=1;setsockopt(h,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&yes),sizeof(yes));if(::bind(h,p->ai_addr,static_cast<int>(p->ai_addrlen))==0&&::listen(h,backlog)==0)break;strut_socket_close(h);h=strut_invalid_socket;}freeaddrinfo(list);if(h==strut_invalid_socket)throw strut_checked_error("NetworkError","TCP listen failed: address unavailable or port already in use");return strut_tcp_listener(h);}


#ifdef STRUT_USE_CURL
struct strut_curl_global{strut_curl_global(){if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)throw strut_checked_error("TlsError","libcurl global initialization failed");}~strut_curl_global(){curl_global_cleanup();}};
inline void strut_curl_init(){static strut_curl_global g;(void)g;}
class strut_tls_stream {
public:
    strut_tls_stream()=default;
    explicit strut_tls_stream(CURL* c):curl_(c){}
    strut_tls_stream(const strut_tls_stream&)=delete;strut_tls_stream& operator=(const strut_tls_stream&)=delete;
    strut_tls_stream(strut_tls_stream&& o) noexcept:curl_(o.curl_){o.curl_=nullptr;} strut_tls_stream& operator=(strut_tls_stream&& o) noexcept{if(this!=&o){close();curl_=o.curl_;o.curl_=nullptr;}return *this;}
    bool is_open() const{return curl_!=nullptr;} void close(){if(curl_){curl_easy_cleanup(curl_);curl_=nullptr;}}
    void write(const strut_string& data){if(!curl_)throw strut_checked_error("TlsError","write on closed TLS stream");size_t off=0;while(off<data.v.size()){size_t sent=0;auto rc=curl_easy_send(curl_,data.v.data()+off,data.v.size()-off,&sent);if(rc!=CURLE_OK)throw strut_checked_error("TlsError",curl_easy_strerror(rc));off+=sent;}}
    strut_string read(std::int64_t max_bytes=4096){if(!curl_)throw strut_checked_error("TlsError","read on closed TLS stream");std::string out(static_cast<std::size_t>(max_bytes),'\0');size_t got=0;auto rc=curl_easy_recv(curl_,out.data(),out.size(),&got);if(rc!=CURLE_OK&&rc!=CURLE_AGAIN)throw strut_checked_error("TlsError",curl_easy_strerror(rc));out.resize(got);return strut_string(std::move(out));}
    ~strut_tls_stream(){close();}
private:CURL* curl_=nullptr;
};
)CPP" << R"CPP(inline strut_tls_stream tls_connect(const strut_string& host,std::int32_t port){strut_curl_init();CURL* c=curl_easy_init();if(!c)throw strut_checked_error("TlsError","curl_easy_init failed");const std::string url="https://"+host.v+":"+std::to_string(port)+"/";curl_easy_setopt(c,CURLOPT_URL,url.c_str());curl_easy_setopt(c,CURLOPT_CONNECT_ONLY,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT_MS,30000L);auto rc=curl_easy_perform(c);if(rc!=CURLE_OK){curl_easy_cleanup(c);throw strut_checked_error("TlsError",curl_easy_strerror(rc));}return strut_tls_stream(c);}
#endif
#ifdef STRUT_USE_CURL
struct strut_http_response {
    std::int32_t status=0; strut_string body; std::unordered_map<strut_string,strut_string> headers;
    json::Document json() const { json::Document d; json::ParseDiagnostic diag; if(!json::Document::parse(body.v,d,diag))throw strut_checked_error("HttpError","response body is not valid JSON"); return d; }
};
inline size_t strut_http_write_cb(char* p,size_t size,size_t nmemb,void* u){auto* body=static_cast<std::string*>(u);body->append(p,size*nmemb);return size*nmemb;}
inline size_t strut_http_header_cb(char* p,size_t size,size_t nmemb,void* u){const size_t n=size*nmemb;std::string line(p,n);auto* headers=static_cast<std::unordered_map<strut_string,strut_string>*>(u);auto colon=line.find(':');if(colon!=std::string::npos){std::string k=line.substr(0,colon),v=line.substr(colon+1);while(!v.empty()&&(v.front()==' '||v.front()=='\t'))v.erase(v.begin());while(!v.empty()&&(v.back()=='\r'||v.back()=='\n'||v.back()==' '||v.back()=='\t'))v.pop_back();(*headers)[strut_string(k)]=strut_string(v);}return n;}
inline strut_http_response http_request(const strut_string& method,const strut_string& url,const json::Document& options){strut_curl_init();CURL* c=curl_easy_init();if(!c)throw strut_checked_error("HttpError","curl_easy_init failed");strut_http_response response;curl_slist* list=nullptr;std::string body;long timeout=30000;bool follow=true;if(options.type!=json::Type::Null&&options.type!=json::Type::Object){curl_easy_cleanup(c);throw strut_checked_error("HttpError","HTTP options must be JSON object or null");}if(options.type==json::Type::Object){if(options.has("timeout_ms")&&options["timeout_ms"].type==json::Type::Number)timeout=static_cast<long>(options["timeout_ms"].num);if(options.has("follow_redirects")&&options["follow_redirects"].type==json::Type::Boolean)follow=options["follow_redirects"].boolean;if(options.has("body")){const auto& b=options["body"];body=b.type==json::Type::String?b.string:b.dump();}if(options.has("headers")){const auto& h=options["headers"];if(h.type!=json::Type::Object){curl_easy_cleanup(c);throw strut_checked_error("HttpError","headers must be JSON object");}for(const auto& kv:h.object){if(kv.second.type!=json::Type::String){curl_easy_cleanup(c);throw strut_checked_error("HttpError","header values must be strings");}const std::string line=kv.first+": "+kv.second.string;list=curl_slist_append(list,line.c_str());}}}
curl_easy_setopt(c,CURLOPT_URL,url.v.c_str());curl_easy_setopt(c,CURLOPT_CUSTOMREQUEST,method.v.c_str());curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,follow?1L:0L);curl_easy_setopt(c,CURLOPT_MAXREDIRS,10L);curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,timeout);curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,strut_http_write_cb);curl_easy_setopt(c,CURLOPT_WRITEDATA,&response.body.v);curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,strut_http_header_cb);curl_easy_setopt(c,CURLOPT_HEADERDATA,&response.headers);if(list)curl_easy_setopt(c,CURLOPT_HTTPHEADER,list);if(!body.empty()){curl_easy_setopt(c,CURLOPT_POSTFIELDS,body.data());curl_easy_setopt(c,CURLOPT_POSTFIELDSIZE,static_cast<long>(body.size()));}auto rc=curl_easy_perform(c);if(rc==CURLE_OK){long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);response.status=static_cast<std::int32_t>(status);}if(list)curl_slist_free_all(list);curl_easy_cleanup(c);if(rc!=CURLE_OK)throw strut_checked_error("HttpError",curl_easy_strerror(rc));return response;}
inline strut_http_response http_request(const strut_string& method,const strut_string& url){return http_request(method,url,json::Document(nullptr));}
inline strut_http_response http_get(const strut_string& url){return http_request(strut_string("GET"),url);}
inline strut_http_response http_get_ca(const strut_string& url,const strut_string& ca_file){strut_curl_init();CURL* c=curl_easy_init();if(!c)throw strut_checked_error("HttpError","curl_easy_init failed");strut_http_response response;curl_easy_setopt(c,CURLOPT_URL,url.v.c_str());curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_CAINFO,ca_file.v.c_str());curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,30000L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,strut_http_write_cb);curl_easy_setopt(c,CURLOPT_WRITEDATA,&response.body.v);curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,strut_http_header_cb);curl_easy_setopt(c,CURLOPT_HEADERDATA,&response.headers);auto rc=curl_easy_perform(c);if(rc==CURLE_OK){long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);response.status=static_cast<std::int32_t>(status);}curl_easy_cleanup(c);if(rc!=CURLE_OK)throw strut_checked_error("HttpError",curl_easy_strerror(rc));return response;}
inline json::Document http_get_json(const strut_string& url){return http_get(url).json();}
inline strut_future<strut_http_response> http_get_async(const strut_string& url){return strut_async([url]{return http_get(url);});}
inline strut_future<strut_http_response> http_request_async(const strut_string& method,const strut_string& url,const json::Document& options){return strut_async([method,url,options]{return http_request(method,url,options);});}
#endif

struct strut_server_request {
    strut_string method, path, body;
    std::unordered_map<strut_string,strut_string> headers, query, params;
    json::Document json() const { return strut_json_parse(body); }
};
struct strut_server_response { std::int32_t status=200; strut_string body; strut_string content_type="text/plain; charset=utf-8"; std::unordered_map<strut_string,strut_string> headers; };
inline strut_server_response strut_http_text(const strut_string& s){return {200,s,"text/plain; charset=utf-8",{}};}
inline strut_server_response strut_http_html(const strut_string& s){return {200,s,"text/html; charset=utf-8",{}};}
inline strut_server_response strut_http_json_response(const json::Document& j){return {200,strut_string(j.dump()),"application/json",{}};}
inline std::string strut_trim_ascii(std::string s){while(!s.empty()&&(s.back()=='\r'||s.back()==' '||s.back()=='\t'))s.pop_back();std::size_t i=0;while(i<s.size()&&(s[i]==' '||s[i]=='\t'))++i;return s.substr(i);}
inline void strut_parse_query(const std::string& raw,std::unordered_map<strut_string,strut_string>& out){std::size_t p=0;while(p<=raw.size()){auto amp=raw.find('&',p);auto part=raw.substr(p,amp==std::string::npos?std::string::npos:amp-p);auto eq=part.find('=');out[strut_string(part.substr(0,eq))]=strut_string(eq==std::string::npos?"":part.substr(eq+1));if(amp==std::string::npos)break;p=amp+1;}}
inline bool strut_route_match(const std::string& pattern,const std::string& path,std::unordered_map<strut_string,strut_string>& params){std::stringstream a(pattern),b(path);std::string x,y;while(true){bool ax=static_cast<bool>(std::getline(a,x,'/')),by=static_cast<bool>(std::getline(b,y,'/'));if(!ax||!by)return ax==by;if(x.empty()&&y.empty())continue;if(!x.empty()&&x[0]==':')params[strut_string(x.substr(1))]=strut_string(y);else if(x!=y)return false;}}
class strut_http_server_legacy {
public:
    using handler=std::function<strut_server_response(strut_server_request)>;
    void get(const strut_string& path,handler h){routes_.push_back({"GET",path.v,std::move(h)});} void post(const strut_string& path,handler h){routes_.push_back({"POST",path.v,std::move(h)});}
    void get_async(const strut_string& path,std::function<strut_future<strut_server_response>(strut_server_request)> h){get(path,[h=std::move(h)](strut_server_request r){auto f=h(std::move(r));return strut_await(f);});}
    void post_async(const strut_string& path,std::function<strut_future<strut_server_response>(strut_server_request)> h){post(path,[h=std::move(h)](strut_server_request r){auto f=h(std::move(r));return strut_await(f);});}
    void serve_static(const strut_string& prefix,const std::unordered_map<strut_string,strut_string>& files,const strut_string& fallback=strut_string()){static_prefix_=prefix.v;static_files_=files;static_fallback_=fallback.v;}
    void listen(const strut_string& host,std::int32_t port,std::int32_t max_requests=0){auto l=tcp_listen(host,port);std::int32_t served=0;while(max_requests<=0||served<max_requests){auto c=l.accept();serve_one(c);++served;}l.close();}
private:
)CPP" << R"CPP(    struct route{std::string method,path;handler fn;}; std::vector<route> routes_; std::string static_prefix_; std::string static_fallback_; std::unordered_map<strut_string,strut_string> static_files_;
    static strut_string mime(const std::string& p){auto dot=p.rfind('.');auto e=dot==std::string::npos?std::string():p.substr(dot);if(e==".html")return "text/html; charset=utf-8";if(e==".css")return "text/css; charset=utf-8";if(e==".js")return "application/javascript";if(e==".json")return "application/json";if(e==".svg")return "image/svg+xml";if(e==".png")return "image/png";return "application/octet-stream";}
    static std::string etag(const std::string& data){std::uint64_t h=1469598103934665603ull;for(unsigned char c:data){h^=c;h*=1099511628211ull;}std::ostringstream o;o<<'"'<<std::hex<<h<<'"';return o.str();}
    void serve_one(strut_tcp_socket& sock){std::string raw;for(;;){auto chunk=sock.read(4096).v;if(chunk.empty())break;raw+=chunk;if(raw.find("\r\n\r\n")!=std::string::npos)break;}strut_server_request req;auto line_end=raw.find("\r\n");if(line_end==std::string::npos)return;std::istringstream first(raw.substr(0,line_end));std::string target,version;first>>req.method.v>>target>>version;auto q=target.find('?');req.path=strut_string(target.substr(0,q));if(q!=std::string::npos)strut_parse_query(target.substr(q+1),req.query);auto header_end=raw.find("\r\n\r\n");std::size_t p=line_end+2;std::size_t content_len=0;while(p<header_end){auto e=raw.find("\r\n",p);auto ln=raw.substr(p,e-p);auto colon=ln.find(':');if(colon!=std::string::npos){auto k=strut_trim_ascii(ln.substr(0,colon));auto v=strut_trim_ascii(ln.substr(colon+1));req.headers[strut_string(k)]=strut_string(v);if(k=="Content-Length")content_len=static_cast<std::size_t>(std::strtoull(v.c_str(),nullptr,10));}p=e+2;}if(header_end!=std::string::npos){req.body=strut_string(raw.substr(header_end+4));while(req.body.v.size()<content_len){auto more=sock.read(static_cast<std::int64_t>(content_len-req.body.v.size())).v;if(more.empty())break;req.body.v+=more;}}
      strut_server_response res;bool found=false;for(auto& r:routes_){if(r.method!=req.method.v)continue;req.params.clear();if(strut_route_match(r.path,req.path.v,req.params)){res=r.fn(req);found=true;break;}}if(!found&&!static_files_.empty()&&req.method.v=="GET"){std::string key=req.path.v;if(!static_prefix_.empty()&&key.rfind(static_prefix_,0)==0)key=key.substr(static_prefix_.size());while(!key.empty()&&key.front()=='/')key.erase(key.begin());if(key.empty())key="index.html";if(key.find("..")!=std::string::npos){res.status=400;res.body="Bad Request";found=true;}else{auto it=static_files_.find(strut_string(key));if(it==static_files_.end()&&!static_fallback_.empty())it=static_files_.find(strut_string(static_fallback_));if(it!=static_files_.end()){res.status=200;res.body=it->second;res.content_type=mime(key);res.headers[strut_string("ETag")]=strut_string(etag(res.body.v));res.headers[strut_string("Cache-Control")]=strut_string("public, max-age=0, must-revalidate");found=true;}}}if(!found){res.status=404;res.body="Not Found";}std::ostringstream out;out<<"HTTP/1.1 "<<res.status<<" "<<(res.status==200?"OK":res.status==404?"Not Found":"Response")<<"\r\nContent-Type: "<<res.content_type.v<<"\r\nContent-Length: "<<res.body.v.size()<<"\r\nConnection: close\r\n";for(auto& h:res.headers)out<<h.first.v<<": "<<h.second.v<<"\r\n";out<<"\r\n"<<res.body.v;sock.write(strut_string(out.str()));sock.close();}
};

#ifdef STRUT_USE_SQLITE
inline void strut_sqlite_bind(sqlite3_stmt* st,const json::Document& params){if(params.type!=json::Type::Array)return;for(std::size_t i=0;i<params.array.size();++i){const auto& v=params.array[i];int n=static_cast<int>(i+1);switch(v.type){case json::Type::Null:sqlite3_bind_null(st,n);break;case json::Type::Boolean:sqlite3_bind_int(st,n,v.boolean?1:0);break;case json::Type::Number:case json::Type::StrNumber:sqlite3_bind_double(st,n,v.is_number()?std::strtod(v.type==json::Type::StrNumber?v.string.c_str():v.dump().c_str(),nullptr):0.0);break;case json::Type::String:sqlite3_bind_text(st,n,v.string.c_str(),-1,SQLITE_TRANSIENT);break;default:{auto text=v.dump();sqlite3_bind_text(st,n,text.c_str(),-1,SQLITE_TRANSIENT);break;}}}}
struct strut_sqlite_state{sqlite3* db=nullptr;~strut_sqlite_state(){if(db)sqlite3_close(db);}};
class strut_sqlite_db {
public:
    strut_sqlite_db():s_(std::make_shared<strut_sqlite_state>()){} explicit strut_sqlite_db(sqlite3* db):s_(std::make_shared<strut_sqlite_state>()){s_->db=db;}
    void close(){if(s_&&s_->db){sqlite3_close(s_->db);s_->db=nullptr;}}
    void exec(const strut_string& sql) const{exec(sql,json::Document::make_array());}
    void exec(const strut_string& sql,const json::Document& params) const{auto db=handle();sqlite3_stmt* st=nullptr;if(sqlite3_prepare_v2(db,sql.v.c_str(),-1,&st,nullptr)!=SQLITE_OK)throw strut_checked_error("SqliteError",sqlite3_errmsg(db));strut_sqlite_bind(st,params);int rc=sqlite3_step(st);if(rc!=SQLITE_DONE&&rc!=SQLITE_ROW){std::string m=sqlite3_errmsg(db);sqlite3_finalize(st);throw strut_checked_error("SqliteError",m);}sqlite3_finalize(st);}
    json::Document query(const strut_string& sql) const{return query(sql,json::Document::make_array());}
    json::Document query(const strut_string& sql,const json::Document& params) const{auto db=handle();sqlite3_stmt* st=nullptr;if(sqlite3_prepare_v2(db,sql.v.c_str(),-1,&st,nullptr)!=SQLITE_OK)throw strut_checked_error("SqliteError",sqlite3_errmsg(db));strut_sqlite_bind(st,params);json::Document rows=json::Document::make_array();while(sqlite3_step(st)==SQLITE_ROW){json::Document row=json::Document::make_object();for(int i=0;i<sqlite3_column_count(st);++i){const char* n=sqlite3_column_name(st,i);switch(sqlite3_column_type(st,i)){case SQLITE_INTEGER:row[n]=static_cast<double>(sqlite3_column_int64(st,i));break;case SQLITE_FLOAT:row[n]=sqlite3_column_double(st,i);break;case SQLITE_TEXT:row[n]=reinterpret_cast<const char*>(sqlite3_column_text(st,i));break;case SQLITE_NULL:row[n]=json::Document(nullptr);break;default:row[n]="<blob>";}}rows.push_back(row);}sqlite3_finalize(st);return rows;}
    void transaction(std::function<void()> body){exec("BEGIN");try{body();exec("COMMIT");}catch(...){try{exec("ROLLBACK");}catch(...){ }throw;}}
private: sqlite3* handle() const{if(!s_||!s_->db)throw strut_checked_error("SqliteError","database is closed");return s_->db;} std::shared_ptr<strut_sqlite_state> s_;
};
inline strut_sqlite_db strut_sqlite_open(const strut_string& path){sqlite3* db=nullptr;if(sqlite3_open(path.v.c_str(),&db)!=SQLITE_OK){std::string m=db?sqlite3_errmsg(db):"sqlite open failed";if(db)sqlite3_close(db);throw strut_checked_error("SqliteError",m);}return strut_sqlite_db(db);}
#endif

inline strut_string strut_embed_file(const strut_string& path){std::ifstream f(path.v,std::ios::binary);if(!f)throw strut_checked_error("EmbedError","unable to open embedded file");std::ostringstream o;o<<f.rdbuf();return strut_string(o.str());}
inline std::unordered_map<strut_string,strut_string> strut_embed_dir(const strut_string& root){std::unordered_map<strut_string,strut_string> out;for(auto& e:std::filesystem::recursive_directory_iterator(root.v)){if(!e.is_regular_file())continue;std::ifstream f(e.path(),std::ios::binary);std::ostringstream o;o<<f.rdbuf();out[strut_string(std::filesystem::relative(e.path(),root.v).generic_string())]=strut_string(o.str());}return out;}

template<class... T> void strut_print(const T&... v){((std::cout<<v),...);std::cout<<'\n';}
)CPP";if(has(RuntimeComponentId::http_server))emit_http_server_lifecycle(o,true,has(RuntimeComponentId::http_server_tls));emit_entry_support(o);for(auto&s:p.statements)stmt(o,*s,0);r.cpp=o.str();return r;}

namespace {
std::string env_flags(const char* name) { const char* value=std::getenv(name); return value&&*value ? std::string(" ")+value : std::string(); }
int run_native_command(const std::string& command) {
#ifdef _WIN32
    // cmd.exe needs an extra outer quote when the executable path itself is quoted (e.g. Program Files\...\cl.exe).
    return std::system((std::string("\"")+command+"\"").c_str());
#else
    return std::system(command.c_str());
#endif
}
std::string native_failure(const IRProgram& program,std::string_view phase) {
    std::ostringstream out;
    out << "native " << phase << " failed";
    const auto libraries=analyze_runtime_components(program).link_libraries();
    if(!libraries.empty()) {
        out << "\nhelp: install the development package for required native ";
        out << (libraries.size()==1?"library ":"libraries ");
        for(std::size_t i=0;i<libraries.size();++i){if(i)out<<", ";out<<libraries[i];}
        out << " and ensure its headers and linker path are visible to the configured C++ toolchain";
#if defined(__linux__)
        out << "\nhelp: on Debian/Ubuntu install ";
        bool first=true;for(const auto& library:libraries){if(!first)out<<" ";first=false;if(library=="curl")out<<"libcurl4-openssl-dev";else if(library=="sqlite3")out<<"libsqlite3-dev";else out<<library<<" development files";}
#elif defined(__APPLE__)
        out << "\nhelp: on macOS install the dependency with Homebrew if it is not provided by the SDK";
#elif defined(_WIN32)
        out << "\nhelp: on Windows make the dependency available to MSVC (for example with vcpkg)";
#endif
    } else {
        out << "\nhelp: verify the configured C++20 compiler and native link options";
    }
    out << "\nnote: the native toolchain output above is secondary detail";
    return out.str();
}
std::string target_env_name(const std::string& target) {
    std::string out = "STRUT_CXX_";
    for (unsigned char c : target) out += std::isalnum(c) ? static_cast<char>(std::toupper(c)) : '_';
    return out;
}
std::filesystem::path executable_path() {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size()) return {};
    buffer.resize(length);
    return std::filesystem::path(buffer);
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
    buffer.resize(std::char_traits<char>::length(buffer.c_str()));
    return std::filesystem::weakly_canonical(buffer);
#else
    std::string buffer(4096, '\0');
    const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (length <= 0 || static_cast<std::size_t>(length) == buffer.size()) return {};
    buffer.resize(static_cast<std::size_t>(length));
    return std::filesystem::path(buffer);
#endif
}
std::filesystem::path jsonic_include_dir() {
    if (const char* override_dir = std::getenv("STRUT_JSONIC_INCLUDE_DIR"); override_dir && *override_dir)
        return override_dir;
    const auto executable = executable_path();
    if (!executable.empty()) {
        const auto build_tree = executable.parent_path() / "share" / "strut" / "jsonic";
        if (std::filesystem::exists(build_tree / "json.h")) return build_tree;
        const auto installed = executable.parent_path().parent_path() / "share" / "strut" / "jsonic";
        if (std::filesystem::exists(installed / "json.h")) return installed;
    }
    return {};
}
bool target_windows(const std::string& t) { return t == "windows-x64" || (t == "native"
#ifdef _WIN32
    && true
#else
    && false
#endif
); }
bool target_macos(const std::string& t) { return t.rfind("macos-", 0) == 0 || (t == "native"
#ifdef __APPLE__
    && true
#else
    && false
#endif
); }
std::string target_compiler(const std::string& target, bool& msvc) {
    msvc = false;
    if (target == "native") {
        const char* env = std::getenv("CXX");
#ifdef _WIN32
        msvc = true; return env && *env ? env : STRUT_HOST_CXX;
#else
        return env && *env ? env : STRUT_HOST_CXX;
#endif
    }
    const std::string key = target_env_name(target);
    if (const char* env = std::getenv(key.c_str()); env && *env) return env;
    if (target == "linux-x64") return "x86_64-linux-gnu-g++";
    if (target == "linux-arm64") return "aarch64-linux-gnu-g++";
    if (target == "windows-x64") return "x86_64-w64-mingw32-g++";
    if (target == "macos-arm64") return "oa64-clang++";
    if (target == "macos-x64") return "o64-clang++";
    return "c++";
}
void append_runtime_link_libraries(std::string& command,const IRProgram& program,bool msvc) {
    const auto resolution=analyze_runtime_components(program);
    for(const auto& library:resolution.link_libraries()) {
        if(msvc) command += library=="curl" ? " libcurl.lib" : (library=="ssl"||library=="crypto" ? " lib"+library+".lib" : " "+library+".lib");
        else command += " -l"+library;
    }
}
}
std::string classify_native_failure(const IRProgram& program,std::string_view phase){return native_failure(program,phase);}
bool CppBackend::compile_object(const IRProgram& p,const std::filesystem::path& object,const std::filesystem::path& generated_cpp,std::string& error,const NativeLinkOptions& link) const {
 auto g=generate(p);if(!g.ok()){error=g.error;return false;}std::error_code ec;std::filesystem::create_directories(object.parent_path(),ec);if(ec){error=ec.message();return false;}std::filesystem::create_directories(generated_cpp.parent_path(),ec);if(ec){error=ec.message();return false;}{std::ofstream f(generated_cpp);if(!f){error="cannot write generated C++ source";return false;}f<<g.cpp;}
bool msvc=false; std::string cxx=target_compiler(link.target,msvc); std::string cmd; const auto jsonic=jsonic_include_dir().string();
 if(msvc) cmd="\""+cxx+"\" /nologo /std:c++20 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /c "+(link.release?"/O2 /Gy ":"/Od /Zi ")+env_flags("STRUT_CXXFLAGS")+" /I\""+jsonic+"\" \""+generated_cpp.string()+"\" /Fo:\""+object.string()+"\"";
 else cmd="\""+cxx+"\" -std=c++20 "+(link.release?"-O2 -flto -ffunction-sections -fdata-sections ":"-O0 -g ")+env_flags("STRUT_CXXFLAGS")+" -I\""+jsonic+"\" -c \""+generated_cpp.string()+"\" -o \""+object.string()+"\"";
 if(run_native_command(cmd)!=0){error=native_failure(p,"C++ object compilation");return false;}return true;
}

bool CppBackend::link_objects(const IRProgram& p,const std::vector<std::filesystem::path>& objects,const std::filesystem::path& output,std::string& error,const NativeLinkOptions& link) const {
 if(objects.empty()){error="no object files to link";return false;}
bool msvc=false; std::string cxx=target_compiler(link.target,msvc); std::string cmd="\""+cxx+"\"";
 if(msvc){
 cmd+=" /nologo "+std::string(link.fully_static?"/MT ":"/MD ");for(const auto&o:objects)cmd+=" \""+o.string()+"\"";cmd+=" /Fe:\""+output.string()+"\"";bool ls=false;for(const auto&d:link.search_paths){if(!ls){cmd+=" /link";ls=true;}cmd+=" /LIBPATH:\""+d.string()+"\"";}for(const auto&lib:link.libraries){std::filesystem::path lp(lib.value);cmd+=" "+(lp.has_extension()?"\""+lib.value+"\"":lib.value+".lib");}if(!ls)cmd+=" /link";cmd+=" ws2_32.lib";if(link.release)cmd+=" /OPT:REF /OPT:ICF /LTCG";else cmd+=" /DEBUG";
 } else {
 if(target_macos(link.target)&&link.fully_static){error="fully static final executables are not supported for macOS targets";return false;}

 for(const auto& o : objects) { cmd += " \"" + o.string() + "\""; }
 cmd += " -o \"" + output.string() + "\"";
 if (link.fully_static) cmd += " -static";
 for (const auto& d : link.search_paths) { cmd += " -L\"" + d.string() + "\""; }
 for (const auto& lib : link.libraries) { std::filesystem::path lp(lib.value); if(lp.has_extension()||lib.value.find('/')!=std::string::npos){cmd+=" \""+lib.value+"\"";continue;}
#ifdef __APPLE__
 if(lib.mode==NativeLinkMode::static_link){error="macOS static native libraries must be supplied as an explicit .a path";return false;}cmd+=" -l"+lib.value;
#else
 if(lib.mode==NativeLinkMode::static_link)cmd+=" -Wl,-Bstatic -l"+lib.value+" -Wl,-Bdynamic";else cmd+=" -l"+lib.value;
#endif
 }if(!link.release){
#ifndef __APPLE__
 cmd+=" -rdynamic";
#endif
 }if(link.release){cmd+=" -flto";
#ifdef __APPLE__
 cmd+=" -Wl,-dead_strip -Wl,-x";
#else
 cmd+=" -Wl,--gc-sections -s";
#endif
 }
}
 if(!msvc) cmd += env_flags("STRUT_LDFLAGS");
 if(!msvc && target_windows(link.target)) cmd += " -lws2_32";
 append_runtime_link_libraries(cmd,p,msvc);
 if(run_native_command(cmd)!=0){error=native_failure(p,"linking");return false;}return true;
}

bool CppBackend::compile(const IRProgram& p,const std::filesystem::path& output,std::string& error,const NativeLinkOptions& link) const {if(link.target!="native"){auto obj=output;obj += ".strut.o";auto gen=output;gen += ".strut.cpp";if(!compile_object(p,obj,gen,error,link))return false;bool ok=link_objects(p,{obj},output,error,link);std::error_code ec;std::filesystem::remove(obj,ec);std::filesystem::remove(gen,ec);return ok;}auto g=generate(p);if(!g.ok()){error=g.error;return false;}auto tmp=output;tmp += ".strut.cpp";{std::ofstream f(tmp);if(!f){error="cannot write temporary C++ source";return false;}f<<g.cpp;}
const char* env=std::getenv("CXX");std::string cxx=env&&*env?env:STRUT_HOST_CXX;std::string cmd;const auto jsonic=jsonic_include_dir().string();
#ifdef _WIN32
 cmd="\""+cxx+"\" /nologo /std:c++20 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX "+(link.fully_static?"/MT ":"/MD ")+(link.release?"/O2 /Gy ":"/Od /Zi ")+env_flags("STRUT_CXXFLAGS")+" /I\""+jsonic+"\" \""+tmp.string()+"\" /Fe:\""+output.string()+"\"";
 bool link_section=false;
 for(const auto& d:link.search_paths){if(!link_section){cmd+=" /link";link_section=true;}cmd+=" /LIBPATH:\""+d.string()+"\"";}
 for(const auto& lib:link.libraries){std::filesystem::path lp(lib.value);cmd+=" "+(lp.has_extension()?"\""+lib.value+"\"":lib.value+".lib");}
 if(!link_section){cmd+=" /link";link_section=true;}cmd+=" ws2_32.lib";
 if(link.release){cmd+=" /OPT:REF /OPT:ICF";}else{cmd+=" /DEBUG";}
#else
 #ifdef __APPLE__
 if(link.fully_static){error="fully static final executables are not supported by the default macOS toolchain";std::error_code ec;std::filesystem::remove(tmp,ec);return false;}
 #endif
 cmd="\""+cxx+"\" -std=c++20 "+(link.release?"-O2 -ffunction-sections -fdata-sections ":"-O0 -g ")+env_flags("STRUT_CXXFLAGS")+" -I\""+jsonic+"\" \""+tmp.string()+"\" -o \""+output.string()+"\"";
 if(link.fully_static)cmd+=" -static";
 for(const auto& d:link.search_paths)cmd+=" -L\""+d.string()+"\"";
 for(const auto& lib:link.libraries){std::filesystem::path lp(lib.value);if(lp.has_extension()||lib.value.find('/')!=std::string::npos){cmd+=" \""+lib.value+"\"";continue;}
 #ifdef __APPLE__
 if(lib.mode==NativeLinkMode::static_link){error="macOS static native libraries must be supplied as an explicit .a path";std::error_code ec;std::filesystem::remove(tmp,ec);return false;}cmd+=" -l"+lib.value;
 #else
 if(lib.mode==NativeLinkMode::static_link)cmd+=" -Wl,-Bstatic -l"+lib.value+" -Wl,-Bdynamic";else if(lib.mode==NativeLinkMode::dynamic_link)cmd+=" -Wl,-Bdynamic -l"+lib.value;else cmd+=" -l"+lib.value;
 #endif
 }
 if(!link.release){
 #ifndef __APPLE__
 cmd+=" -rdynamic";
 #endif
 }
 if(link.release){
 #ifdef __APPLE__
 cmd+=" -Wl,-dead_strip -Wl,-x";
 #else
 cmd+=" -Wl,--gc-sections -s";
 #endif
 }
#endif
#ifndef _WIN32
 cmd += env_flags("STRUT_LDFLAGS");
#endif
 append_runtime_link_libraries(cmd,p,
#ifdef _WIN32
 true
#else
 false
#endif
 );
 int rc=run_native_command(cmd);std::error_code ec;std::filesystem::remove(tmp,ec);if(rc!=0){error=native_failure(p,"C++ compilation/linking");return false;}return true;}
}
