#include <cstdlib>
#include <iostream>
#include "strut/package.h"
namespace { void req(bool ok,const char* msg){if(!ok){std::cerr<<"FAIL: "<<msg<<'\n';std::exit(1);}} }
int main(){
    req(strut::valid_version_requirement("1.2.3"),"exact requirement");
    req(strut::valid_version_requirement("^1.2.3"),"caret requirement");
    req(strut::valid_version_requirement("~1.2.3"),"tilde requirement");
    req(!strut::valid_version_requirement("latest"),"reject floating text requirement");
    strut::PackageManifest m;std::string error;
    req(strut::parse_package_manifest(R"({"name":"demo","version":"0.1.0","entry":"src/main.p","dependencies":{"http":"^0.2.0","sqlite":"~0.3.1"}})",m,error),error.c_str());
    req(m.name=="demo"&&m.dependencies.size()==2,"manifest fields");
    req(!strut::package_cache_root().empty(),"cache root");
    return 0;
}
