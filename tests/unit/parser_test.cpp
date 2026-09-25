#include <cstdlib>
#include <iostream>
#include <string>
#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/source.h"
namespace {void require(bool c,const char*m){if(!c){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}} strut::ParseResult parse(std::string t){strut::SourceFile s("fixture.p",std::move(t));strut::Lexer l(s);auto x=l.lex();require(x.ok(),"fixture lex");strut::Parser p(x.tokens);return p.parse();}}
int main(){
 auto d=parse("x := 2; int y := 3; const z := 4; const int q := 5; y = x;");require(d.ok(),"declarations");require(d.program.statements.size()==5,"five statements");
 auto e=parse("x := 1 + 2 * 3; y = x << 2; ok := x >= y || false && true; z := obj.member[2]++; ++x; x += 4;");require(e.ok(),"expressions parse");require(e.program.statements.size()==6,"six expression statements");auto& root=*e.program.statements[0]->value;require(root.kind==strut::Expr::Kind::binary&&root.text=="+","add root");require(root.right&&root.right->text=="*","multiplication precedence");
 auto missing=parse("x := 2");require(!missing.ok(),"semicolon rejected");
 auto cppish=parse("int *x := 2;");require(!cppish.ok(),"C declarator rejected");return 0;}
