#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include "strut/package.h"
namespace { void req(bool ok,const char* msg){if(!ok){std::cerr<<"FAIL: "<<msg<<'\n';std::exit(1);}} }
int main(){
    req(strut::valid_package_name("http"),"package name");
    req(!strut::valid_package_name("HTTP"),"reject uppercase package name");
    req(strut::valid_package_relative_path("src/main.p"),"relative package path");
    req(!strut::valid_package_relative_path("../escape.p"),"reject escaping package path");
    req(strut::valid_version_requirement("1.2.3"),"exact requirement");
    req(strut::valid_version_requirement("^1.2.3"),"caret requirement");
    req(strut::valid_version_requirement("~1.2.3"),"tilde requirement");
    req(!strut::valid_version_requirement("latest"),"reject floating text requirement");
    strut::PackageManifest m;std::string error;
    req(strut::parse_package_manifest(R"({"name":"demo","version":"0.1.0","entry":"src/main.p","description":"demo","license":"MIT","repository":"https://github.com/strut-packages/demo","sources":["src"],"include_dirs":["include"],"dependencies":{"http":"^0.2.0","sqlite":"~0.3.1"}})",m,error),error.c_str());
    req(m.name=="demo"&&m.dependencies.size()==2&&m.sources.size()==1&&m.include_dirs.size()==1,"manifest fields");
    strut::PackageManifest bad; std::string bad_error;
    req(!strut::parse_package_manifest(R"({"name":"demo","version":"0.1.0","sources":["../bad"]})",bad,bad_error),"reject escaping source path");
    req(!strut::package_cache_root().empty(),"cache root");
    const auto tmp=std::filesystem::temp_directory_path()/"strut-package-test"; std::filesystem::remove_all(tmp); std::filesystem::create_directories(tmp/"pkg");
    { std::ofstream f(tmp/"pkg"/"strut.json"); f << R"({"name":"local","version":"1.2.3","entry":"main.p"})"; } { std::ofstream f(tmp/"pkg"/"main.p"); f << "function answer() -> int { return 42; }\n"; }
    std::filesystem::path cached; strut::PackageManifest local; req(strut::cache_local_package(tmp/"pkg",cached,local,error),error.c_str()); req(local.name=="local","local package metadata");
    return 0;
}
