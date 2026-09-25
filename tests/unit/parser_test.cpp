#include <cstdlib>
#include <iostream>
#include <string>
#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/source.h"
namespace {void require(bool c,const char*m){if(!c){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}} strut::ParseResult parse(std::string t){strut::SourceFile s("fixture.p",std::move(t));strut::Lexer l(s);auto x=l.lex();require(x.ok(),"fixture lex");strut::Parser p(x.tokens);return p.parse();}}
int main(){require(parse("x := 2; int y := 3;").ok(),"decl");require(parse("if (x) { y := 1; } for (v : values) { x := v; }").ok(),"control");require(parse("function add[T](T a, T b) -> T { return a + b; } function main() -> void { return; }").ok(),"functions");
auto l=parse("double_it := (x) => x * 2; max := (T x, T y) => { return x > y; }; fetch := async (url) => url; function<(double, double) -> double> mul := (a, b) => a * b; function[T]<(T, T) -> T> pick := (a, b) => a;");require(l.ok(),"lambdas and function types");require(l.program.statements.size()==5,"five lambdas");require(l.program.statements[1]->value->lambda->generic_parameters[0]=="T","inferred generic lambda");
require(!parse("function bad[t](t x) -> t { return x; }").ok(),"lowercase generic rejected");require(parse("function f() -> void { try { throw Error(\"x\"); } catch (Error err) { print(err); } catch { print(\"other\"); } }").ok(),"try catch");require(parse("operator +(Vec a, Vec b) -> Vec { return a; }").ok(),"operator declaration");require(parse("unsafe { x := raw->name; }").ok(),"raw pointer arrow member syntax");return 0;}
