#include <cstdlib>
#include <iostream>
#include "strut/ir.h"
#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/source.h"
namespace { void req(bool x,const char*m){if(!x){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}} }
int main(){strut::SourceFile s("x.p","function main() -> void { x := 2; if (x > 1) { x = x + 1; } return; }");strut::Lexer l(s);auto lx=l.lex();strut::Parser p(lx.tokens);auto a=p.parse();req(a.ok(),"parse");strut::IRLowerer lower;auto ir=lower.lower(a.program);req(ir.ok(),"lower");req(ir.program.statements.size()==1,"one fn");auto& fn=*ir.program.statements[0];req(fn.body.size()==3,"body lowered");req(fn.body[0]->type_name=="int_32","typed inferred declaration");req(fn.body[0]->span.begin.line==1,"source span retained");return 0;}
