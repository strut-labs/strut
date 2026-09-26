#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "strut/project.h"
#include "temp_directory.h"
namespace { void req(bool c,const char* m){if(!c){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}} }
int main(){
    strut::BuildConfig c; std::string e;
    req(strut::parse_build_config(R"({"entrypoint":"src/main.p","output":"bin/app","target":"native","mode":"release","linking":"dynamic","incremental":"modified"})",c,e),"parse config");
    req(c.entrypoint=="src/main.p"&&c.mode=="release","config values");
    req(!strut::parse_build_config(R"({"mode":"weird"})",c,e),"reject bad mode");
    TestTempDirectory temp("strut-project-test");auto root=temp.path();std::error_code ec;std::filesystem::create_directories(root/"src",ec);std::ofstream(root/"src/main.p")<<"function main() -> void { return; }\n";
    req(strut::init_project_build_state(root,e),"init project");req(std::filesystem::exists(root/".strut/config.json"),"config created");req(!strut::init_project_build_state(root,e),"no overwrite");
    return 0;
}
