#include <cstdlib>
#include <iostream>
#include <string>
#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/source.h"
namespace {void require(bool c,const char*m){if(!c){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}} strut::ParseResult parse(std::string t){strut::SourceFile s("fixture.p",std::move(t));strut::Lexer l(s);auto x=l.lex();require(x.ok(),"fixture lex");strut::Parser p(x.tokens);return p.parse();}}
int main(){auto d=parse("x := 2; int y := 3; const z := 4; y = x;");require(d.ok(),"declarations");auto e=parse("x := 1 + 2 * 3; y := obj.member[2]++; ++x; x += 4;");require(e.ok(),"expressions");require(e.program.statements[0]->value->right->text=="*","precedence");
auto c=parse("if (x > 0) { y := 1; } else { y := 2; } while (x) { x--; if (x == 2) { break; } } for (i := 0; i < 10; i++) { continue; } for (item : items) { out := item; }");require(c.ok(),"control flow");require(c.program.statements.size()==4,"four controls");require(c.program.statements[3]->kind==strut::Stmt::Kind::range_for,"range for");
auto missing=parse("x := 2");require(!missing.ok(),"semicolon rejected");return 0;}
