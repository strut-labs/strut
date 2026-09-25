#include <cstdlib>
#include <iostream>
#include <string>
#include "strut/diagnostic.h"
namespace { void require(bool c,const char*m){if(!c){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}} }
int main(){strut::Diagnostic d{{{0,3,7},{1,3,8}},"expected ';' after statement"};require(strut::format_diagnostic("file.p",d)=="file.p:3:7: error: expected ';' after statement","stable diagnostic format");return 0;}
