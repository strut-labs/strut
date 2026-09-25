#include <cstdlib>
#include <iostream>
#include <string>
#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/source.h"
namespace {void require(bool c,const char*m){if(!c){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}} strut::ParseResult parse(std::string t){strut::SourceFile s("fixture.p",std::move(t));strut::Lexer l(s);auto x=l.lex();require(x.ok(),"fixture lex");strut::Parser p(x.tokens);return p.parse();}}
int main(){
require(parse("x := 2; int y := 3; y = x;").ok(),"declarations");auto e=parse("x := 1 + 2 * 3;");require(e.ok()&&e.program.statements[0]->value->right->text=="*","precedence");
require(parse("if (x) { y := 1; } else { y := 2; } while (x) { break; } for (i := 0; i < 3; i++) { continue; } for (v : values) { x := v; }").ok(),"control");
auto f=parse("function add[T](T a, T b) -> T { return a + b; } function User::name() -> string; function main() -> void { return; }");require(f.ok(),"functions");require(f.program.statements.size()==3,"three functions");require(f.program.statements[0]->generic_parameters[0]=="T","generic T");require(f.program.statements[1]->owner=="User"&&!f.program.statements[1]->has_body,"method declaration");
auto bad=parse("function bad[t](t x) -> t { return x; }");require(!bad.ok(),"lowercase generic rejected");return 0;}
