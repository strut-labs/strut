#include <cstdlib>
#include <iostream>
#include <string>
#include "strut/formatter.h"
namespace { void req(bool c,const char* m){if(!c){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}} }
int main(){
    const std::string input="function main() -> void {\n  ptr < int > p; // keep me\nif (true) {\nprint(\"{not a brace}\");\n}\n}\n";
    const auto once=strut::format_source_text(input); const auto twice=strut::format_source_text(once);
    req(once==twice,"formatter idempotent");
    req(once.find("    ptr<int> p; // keep me")!=std::string::npos,"canonical ptr spelling/comment");
    req(once.find("        print")!=std::string::npos,"nested indentation");
    const auto generics=strut::format_source_text("tuple < double, int > t;\nmap < string, int > m;\n");
    req(generics.find("tuple<double,int> t;")!=std::string::npos,"tuple generic spacing");
    req(generics.find("map<string,int> m;")!=std::string::npos,"map generic spacing");
    return 0;
}
