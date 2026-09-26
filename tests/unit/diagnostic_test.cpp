#include <cstdlib>
#include <iostream>
#include <string>
#include "strut/diagnostic.h"
namespace { void require(bool c,const char*m){if(!c){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}} }
int main(){strut::Diagnostic d{{{0,3,7},{1,3,8}},"expected ';' after statement"};require(strut::format_diagnostic("file.p",d)=="file.p:3:7: error: expected ';' after statement","stable diagnostic format");auto rich=strut::format_diagnostic_with_source("file.p","function main() -> void {\n  int x := 1;\n  return;\n}\n",d,false,true);require(rich.find("\x1b[")!=std::string::npos,"rich diagnostic uses ANSI syntax highlighting");require(rich.find("3 |")!=std::string::npos,"rich diagnostic shows source line");require(rich.find("^")!=std::string::npos,"rich diagnostic shows caret");return 0;}
