#include "strut/codegen.h"
#include "strut/type.h"
#include <cstdlib>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
namespace strut { namespace {
std::string cpp_type(std::string t);
std::string safe_symbol(std::string s){for(char& c:s)if(!std::isalnum(static_cast<unsigned char>(c)))c='_';return s;}
std::string normalized_operator_type(std::string t){if(t.rfind("ref<",0)==0&&t.back()=='>')t=t.substr(4,t.size()-5);if(t.rfind("const ",0)==0)t=t.substr(6);return t;}
std::string operator_param_cpp(const std::string& t,const std::string& name){if(t.rfind("ref<const ",0)==0&&t.back()=='>')return "const "+cpp_type(t.substr(10,t.size()-11))+"& "+name;if(t.rfind("ref<",0)==0&&t.back()=='>')return cpp_type(t.substr(4,t.size()-5))+"& "+name;return cpp_type(t)+" "+name;}
std::string operator_return_cpp(const std::string& t){if(t.rfind("ref<const ",0)==0&&t.back()=='>')return "const "+cpp_type(t.substr(10,t.size()-11))+"&";if(t.rfind("ref<",0)==0&&t.back()=='>')return cpp_type(t.substr(4,t.size()-5))+"&";return cpp_type(t);}
std::string cpp_type(std::string t){
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
    if(t.rfind("map<",0)==0 && t.back()=='>'){auto inner=t.substr(4,t.size()-5);int depth=0;std::size_t comma=std::string::npos;for(std::size_t i=0;i<inner.size();++i){if(inner[i]=='<')++depth;else if(inner[i]=='>')--depth;else if(inner[i]==','&&depth==0){comma=i;break;}}if(comma!=std::string::npos)return "std::map<"+cpp_type(inner.substr(0,comma))+","+cpp_type(inner.substr(comma+1))+">";}
    if(t.size()>2 && t.compare(t.size()-2,2,"[]")==0) return "std::vector<"+cpp_type(t.substr(0,t.size()-2))+">";
    if (auto extent = array_extent(t)) { const auto open=t.rfind('['); return "std::array<"+cpp_type(t.substr(0,open))+","+std::to_string(*extent)+">"; }
    if(t=="void") return "void";
    if(t=="bool") return "bool";
    if(t=="string") return "strut_string";
    if(t=="json") return "json::Document";
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
    if(t=="mutex") return "strut_mutex";
    if(t.rfind("future<",0)==0&&t.back()=='>') return "strut_future<"+cpp_type(t.substr(7,t.size()-8))+">";
    if(t.rfind("channel<",0)==0&&t.back()=='>') return "strut_channel<"+cpp_type(t.substr(8,t.size()-9))+">";
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
std::string expr(const IRExpr& e);
void stmt(std::ostringstream& o,const IRStmt& s,int n);
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
        case IRExpr::Kind::json_object:return json_expr(e);
        case IRExpr::Kind::struct_literal:{std::string out="[&](){"+e.text+" value{};";for(std::size_t i=0;i<e.names.size();++i)out+="value."+e.names[i]+"="+expr(*e.arguments[i])+";";return out+"return value;}()";}
        case IRExpr::Kind::grouping:return "("+expr(*e.left)+")";
        case IRExpr::Kind::unary:if(e.text=="await")return "strut_await("+expr(*e.right)+")";if(e.text=="*"&&e.right&&e.right->type_name.rfind("ptr<",0)==0)return "strut_deref("+expr(*e.right)+")";return e.text+expr(*e.right);
        case IRExpr::Kind::postfix:return expr(*e.left)+e.text;
        case IRExpr::Kind::binary:if(e.text=="??")return "strut_coalesce("+expr(*e.left)+","+expr(*e.right)+")";return "("+expr(*e.left)+" "+e.text+" "+expr(*e.right)+")";
        case IRExpr::Kind::safe_member:return "strut_safe_member("+expr(*e.left)+",[](const auto& value){return value."+e.text+";})";
        case IRExpr::Kind::member:{if(e.text.rfind("::",0)==0)return expr(*e.left)+e.text; if(e.text.rfind("->",0)==0)return expr(*e.left)+e.text; if(e.left && !e.left->type_name.empty() && e.left->type_name.back()=='?') return "(*"+expr(*e.left)+")."+e.text; if(e.left&&e.left->kind==IRExpr::Kind::identifier&&e.left->text=="json"){if(e.text=="parse")return "strut_json_parse";if(e.text=="stringify")return "strut_json_stringify";if(e.text=="pretty")return "strut_json_pretty";if(e.text=="encode")return "strut_json_value";}std::string m=e.text;if(m=="push")m="push_back";else if(m=="pop")m="pop_back";else if(m=="length")m="size";else if(m=="remove")m="erase";if(e.left&&e.left->type_name.rfind("ref<",0)==0)return "(*"+expr(*e.left)+")."+m;if(e.left&&e.left->type_name.rfind("ptr<",0)==0)return expr(*e.left)+"->"+m;return expr(*e.left)+"."+m;}
        case IRExpr::Kind::index:{if(e.left->type_name=="json"){std::string k=(e.right->kind==IRExpr::Kind::string_literal)?e.right->text:expr(*e.right);return expr(*e.left)+"["+k+"]";}return expr(*e.left)+".at("+expr(*e.right)+")";}
        case IRExpr::Kind::call:{
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
            std::string name=expr(*e.left); if(name=="ptr"&&e.arguments.size()==1)return "strut_ptr("+expr(*e.arguments[0])+")"; if(name=="ref"&&e.arguments.size()==1)return "strut_make_ref("+expr(*e.arguments[0])+")"; if(name=="weak"&&e.arguments.size()==1)return "strut_weak("+expr(*e.arguments[0])+")"; if(name=="raw"&&e.arguments.size()==1)return "strut_raw("+expr(*e.arguments[0])+")"; if(name=="print"){std::string out="strut_print(";for(size_t i=0;i<e.arguments.size();++i){if(i)out+=",";out+=expr(*e.arguments[i]);}return out+")";}
            if(name=="input") name="strut_input"; else if(name=="istream") name="strut_istream"; else if(name=="ostream") name="strut_ostream"; else if(name=="sstream") name="strut_sstream"; else if(name=="ifstream") name="strut_ifstream"; else if(name=="ofstream") name="strut_ofstream"; else if(name=="join") name="strut_join"; else if(name=="to_int") name="strut_to_int"; else if(name=="to_double") name="strut_to_double"; else if(name=="to_string") name="strut_to_string"; else if(name=="exists") name="strut_fs_exists"; else if(name=="make_dir") name="strut_fs_make_dir"; else if(name=="remove") name="strut_fs_remove"; else if(name=="copy") name="strut_fs_copy"; else if(name=="move") name="strut_fs_move"; else if(name=="touch") name="strut_fs_touch"; else if(name=="ls") name="strut_fs_ls"; else if(name=="env") name="strut_env"; else if(name=="set_env") name="strut_set_env"; else if(name=="unset_env") name="strut_unset_env"; else if(name=="now_ms") name="strut_now_ms"; else if(name=="unix_ms") name="strut_unix_ms"; else if(name=="sleep_ms") name="strut_sleep_ms"; else if(name=="exec") name="strut_exec"; else if(name=="exec_shell") name="strut_exec_shell"; else if(name=="process") name="strut_process"; else if(name=="pipe_exec") name="strut_pipe_exec"; else if(name=="thread") name="strut_thread"; else if(name=="mutex") name="strut_mutex";
            std::string out=name+"(";for(size_t i=0;i<e.arguments.size();++i){if(i)out+=",";out+=expr(*e.arguments[i]);}return out+")";
        }
        case IRExpr::Kind::lambda:{std::ostringstream o;o<<"[=](";for(std::size_t i=0;i<e.lambda_parameters.size();++i){if(i)o<<",";{const auto& tn=e.lambda_parameters[i].type.name;bool generic=!tn.empty();for(unsigned char c:tn)if(std::islower(c))generic=false;o<<(generic?"auto":cpp_type(tn))<<" "<<e.lambda_parameters[i].name;}}o<<")";if(e.lambda_async){o<<" { return strut_async([=]() mutable";if(e.lambda_expression)o<<" { return "<<expr(*e.lambda_expression)<<"; }); }";else{o<<" {\n";for(const auto& c:e.lambda_body)stmt(o,*c,8);o<<"    });\n}";}}else if(e.lambda_expression){o<<" { return "<<expr(*e.lambda_expression)<<"; }";}else{o<<" {\n";for(const auto& c:e.lambda_body)stmt(o,*c,4);o<<"}";}return o.str();}
    } return {};
}
void stmt(std::ostringstream& o,const IRStmt& s,int n){std::string pad(n,' ');
    switch(s.kind){
        case IRStmt::Kind::declaration:o<<pad<<(s.is_const?"const ":"")<<cpp_type(s.type_name)<<" "<<s.name;if(s.value){o<<" = ";if(!s.overload_name.empty())o<<s.overload_name<<"("<<expr(*s.value)<<")";else o<<expr(*s.value);}else o<<"{}";o<<";\n";break;
        case IRStmt::Kind::assignment:if(!s.overload_name.empty())o<<pad<<s.overload_name<<"("<<(s.target?expr(*s.target):s.name)<<","<<expr(*s.value)<<");\n";else o<<pad<<(s.target?expr(*s.target):s.name)<<" "<<s.op<<" "<<expr(*s.value)<<";\n";break;
        case IRStmt::Kind::expression:o<<pad<<expr(*s.value)<<";\n";break;
        case IRStmt::Kind::return_stmt:o<<pad<<"return"<<(s.value?" "+expr(*s.value):"")<<";\n";break;
        case IRStmt::Kind::throw_stmt:{std::string en="Error";std::string msg="\"checked error\"";if(s.value&&s.value->kind==IRExpr::Kind::call&&s.value->left&&s.value->left->kind==IRExpr::Kind::identifier){en=s.value->left->text;if(!s.value->arguments.empty())msg=(s.value->arguments[0]->kind==IRExpr::Kind::string_literal?s.value->arguments[0]->text:expr(*s.value->arguments[0]));}o<<pad<<"throw strut_checked_error(\""<<en<<"\","<<msg<<");\n";break;}
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
        case IRStmt::Kind::range_for:o<<pad<<"for (auto& "<<s.name<<" : "<<expr(*s.value)<<") {\n";for(auto&c:s.body)stmt(o,*c,n+4);o<<pad<<"}\n";break;
        case IRStmt::Kind::enum_decl:{o<<pad<<"enum class "<<s.name<<" : std::int32_t {";for(std::size_t i=0;i<s.enum_names.size();++i){if(i)o<<",";o<<s.enum_names[i]<<"="<<s.enum_values[i];}o<<"};\n";break;}
        case IRStmt::Kind::struct_decl:{if(!s.generic_parameters.empty()){o<<pad<<"template<";for(std::size_t i=0;i<s.generic_parameters.size();++i){if(i)o<<",";o<<"class "<<s.generic_parameters[i];}o<<">\n";}o<<pad<<"struct "<<s.name; if(!s.bases.empty()){o<<" : ";for(std::size_t i=0;i<s.bases.size();++i){if(i)o<<", ";o<<"public "<<cpp_type(s.bases[i]);}} o<<" {\n";for(const auto& f:s.fields)o<<pad<<"    "<<cpp_type(f.type.name)<<" "<<f.name<<"{};\n";for(const auto& m:s.body){o<<pad<<"    "<<cpp_type(m->return_type)<<" "<<m->name<<"(";for(std::size_t i=0;i<m->parameters.size();++i){if(i)o<<",";o<<cpp_type(m->parameters[i].type.name)<<" "<<m->parameters[i].name;}o<<")";if(!m->has_body){o<<";\n";}else{o<<" {\n";for(const auto& c:m->body)stmt(o,*c,n+8);o<<pad<<"    }\n";}}o<<pad<<"};\n";break;}
        case IRStmt::Kind::operator_decl:{
            if(s.parameters.size()==2 && (s.op==":="||s.op=="=")){
                const std::string dest=normalized_operator_type(s.parameters[0].type.name),src=normalized_operator_type(s.parameters[1].type.name);
                const std::string helper="strut_op_"+(s.op==":="?std::string("init"):std::string("assign"))+"_"+safe_symbol(dest)+"_"+safe_symbol(src);
                if(s.op==":="){o<<cpp_type(dest)<<" "<<helper<<"("<<operator_param_cpp(s.parameters[1].type.name,s.parameters[1].name)<<") {\n";o<<"    "<<cpp_type(dest)<<" "<<s.parameters[0].name<<"{};\n";if(s.value&&s.value->kind==IRExpr::Kind::lambda){if(s.value->lambda_expression)o<<"    (void)"<<expr(*s.value->lambda_expression)<<";\n";else for(const auto& c:s.value->lambda_body)stmt(o,*c,4);}else for(const auto& c:s.body)stmt(o,*c,4);o<<"    return "<<s.parameters[0].name<<";\n}\n";}
                else{o<<"void "<<helper<<"("<<cpp_type(dest)<<"& "<<s.parameters[0].name<<","<<operator_param_cpp(s.parameters[1].type.name,s.parameters[1].name)<<") {\n";if(s.value&&s.value->kind==IRExpr::Kind::lambda){if(s.value->lambda_expression)o<<"    (void)"<<expr(*s.value->lambda_expression)<<";\n";else for(const auto& c:s.value->lambda_body)stmt(o,*c,4);}else for(const auto& c:s.body)stmt(o,*c,4);o<<"}\n";}break;
            }
            if(!s.generic_parameters.empty()){o<<"template<";for(std::size_t i=0;i<s.generic_parameters.size();++i){if(i)o<<",";o<<"class "<<s.generic_parameters[i];}o<<">\n";}o<<operator_return_cpp(s.return_type)<<" operator"<<s.op<<"(";for(std::size_t i=0;i<s.parameters.size();++i){if(i)o<<",";o<<operator_param_cpp(s.parameters[i].type.name,s.parameters[i].name);}o<<")";if(!s.has_body){o<<";\n";break;}o<<" {\n";if(s.value&&s.value->kind==IRExpr::Kind::lambda){if(s.value->lambda_expression)o<<std::string(n+4,' ')<<"return "<<expr(*s.value->lambda_expression)<<";\n";else for(const auto& c:s.value->lambda_body)stmt(o,*c,n+4);}else for(const auto& c:s.body)stmt(o,*c,n+4);o<<"}\n";break;}
        case IRStmt::Kind::function_decl:{if(s.is_extern_c)o<<"extern \"C\" ";if(!s.generic_parameters.empty()){o<<"template<";for(std::size_t i=0;i<s.generic_parameters.size();++i){if(i)o<<",";o<<"class "<<s.generic_parameters[i];}o<<">\n";}const bool main_void=(s.name=="main"&&s.return_type=="void"&&!s.is_async);o<<(s.is_async?("strut_future<"+cpp_type(s.return_type)+">"):(main_void?"int":cpp_type(s.return_type)))<<" ";if(!s.owner.empty())o<<s.owner<<"::";o<<s.name<<"(";for(size_t i=0;i<s.parameters.size();++i){if(i)o<<",";o<<cpp_type(s.parameters[i].type.name)<<" "<<s.parameters[i].name;}o<<")";if(!s.has_body){o<<";\n";break;}o<<" {\n";if(s.is_async){o<<"    return strut_async([=]() mutable {\n";for(auto&c:s.body)stmt(o,*c,n+8);o<<"    });\n";}else{for(auto&c:s.body){if(main_void&&c->kind==IRStmt::Kind::return_stmt&&!c->value){o<<std::string(n+4,' ')<<"return 0;\n";}else stmt(o,*c,n+4);}}o<<"}\n";break;}
        default:break;
    }}
}
CodegenResult CppBackend::generate(const IRProgram& p) const {CodegenResult r;std::ostringstream o;o<<"#include \"json.h\"\n#include <cstdint>\n#include <iostream>\n#include <string>\n#include <vector>\n#include <array>\n#include <map>\n#include <stdexcept>\n#include <charconv>\n#include <algorithm>\n#include <cctype>\n#include <utility>\n#include <optional>\n#include <memory>\n#include <type_traits>\n#include <functional>\n#include <filesystem>\n#include <fstream>\n#include <sstream>\n#include <chrono>\n#include <thread>\n#include <mutex>\n#include <condition_variable>\n#include <queue>\n#include <future>\n#include <cerrno>\n#include <cstring>\n#ifdef _WIN32\n#include <windows.h>\n#include <winsock2.h>\n#include <ws2tcpip.h>\n#else\n#include <sys/types.h>\n#include <sys/wait.h>\n#include <sys/socket.h>\n#include <netdb.h>\n#include <arpa/inet.h>\n#include <netinet/in.h>\n#include <unistd.h>\n#endif\n";
o<<R"CPP(
template<class T> class strut_ref {
public:
    explicit strut_ref(T& value):p_(&value){}
    strut_ref(const strut_ref&)=default;
    strut_ref& operator=(const strut_ref&)=delete;
    T& operator*() const { return *p_; }
    T* operator->() const { return p_; }
private:
    T* p_;
};
template<class T> strut_ref<T> strut_make_ref(T& value){return strut_ref<T>(value);}
struct strut_checked_error : std::runtime_error { std::string type; strut_checked_error(std::string t,const std::string& m):std::runtime_error(m),type(std::move(t)){} };
struct strut_null_t {
    template<class T> operator std::optional<T>() const { return std::nullopt; }
    template<class T> operator std::shared_ptr<T>() const { return {}; }
    template<class T> operator std::weak_ptr<T>() const { return {}; }
    template<class T> operator T*() const { return nullptr; }
};
constexpr strut_null_t strut_null{};
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
template<class T> T& strut_deref(const std::shared_ptr<T>& value){if(!value)throw std::runtime_error("null ptr<T> dereference");return *value;}
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
inline bool operator<(const strut_string&a,const strut_string&b){return a.v<b.v;}

class strut_ostream {
public:
    strut_ostream()=default; explicit strut_ostream(std::ostream& s):p_(&s){}
    template<class T> strut_ostream& write_value(const T& v){if(!p_)throw strut_checked_error("StreamError","output stream is not open");(*p_)<<v;return *this;}
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
    strut_string read_all(){if(!file_.is_open())throw strut_checked_error("StreamError","ifstream.read_all on closed stream");std::ostringstream ss;ss<<file_.rdbuf();return ss.str();}
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
inline strut_endl_t endl{};
template<class T> strut_ostream& operator<<(strut_ostream& s,const T& value){return s.write_value(value);}
inline strut_ostream& operator<<(strut_ostream& s,strut_endl_t){s.write_value('\n');s.flush();return s;}
template<class T> strut_istream& operator>>(strut_istream& s,T& value){return s.read_value(value);}
inline strut_istream& operator>>(strut_istream& s,strut_string& value){std::string tmp;s.read_value(tmp);value=strut_string(tmp);return s;}
inline strut_istream in{std::cin};
inline strut_ostream out{std::cout};
inline strut_ostream err{std::cerr};
inline strut_string strut_input(){std::string value;std::getline(std::cin,value);return value;}
template<class T> void strut_input(T& value){std::cin>>value;}
inline void strut_input(strut_string& value){std::string tmp;std::cin>>tmp;value=strut_string(tmp);}
inline json::Document strut_json_value(const json::Document& d){return d;}
inline json::Document strut_json_value(const strut_string& s){return json::Document(s.v);}
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

template<class C,class F> auto strut_count_by(const C& xs,F f) -> std::map<typename std::decay<decltype(f(*xs.begin()))>::type,std::size_t>{using K=typename std::decay<decltype(f(*xs.begin()))>::type;std::map<K,std::size_t> out;for(const auto& x:xs)++out[f(x)];return out;}
template<class C,class F> auto strut_index_by(const C& xs,F f) -> std::map<typename std::decay<decltype(f(*xs.begin()))>::type,typename C::value_type>{using K=typename std::decay<decltype(f(*xs.begin()))>::type;std::map<K,typename C::value_type> out;for(const auto& x:xs){auto k=f(x);if(out.find(k)!=out.end())throw std::runtime_error("index_by duplicate key");out.emplace(std::move(k),x);}return out;}
template<class T> struct strut_partition_result{std::vector<T> matched;std::vector<T> unmatched;};
template<class C,class F> auto strut_partition(const C& xs,F f) -> strut_partition_result<typename C::value_type>{strut_partition_result<typename C::value_type> out;for(const auto& x:xs)(f(x)?out.matched:out.unmatched).push_back(x);return out;}
inline json::Document strut_json_pick(const json::Document& d,const std::vector<strut_string>& keys){json::Document out=json::Document::make_object();if(d.type!=json::Type::Object)return out;for(const auto& k:keys)if(d.has(k.v))out[k.v]=d[k.v];return out;}
inline json::Document strut_json_omit(const json::Document& d,const std::vector<strut_string>& keys){json::Document out=json::Document::make_object();if(d.type!=json::Type::Object)return out;for(const auto& kv:d.object){bool omit=false;for(const auto& k:keys)if(k.v==kv.first){omit=true;break;}if(!omit)out[kv.first]=kv.second;}return out;}
inline json::Document strut_json_merge_deep(const json::Document& a,const json::Document& b){if(a.type!=json::Type::Object||b.type!=json::Type::Object)return b;json::Document out=a;for(const auto& kv:b.object){if(out.has(kv.first)&&out[kv.first].type==json::Type::Object&&kv.second.type==json::Type::Object)out[kv.first]=strut_json_merge_deep(out[kv.first],kv.second);else out[kv.first]=kv.second;}return out;}
inline bool strut_contains(const strut_string& s,const strut_string& x){return s.contains(x);}
template<class M,class K> bool strut_contains(const M& m,const K& k){return m.find(k)!=m.end();}
template<class M,class K,class V> void strut_map_insert(M& m,K&& k,V&& v){m[std::forward<K>(k)]=std::forward<V>(v);}
inline std::filesystem::path strut_fs_path(const strut_string& s){return std::filesystem::path(s.v);}
inline void strut_fs_fail(const char* op,const std::error_code& ec){if(ec)throw strut_checked_error("FilesystemError",std::string(op)+": "+ec.message());}
inline bool strut_fs_exists(const strut_string& p){std::error_code ec;bool v=std::filesystem::exists(strut_fs_path(p),ec);strut_fs_fail("exists",ec);return v;}
inline void strut_fs_make_dir(const strut_string& p){std::error_code ec;std::filesystem::create_directories(strut_fs_path(p),ec);strut_fs_fail("make_dir",ec);}
inline void strut_fs_remove(const strut_string& p){std::error_code ec;std::filesystem::remove_all(strut_fs_path(p),ec);strut_fs_fail("remove",ec);}
inline void strut_fs_copy(const strut_string& a,const strut_string& b){std::error_code ec;std::filesystem::copy(strut_fs_path(a),strut_fs_path(b),std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing,ec);strut_fs_fail("copy",ec);}
inline void strut_fs_move(const strut_string& a,const strut_string& b){std::error_code ec;std::filesystem::rename(strut_fs_path(a),strut_fs_path(b),ec);strut_fs_fail("move",ec);}
inline void strut_fs_touch(const strut_string& p){std::ofstream f(strut_fs_path(p),std::ios::app);if(!f)throw strut_checked_error("FilesystemError","touch: unable to open path");}
inline std::vector<strut_string> strut_fs_ls(const strut_string& p){std::error_code ec;std::vector<strut_string> out;for(std::filesystem::directory_iterator it(strut_fs_path(p),ec),end;!ec&&it!=end;it.increment(ec))out.emplace_back(it->path().filename().string());strut_fs_fail("ls",ec);std::sort(out.begin(),out.end());return out;}
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
struct strut_exec_result { std::int32_t exit_code=0; strut_string stdout; strut_string stderr; };
struct strut_exec_options { bool capture=true; bool inherit_stdio=false; strut_string cwd; std::vector<std::pair<std::string,std::string>> env; };
inline strut_exec_options strut_parse_exec_options(const json::Document& d){strut_exec_options o;if(d.type==json::Type::Null)return o;if(d.type!=json::Type::Object)throw strut_checked_error("ExecError","exec options must be a JSON object");if(d.has("capture")&&d["capture"].type==json::Type::Boolean)o.capture=d["capture"].boolean;if(d.has("inherit_stdio")&&d["inherit_stdio"].type==json::Type::Boolean)o.inherit_stdio=d["inherit_stdio"].boolean;if(d.has("cwd")&&d["cwd"].type==json::Type::String)o.cwd=d["cwd"].string;if(d.has("env")){const auto& e=d["env"];if(e.type!=json::Type::Object)throw strut_checked_error("ExecError","exec env option must be an object");for(const auto& kv:e.object){if(kv.second.type!=json::Type::String)throw strut_checked_error("ExecError","exec environment values must be strings");o.env.emplace_back(kv.first,kv.second.string);}}if(o.inherit_stdio)o.capture=false;return o;}
#ifndef _WIN32
inline strut_exec_result strut_exec_impl(const strut_string& program,const std::vector<strut_string>& args,const strut_exec_options& options){
    int out_pipe[2]={-1,-1},err_pipe[2]={-1,-1}; if(options.capture&&(pipe(out_pipe)!=0||pipe(err_pipe)!=0))throw strut_checked_error("ExecError",std::string("pipe failed: ")+std::strerror(errno));
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
    std::string sout,serr;auto read_handle=[](HANDLE h,std::string& dst){char b[4096];DWORD n=0;while(ReadFile(h,b,sizeof(b),&n,nullptr)&&n)dst.append(b,n);CloseHandle(h);};std::thread t1,t2;if(options.capture){t1=std::thread(read_handle,out_r,std::ref(sout));t2=std::thread(read_handle,err_r,std::ref(serr));}WaitForSingleObject(pi.hProcess,INFINITE);DWORD code=0;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);if(options.capture){t1.join();t2.join();}strut_exec_result result;result.exit_code=static_cast<std::int32_t>(code);result.stdout=strut_string(std::move(sout));result.stderr=strut_string(std::move(serr));return result;
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
    template<class F> auto submit(F&& f)->std::future<typename std::result_of<F()>::type>{using R=typename std::result_of<F()>::type;auto task=std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));auto fut=task->get_future();{std::lock_guard<std::mutex> g(m_);q_.emplace([task]{(*task)();});}cv_.notify_one();return fut;}
