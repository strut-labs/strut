#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <thread>
#include "strut/package.h"
#include "temp_directory.h"
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
    TestTempDirectory temp("strut-package-test"); const auto tmp=temp.path();
    const auto isolated=tmp/"home";
#ifdef _WIN32
    _putenv_s("STRUT_HOME",isolated.string().c_str());
#else
    setenv("STRUT_HOME",isolated.string().c_str(),1);
#endif
    std::filesystem::create_directories(tmp/"empty");std::string empty_checksum;req(strut::package_content_checksum(tmp/"empty",empty_checksum,error),error.c_str());req(empty_checksum=="sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","standard SHA-256");
    std::filesystem::create_directories(tmp/"pkg");
    { std::ofstream f(tmp/"pkg"/"strut.json"); f << R"({"name":"local","version":"1.2.3","entry":"main.p"})"; } { std::ofstream f(tmp/"pkg"/"main.p"); f << "function answer() -> int { return 42; }\n"; }
    std::filesystem::path cached; strut::PackageManifest local; req(strut::cache_local_package(tmp/"pkg",cached,local,error),error.c_str()); req(local.name=="local","local package metadata");req(cached.parent_path().filename()=="1.2.3"&&cached.filename().string().size()==64,"content-addressed cache layout");
    std::filesystem::path cache_hit;strut::PackageManifest hit_manifest;req(strut::cache_local_package(tmp/"pkg",cache_hit,hit_manifest,error)&&cache_hit==cached,"verified cache hit");
    {std::ofstream f(cached/"main.p",std::ios::app);f<<"corrupt";}std::string corruption;req(!strut::resolve_cached_package("local","1.2.3",&corruption)&&corruption.find("checksum")!=std::string::npos,"corrupt cache detected");req(strut::cache_local_package(tmp/"pkg",cached,local,error),error.c_str());req(strut::resolve_cached_package("local","1.2.3").has_value(),"local source repairs corrupt cache");
    std::filesystem::remove_all(cached);std::filesystem::path concurrent_a,concurrent_b;strut::PackageManifest concurrent_ma,concurrent_mb;std::string concurrent_ea,concurrent_eb;bool concurrent_ok_a=false,concurrent_ok_b=false;std::thread a([&]{concurrent_ok_a=strut::cache_local_package(tmp/"pkg",concurrent_a,concurrent_ma,concurrent_ea);}),b([&]{concurrent_ok_b=strut::cache_local_package(tmp/"pkg",concurrent_b,concurrent_mb,concurrent_eb);});a.join();b.join();req(concurrent_ok_a&&concurrent_ok_b&&concurrent_a==concurrent_b,"concurrent atomic cache promotion");std::size_t staging_entries=0;std::error_code stage_ec;for(const auto& ignored:std::filesystem::directory_iterator(isolated/"cache/packages/.staging",stage_ec)){(void)ignored;++staging_entries;}req(!stage_ec&&staging_entries==0,"staging cleaned after acquisition");cached=concurrent_a;
    strut::PackageManifest app;app.name="app";app.version="0.1.0";app.dependencies["local"]="^1.0.0";req(strut::write_lockfile(tmp,app,error),error.c_str());
    std::ifstream first_file(tmp/"strut.lock.json");std::string first((std::istreambuf_iterator<char>(first_file)),{});req(first.find(tmp.generic_string())==std::string::npos,"lock excludes machine paths");req(first.find("\"schema_version\": 2")!=std::string::npos,"lock schema version");
    strut::PackageLock parsed;req(strut::parse_package_lock(first,parsed,error),error.c_str());req(parsed.packages.size()==1&&parsed.packages[0].name=="local"&&parsed.packages[0].direct,"lock round trip");req(strut::write_package_lock_file(tmp/"second.lock",parsed,error),error.c_str());std::ifstream second_file(tmp/"second.lock");std::string second((std::istreambuf_iterator<char>(second_file)),{});req(first==second,"deterministic lock bytes");
    auto duplicate=parsed;duplicate.packages.push_back(duplicate.packages[0]);req(!strut::validate_package_lock(duplicate,nullptr,error)&&error.find("duplicate")!=std::string::npos,"duplicate lock package rejected");
    auto cycle=parsed;cycle.packages[0].dependencies["local"]=cycle.packages[0].version;req(!strut::validate_package_lock(cycle,nullptr,error)&&error.find("cyclic")!=std::string::npos,"lock cycle rejected");
    strut::PackageManifest stale=app;stale.dependencies["missing"]="1.0.0";req(!strut::validate_package_lock(parsed,&stale,error)&&error.find("stale")!=std::string::npos,"stale lock rejected");
    strut::PackageLock legacy;req(!strut::parse_package_lock(R"({"version":1,"dependencies":{}})",legacy,error)&&error.find("legacy")!=std::string::npos,"legacy lock diagnostic");
    const auto remote_repo=tmp/"remote-repo";std::filesystem::create_directories(remote_repo);{std::ofstream f(remote_repo/"strut.json");f<<R"({"name":"remote","version":"2.0.0","entry":"main.p"})";}{std::ofstream f(remote_repo/"main.p");f<<"function remote_value() -> int { return 7; }\n";}
    const auto quote=[](const std::filesystem::path&p){return std::string("\"")+p.string()+"\"";};req(std::system(("git init --quiet "+quote(remote_repo)).c_str())==0,"local Git fixture init");req(std::system(("git -C "+quote(remote_repo)+" config user.email strut@example.invalid").c_str())==0,"local Git email");req(std::system(("git -C "+quote(remote_repo)+" config user.name Strut").c_str())==0,"local Git name");req(std::system(("git -C "+quote(remote_repo)+" add strut.json main.p").c_str())==0,"local Git add");req(std::system(("git -C "+quote(remote_repo)+" commit --quiet -m fixture").c_str())==0,"local Git commit");const auto revision_path=tmp/"revision.txt";req(std::system(("git -C "+quote(remote_repo)+" rev-parse HEAD > "+quote(revision_path)).c_str())==0,"local Git revision");std::ifstream revision_file(revision_path);std::string revision;revision_file>>revision;
    strut::PackageManifest remote_project;remote_project.name="remote-app";remote_project.version="0.1.0";remote_project.dependencies["remote"]="2.0.0";remote_project.dependency_sources["remote"]={"git",remote_repo.string(),revision};req(strut::write_lockfile(tmp,remote_project,error),error.c_str());strut::PackageLock remote_lock;req(strut::load_package_lock_file(tmp/"strut.lock.json",remote_lock,error),error.c_str());req(remote_lock.packages.size()==1&&remote_lock.packages[0].source_kind=="git"&&remote_lock.packages[0].revision==revision,"immutable Git source locked");
    std::filesystem::create_directories(isolated/"cache/packages/broken/1.0.0");
    {std::ofstream f(isolated/"cache/packages/broken/1.0.0/strut.json");f<<R"({"name":"other","version":"1.0.0"})";}
    strut::PackageManifest project;project.name="project";project.version="0.1.0";project.dependencies["broken"]="1.0.0";
    std::string cache_error;req(!strut::write_lockfile(tmp,project,cache_error),"broken cache metadata rejected");req(cache_error.find("stale or invalid metadata")!=std::string::npos||cache_error.find("corrupted package cache")!=std::string::npos,"broken cache diagnostic is actionable");
    return 0;
}
