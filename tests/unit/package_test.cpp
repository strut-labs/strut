#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include "strut/package.h"
#include "temp_directory.h"
namespace{
void req(bool ok,const char*m){if(!ok){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}}
std::string read(const std::filesystem::path&p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
std::string quote(const std::filesystem::path&p){return "\""+p.string()+"\"";}
}
int main(){
 req(strut::valid_package_name("http")&&!strut::valid_package_name("HTTP"),"package names");
 req(strut::valid_package_relative_path("src/main.p")&&!strut::valid_package_relative_path("../bad"),"package paths");
 req(strut::valid_version_requirement("1.2.3")&&strut::valid_version_requirement("^1.2.3")&&!strut::valid_version_requirement("latest"),"versions");
 std::string error;strut::PackageManifest manifest;
 req(strut::parse_package_manifest(R"({"name":"demo","version":"0.1.0","dependencies":{"http":"^0.2.0"}})",manifest,error),error.c_str());
 TestTempDirectory temp("strut-package-test");const auto root=temp.path(),home=root/"home";
#ifdef _WIN32
 _putenv_s("STRUT_HOME",home.string().c_str());
#else
 setenv("STRUT_HOME",home.string().c_str(),1);
#endif
 std::filesystem::create_directories(root/"empty");std::string checksum;req(strut::package_content_checksum(root/"empty",checksum,error),error.c_str());req(checksum=="sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA-256");
 const auto local_root=root/"local";std::filesystem::create_directories(local_root);std::ofstream(local_root/"strut.json")<<R"({"name":"local","version":"1.2.3","entry":"main.p"})";std::ofstream(local_root/"main.p")<<"function answer() -> int { return 42; }\n";
 std::filesystem::path cached;strut::PackageManifest local;req(strut::cache_local_package(local_root,cached,local,error),error.c_str());req(cached.parent_path().filename()=="1.2.3"&&cached.filename().string().size()==64,"content address");
 std::filesystem::path hit;strut::PackageManifest hit_manifest;req(strut::cache_local_package(local_root,hit,hit_manifest,error)&&hit==cached,"cache hit");
 std::ofstream(cached/"main.p",std::ios::app)<<"corrupt";std::string corruption;req(!strut::resolve_cached_package("local","1.2.3",&corruption)&&corruption.find("checksum")!=std::string::npos,"corruption detection");req(strut::cache_local_package(local_root,cached,local,error),error.c_str());
 std::filesystem::remove_all(cached);std::filesystem::path ap,bp;strut::PackageManifest am,bm;std::string ae,be;bool ao=false,bo=false;std::thread a([&]{ao=strut::cache_local_package(local_root,ap,am,ae);}),b([&]{bo=strut::cache_local_package(local_root,bp,bm,be);});a.join();b.join();req(ao&&bo&&ap==bp,"concurrent promotion");
 strut::PackageManifest app;app.name="app";app.version="0.1.0";app.dependencies["local"]="^1.0.0";req(strut::write_lockfile(root,app,error),error.c_str());const auto first=read(root/"strut.lock.json");req(first.find(root.generic_string())==std::string::npos,"path-free lock");strut::PackageLock parsed;req(strut::parse_package_lock(first,parsed,error),error.c_str());req(strut::write_package_lock_file(root/"second.lock",parsed,error)&&read(root/"second.lock")==first,"stable round trip");
 auto duplicate=parsed;duplicate.packages.push_back(duplicate.packages[0]);req(!strut::validate_package_lock(duplicate,nullptr,error)&&error.find("duplicate")!=std::string::npos,"duplicate rejection");auto cycle=parsed;cycle.packages[0].dependencies["local"]="1.2.3";req(!strut::validate_package_lock(cycle,nullptr,error)&&error.find("cyclic")!=std::string::npos,"cycle rejection");strut::PackageLock legacy;req(!strut::parse_package_lock(R"({"version":1,"dependencies":{}})",legacy,error),"legacy rejection");
 const auto remote=root/"remote";std::filesystem::create_directories(remote);std::ofstream(remote/"strut.json")<<R"({"name":"remote","version":"2.0.0","entry":"main.p"})";std::ofstream(remote/"main.p")<<"function value() -> int { return 7; }\n";
 req(std::system(("git init --quiet "+quote(remote)).c_str())==0,"git init");req(std::system(("git -C "+quote(remote)+" config user.email strut@example.invalid").c_str())==0,"git email");req(std::system(("git -C "+quote(remote)+" config user.name Strut").c_str())==0,"git name");req(std::system(("git -C "+quote(remote)+" add strut.json main.p").c_str())==0,"git add");req(std::system(("git -C "+quote(remote)+" commit --quiet -m fixture").c_str())==0,"git commit");const auto rev_file=root/"revision";req(std::system(("git -C "+quote(remote)+" rev-parse HEAD > "+quote(rev_file)).c_str())==0,"git revision");std::ifstream rf(rev_file);std::string revision;rf>>revision;
 strut::PackageManifest remote_app;remote_app.name="remote-app";remote_app.version="0.1.0";remote_app.dependencies["remote"]="2.0.0";remote_app.dependency_sources["remote"]={"git",remote.string(),revision};req(strut::write_package_manifest_file(root/"strut.json",remote_app,error),error.c_str());req(strut::write_lockfile(root,remote_app,error),error.c_str());strut::PackageLock remote_lock;req(strut::load_package_lock_file(root/"strut.lock.json",remote_lock,error),error.c_str());req(remote_lock.packages[0].source_kind=="git"&&remote_lock.packages[0].revision==revision,"immutable Git lock");
 const auto locked=read(root/"strut.lock.json");const auto remote_cache=home/"cache/packages/remote/2.0.0"/remote_lock.packages[0].checksum.substr(7);std::filesystem::remove_all(remote_cache);strut::PackageLock installed;req(!strut::install_packages(root,true,false,installed,error)&&error.find("offline")!=std::string::npos,"offline missing cache");req(strut::install_packages(root,false,false,installed,error),error.c_str());req(read(root/"strut.lock.json")==locked,"install preserves lock");req(strut::install_packages(root,true,false,installed,error),error.c_str());
 std::filesystem::create_directories(home/"cache/packages/broken/1.0.0");std::ofstream(home/"cache/packages/broken/1.0.0/strut.json")<<R"({"name":"other","version":"1.0.0"})";strut::PackageManifest broken;broken.name="project";broken.version="0.1.0";broken.dependencies["broken"]="1.0.0";req(!strut::write_lockfile(root,broken,error),"broken cache rejected");
 return 0;
}
