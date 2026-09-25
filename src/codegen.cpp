#include "strut/codegen.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
namespace strut { namespace {
std::string cpp_type(std::string t){
    if(t.size()>2 && t.ends_with("[]")) return "std::vector<"+cpp_type(t.substr(0,t.size()-2))+">";
    if(t=="void") return "void";
    if(t=="bool") return "bool";
    if(t=="string") return "std::string";
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
    if(t=="opaque") return "auto";
    return t.empty()?"auto":t;
}
std::string expr(const IRExpr& e){
    switch(e.kind){
        case IRExpr::Kind::identifier: case IRExpr::Kind::integer_literal: case IRExpr::Kind::floating_literal: case IRExpr::Kind::string_literal: case IRExpr::Kind::boolean_literal: return e.text;
        case IRExpr::Kind::null_literal:return "nullptr";
        case IRExpr::Kind::array_literal:{std::string out="{";for(size_t i=0;i<e.arguments.size();++i){if(i)out+=",";out+=expr(*e.arguments[i]);}return out+"}";}
        case IRExpr::Kind::grouping:return "("+expr(*e.left)+")";
        case IRExpr::Kind::unary:return e.text+expr(*e.right);
        case IRExpr::Kind::postfix:return expr(*e.left)+e.text;
        case IRExpr::Kind::binary:return "("+expr(*e.left)+" "+e.text+" "+expr(*e.right)+")";
        case IRExpr::Kind::member:{std::string m=e.text;if(m=="push")m="push_back";else if(m=="pop")m="pop_back";else if(m=="length")m="size";return expr(*e.left)+"."+m;}
        case IRExpr::Kind::index:return expr(*e.left)+".at("+expr(*e.right)+")";
        case IRExpr::Kind::call:{
            std::string name=expr(*e.left); if(name=="print"){std::string out="strut_print(";for(size_t i=0;i<e.arguments.size();++i){if(i)out+=",";out+=expr(*e.arguments[i]);}return out+")";}
            std::string out=name+"(";for(size_t i=0;i<e.arguments.size();++i){if(i)out+=",";out+=expr(*e.arguments[i]);}return out+")";
        }
        case IRExpr::Kind::lambda:return "/* lambda pending */";
    } return {};
}
void stmt(std::ostringstream& o,const IRStmt& s,int n){std::string pad(n,' ');
    switch(s.kind){
        case IRStmt::Kind::declaration:o<<pad<<(s.is_const?"const ":"")<<cpp_type(s.type_name)<<" "<<s.name<<" = "<<expr(*s.value)<<";\n";break;
        case IRStmt::Kind::assignment:o<<pad<<s.name<<" "<<s.op<<" "<<expr(*s.value)<<";\n";break;
        case IRStmt::Kind::expression:o<<pad<<expr(*s.value)<<";\n";break;
        case IRStmt::Kind::return_stmt:o<<pad<<"return"<<(s.value?" "+expr(*s.value):"")<<";\n";break;
        case IRStmt::Kind::break_stmt:o<<pad<<"break;\n";break; case IRStmt::Kind::continue_stmt:o<<pad<<"continue;\n";break;
        case IRStmt::Kind::block:o<<pad<<"{\n";for(auto&c:s.body)stmt(o,*c,n+4);o<<pad<<"}\n";break;
        case IRStmt::Kind::if_stmt:o<<pad<<"if ("<<expr(*s.condition)<<") {\n";for(auto&c:s.body)stmt(o,*c,n+4);o<<pad<<"}";if(!s.else_body.empty()){o<<" else {\n";for(auto&c:s.else_body)stmt(o,*c,n+4);o<<pad<<"}";}o<<"\n";break;
        case IRStmt::Kind::while_stmt:o<<pad<<"while ("<<expr(*s.condition)<<") {\n";for(auto&c:s.body)stmt(o,*c,n+4);o<<pad<<"}\n";break;
        case IRStmt::Kind::range_for:o<<pad<<"for (auto& "<<s.name<<" : "<<expr(*s.value)<<") {\n";for(auto&c:s.body)stmt(o,*c,n+4);o<<pad<<"}\n";break;
        case IRStmt::Kind::function_decl:{if(!s.owner.empty())break;const bool main_void=(s.name=="main"&&s.return_type=="void");o<<(main_void?"int":cpp_type(s.return_type))<<" "<<s.name<<"(";for(size_t i=0;i<s.parameters.size();++i){if(i)o<<",";o<<cpp_type(s.parameters[i].type.name)<<" "<<s.parameters[i].name;}o<<")";if(!s.has_body){o<<";\n";break;}o<<" {\n";for(auto&c:s.body){if(main_void&&c->kind==IRStmt::Kind::return_stmt&&!c->value){o<<std::string(n+4,' ')<<"return 0;\n";}else stmt(o,*c,n+4);}o<<"}\n";break;}
        default:break;
    }}
}
CodegenResult CppBackend::generate(const IRProgram& p) const {CodegenResult r;std::ostringstream o;o<<"#include <cstdint>\n#include <iostream>\n#include <string>\n#include <vector>\n#include <stdexcept>\n";
o<<"template<class... T> void strut_print(const T&... v){((std::cout<<v),...);std::cout<<'\\n';}\n";for(auto&s:p.statements)stmt(o,*s,0);r.cpp=o.str();return r;}
bool CppBackend::compile(const IRProgram& p,const std::filesystem::path& output,std::string& error) const {auto g=generate(p);if(!g.ok()){error=g.error;return false;}auto tmp=output;tmp += ".strut.cpp";{std::ofstream f(tmp);if(!f){error="cannot write temporary C++ source";return false;}f<<g.cpp;}
#ifdef _WIN32
 const char* def="cl";
#else
 const char* def="c++";
#endif
 const char* env=std::getenv("CXX");std::string cxx=env&&*env?env:def;std::string cmd;
#ifdef _WIN32
 cmd=cxx+" /nologo /std:c++20 /EHsc \""+tmp.string()+"\" /Fe:\""+output.string()+"\"";
#else
 cmd=cxx+" -std=c++20 -O2 \""+tmp.string()+"\" -o \""+output.string()+"\"";
#endif
 int rc=std::system(cmd.c_str());std::error_code ec;std::filesystem::remove(tmp,ec);if(rc!=0){error="native C++ compiler failed";return false;}return true;}
}