private:
    void worker(){for(;;){std::function<void()> job;{std::unique_lock<std::mutex> l(m_);cv_.wait(l,[this]{return stopping_||!q_.empty();});if(stopping_&&q_.empty())return;job=std::move(q_.front());q_.pop();}job();}}
    std::vector<std::thread> workers_;std::queue<std::function<void()>> q_;std::mutex m_;std::condition_variable cv_;bool stopping_=false;
};
inline strut_executor& strut_global_executor(){static strut_executor ex;return ex;}
template<class T> class strut_future { public: strut_future()=default; explicit strut_future(std::future<T>&& f):f_(std::move(f)){} T get(){return f_.get();} bool valid() const{return f_.valid();} private: std::future<T> f_; };
template<> class strut_future<void> { public: strut_future()=default; explicit strut_future(std::future<void>&& f):f_(std::move(f)){} void get(){f_.get();} bool valid() const{return f_.valid();} private: std::future<void> f_; };
template<class F> auto strut_async(F&& f)->strut_future<typename std::result_of<F()>::type>{using R=typename std::result_of<F()>::type;return strut_future<R>(strut_global_executor().submit(std::forward<F>(f)));}
template<class T> T strut_await(strut_future<T>& f){return f.get();}
inline void strut_await(strut_future<void>& f){f.get();}

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
    void send(T value) const{std::lock_guard<std::mutex> g(s_->m);if(s_->closed)throw std::runtime_error("send on closed channel");s_->q.push(std::move(value));s_->cv.notify_one();}
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
        if(h{CloseHandle(h);h=nullptr;}
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
        if(h{CloseHandle(h);h=nullptr;}
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
    strut_process(strut_process&&)=default;strut_process& operator=(strut_process&&)=default;
    void start(const strut_string& program,const std::vector<strut_string>& args){
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE child_in=nullptr,out_w=nullptr,err_w=nullptr;if(!CreatePipe(&in.h,&child_in,&sa,0)||!CreatePipe(&out.h,&out_w,&sa,0)||!CreatePipe(&err.h,&err_w,&sa,0))throw strut_checked_error("ExecError","CreatePipe failed");SetHandleInformation(in.h,HANDLE_FLAG_INHERIT,0);SetHandleInformation(out.h,HANDLE_FLAG_INHERIT,0);SetHandleInformation(err.h,HANDLE_FLAG_INHERIT,0);std::string cmd=strut_win_quote(program.v);for(const auto& a:args){cmd+=' ';cmd+=strut_win_quote(a.v);}std::vector<char> mc(cmd.begin(),cmd.end());mc.push_back('\0');STARTUPINFOA si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=child_in;si.hStdOutput=out_w;si.hStdError=err_w;BOOL ok=CreateProcessA(nullptr,mc.data(),nullptr,nullptr,TRUE,0,nullptr,nullptr,&si,&pi_);CloseHandle(child_in);CloseHandle(out_w);CloseHandle(err_w);if(!ok)throw strut_checked_error("ExecError","CreateProcess failed");running_=true;
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
    strut_process a(first,first_args);strut_process b(second,second_args);std::thread pump([&](){for(;;){auto chunk=a.out.read(4096);if(chunk.v.empty())break;b.in.write(chunk);}b.in.close();});auto aerr=std::thread([&](){a.err.read_all();});auto berr=std::thread([&](){b.err.read_all();});auto code_b=b.wait();auto stdout_b=b.out.read_all();pump.join();auto code_a=a.wait();(void)code_a;aerr.join();berr.join();strut_exec_result r;r.exit_code=code_b;r.stdout=std::move(stdout_b);return r;
}

#ifdef _WIN32
using strut_socket_handle=SOCKET; constexpr strut_socket_handle strut_invalid_socket=INVALID_SOCKET;
inline void strut_socket_close(strut_socket_handle h){if(h!=strut_invalid_socket)closesocket(h);}
struct strut_winsock_runtime{strut_winsock_runtime(){WSADATA d{};if(WSAStartup(MAKEWORD(2,2),&d)!=0)throw strut_checked_error("NetworkError","WSAStartup failed");}~strut_winsock_runtime(){WSACleanup();}};
inline void strut_socket_init(){static strut_winsock_runtime runtime;(void)runtime;}
#else
using strut_socket_handle=int; constexpr strut_socket_handle strut_invalid_socket=-1;
inline void strut_socket_close(strut_socket_handle h){if(h!=strut_invalid_socket)::close(h);}
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
inline strut_tcp_listener tcp_listen(const strut_string& host,std::int32_t port,std::int32_t backlog=128){strut_socket_init();if(port<1||port>65535)throw strut_checked_error("NetworkError","invalid TCP port");addrinfo hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_flags=AI_PASSIVE;addrinfo* list=nullptr;const std::string service=std::to_string(port);const char* node=host.v.empty()?nullptr:host.v.c_str();if(getaddrinfo(node,service.c_str(),&hints,&list)!=0)throw strut_checked_error("NetworkError","listen address resolution failed");strut_socket_handle h=strut_invalid_socket;for(addrinfo* p=list;p;p=p->ai_next){h=::socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(h==strut_invalid_socket)continue;int yes=1;setsockopt(h,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&yes),sizeof(yes));if(::bind(h,p->ai_addr,static_cast<int>(p->ai_addrlen))==0&&::listen(h,backlog)==0)break;strut_socket_close(h);h=strut_invalid_socket;}freeaddrinfo(list);if(h==strut_invalid_socket)throw strut_checked_error("NetworkError","TCP listen failed");return strut_tcp_listener(h);}

template<class... T> void strut_print(const T&... v){((std::cout<<v),...);std::cout<<'\n';}
)CPP";for(auto&s:p.statements)stmt(o,*s,0);r.cpp=o.str();return r;}
bool CppBackend::compile(const IRProgram& p,const std::filesystem::path& output,std::string& error,const NativeLinkOptions& link) const {auto g=generate(p);if(!g.ok()){error=g.error;return false;}auto tmp=output;tmp += ".strut.cpp";{std::ofstream f(tmp);if(!f){error="cannot write temporary C++ source";return false;}f<<g.cpp;}
#ifdef _WIN32
 const char* def="cl";
#else
 const char* def="c++";
#endif
 const char* env=std::getenv("CXX");std::string cxx=env&&*env?env:def;std::string cmd;
#ifdef _WIN32
 cmd=cxx+" /nologo /std:c++17 /EHsc "+(link.fully_static?"/MT ":"/MD ")+(link.release?"/O2 /Gy ":"")+"/I\"" STRUT_JSONIC_INCLUDE_DIR "\" \""+tmp.string()+"\" /Fe:\""+output.string()+"\"";
 bool link_section=false;
 for(const auto& d:link.search_paths){if(!link_section){cmd+=" /link";link_section=true;}cmd+=" /LIBPATH:\""+d.string()+"\"";}
 for(const auto& lib:link.libraries){std::filesystem::path lp(lib.value);cmd+=" "+(lp.has_extension()?"\""+lib.value+"\"":lib.value+".lib");}
 if(!link_section){cmd+=" /link";link_section=true;}cmd+=" ws2_32.lib";
 if(link.release){cmd+=" /OPT:REF /OPT:ICF";}
#else
 #ifdef __APPLE__
 if(link.fully_static){error="fully static final executables are not supported by the default macOS toolchain";std::error_code ec;std::filesystem::remove(tmp,ec);return false;}
 #endif
 cmd=cxx+" -std=c++17 "+(link.release?"-O2 -ffunction-sections -fdata-sections ":"-O2 ")+"-I\"" STRUT_JSONIC_INCLUDE_DIR "\" \""+tmp.string()+"\" -o \""+output.string()+"\"";
 if(link.fully_static)cmd+=" -static";
 for(const auto& d:link.search_paths)cmd+=" -L\""+d.string()+"\"";
 for(const auto& lib:link.libraries){std::filesystem::path lp(lib.value);if(lp.has_extension()||lib.value.find('/')!=std::string::npos){cmd+=" \""+lib.value+"\"";continue;}
 #ifdef __APPLE__
 if(lib.mode==NativeLinkMode::static_link){error="macOS static native libraries must be supplied as an explicit .a path";std::error_code ec;std::filesystem::remove(tmp,ec);return false;}cmd+=" -l"+lib.value;
 #else
 if(lib.mode==NativeLinkMode::static_link)cmd+=" -Wl,-Bstatic -l"+lib.value+" -Wl,-Bdynamic";else if(lib.mode==NativeLinkMode::dynamic_link)cmd+=" -Wl,-Bdynamic -l"+lib.value;else cmd+=" -l"+lib.value;
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
 int rc=std::system(cmd.c_str());std::error_code ec;std::filesystem::remove(tmp,ec);if(rc!=0){error="native C++ compiler/linker failed";return false;}return true;}
}
