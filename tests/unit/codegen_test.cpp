#include <cstdlib>
#include <filesystem>
#include <iostream>
#include "strut/codegen.h"
#include "strut/ir.h"
#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/sema.h"
#include "strut/source.h"
namespace { void req(bool x,const char*m){if(!x){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}} }
int main(){strut::SourceFile s("hello.p","function add(int a, int b) -> int { return a + b; } function main() -> void { x := add(2,3); print(\"hello \" , x); return; }"); strut::Lexer l(s);auto lx=l.lex();strut::Parser p(lx.tokens);auto a=p.parse();req(a.ok(),"parse");strut::SemanticAnalyzer sem;auto sr=sem.analyze(a.program);req(sr.ok(),"sema");strut::IRLowerer lower;auto ir=lower.lower(a.program);strut::CppBackend b;auto g=b.generate(ir.program);req(g.cpp.find("strut_print")!=std::string::npos,"print generated");auto out=std::filesystem::temp_directory_path()/"strut_codegen_test";std::string err;req(b.compile(ir.program,out,err),err.c_str());req(std::filesystem::exists(out),"native executable");std::filesystem::remove(out);return 0;}
