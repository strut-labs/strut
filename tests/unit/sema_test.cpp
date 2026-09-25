#include <cstdlib>
#include <iostream>
#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/sema.h"
#include "strut/source.h"
namespace{void req(bool c,const char*m){if(!c){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}} strut::SemanticResult sema(std::string t){strut::SourceFile s("x.p",std::move(t));strut::Lexer l(s);auto lx=l.lex();req(lx.ok(),"lex");strut::Parser p(lx.tokens);auto pr=p.parse();req(pr.ok(),"parse");strut::SemanticAnalyzer a;return a.analyze(pr.program);}}
int main(){req(sema("x := 1; { x := 2; }").ok(),"nested shadowing allowed");req(!sema("x := 1; x := 2;").ok(),"same-scope duplicate rejected");req(!sema("function f(int x) -> void { x := 2; }").ok(),"parameter duplicate rejected");req(sema("x := 1; function x() -> void;").ok(),"value/function namespaces separate");req(sema("type user_id := uint_64; user_id id := 7;").ok(),"user alias");req(!sema("type A := B; type B := A;").ok(),"alias cycle rejected");req(sema("x := 1; int_64 y := x; double z := x;").ok(),"inference and widening");req(!sema("const x := 1; x = 2;").ok(),"const enforced");req(!sema("x := 1; x = \"bad\";").ok(),"assignment type checked");return 0;}
