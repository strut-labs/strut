#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
#include <fstream>

#include "strut/cli.h"
#include "strut/package.h"
#include "temp_directory.h"

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
}

int main() {
    {
        char arg0[] = "strut";
        char arg1[] = "--help";
        char* argv[] = {arg0, arg1};
        std::ostringstream out;
        std::ostringstream err;
        require(strut::run_cli(2, argv, out, err) == 0, "--help exit status");
        require(out.str().find("Usage: strut") != std::string::npos, "--help output");
        require(out.str().find("function main(string cmd, string[] args) -> int") != std::string::npos, "--help entry point discovery");
        require(out.str().find("for (item : items)") != std::string::npos, "--help range loop discovery");
        require(out.str().find("https://strut-labs.github.io/docs.html") != std::string::npos, "--help docs discovery");
        require(err.str().empty(), "--help stderr");
    }

    {
        char arg0[] = "strut";
        char arg1[] = "--version";
        char arg2[] = "--json";
        char* argv[] = {arg0, arg1, arg2};
        std::ostringstream out;
        std::ostringstream err;
        require(strut::run_cli(3, argv, out, err) == 0, "--version --json exit status");
        require(out.str().find("\"jsonic\"") != std::string::npos, "Jsonic++ metadata present");
        require(err.str().empty(), "--version --json stderr");
    }


    {
        const auto old = std::filesystem::current_path();
        TestTempDirectory temp("strut-cli-make-test"); auto root = temp.path();
        std::error_code ec; std::filesystem::create_directories(root / ".strut", ec);
        std::ofstream(root / "main.p") << "function main() -> void { print(\"make-ok\"); return; }\n";
        std::ofstream(root / ".strut/config.json") << R"({"entrypoint":"main.p","output":"app","target":"native","mode":"debug","linking":"dynamic","incremental":"modified"})";
        std::filesystem::current_path(root);
        char arg0[] = "strut"; char arg1[] = "make"; char* argv[] = {arg0,arg1};
        std::ostringstream out; std::ostringstream err;
        require(strut::run_cli(2, argv, out, err) == 0, "make project");
#ifdef _WIN32
        require(std::filesystem::exists(root / "app.exe"), "make output");
#else
        require(std::filesystem::exists(root / "app"), "make output");
#endif
        std::filesystem::current_path(old);
    }


    {
        const auto old = std::filesystem::current_path();
        TestTempDirectory temp("strut-cli-test-command"); auto root = temp.path();
        std::error_code ec; std::filesystem::create_directories(root / ".strut", ec); std::filesystem::create_directories(root / "tests", ec);
        std::ofstream(root / "main.p") << "function main() -> void { return; }\n";
        std::ofstream(root / "tests/smoke_test.p") << "function main() -> void { print(\"test-ok\"); return; }\n";
        std::ofstream(root / ".strut/config.json") << R"({"entrypoint":"main.p","output":"app","target":"native","mode":"debug","linking":"dynamic","incremental":"modified"})";
        std::filesystem::current_path(root);
        char arg0[] = "strut"; char arg1[] = "test"; char arg2[] = "smoke"; char* argv[] = {arg0,arg1,arg2};
        std::ostringstream out; std::ostringstream err;
        require(strut::run_cli(3, argv, out, err) == 0, "test command");
        require(out.str().find("1/1 tests passed") != std::string::npos, "test summary");
        std::filesystem::current_path(old);
    }


    {
        char arg0[]="strut"; char arg1[]="make"; char arg2[]="--help"; char* argv[]={arg0,arg1,arg2};
        std::ostringstream out; std::ostringstream err;
        require(strut::run_cli(3,argv,out,err)==0,"command help status");
        require(out.str().find("Usage: strut make")!=std::string::npos,"command help text");
    }

    {
        char arg0[]="strut";char arg1[]="api";char arg2[]="--json";char* argv[]={arg0,arg1,arg2};
        std::ostringstream out;std::ostringstream err;require(strut::run_cli(3,argv,out,err)==0,"API JSON status");
        require(out.str().find("\"http_get\"")!=std::string::npos,"API contains builtins");
        require(out.str().find("\"HttpError\"")!=std::string::npos,"API contains checked errors");
        require(out.str().find("\"libcurl\"")!=std::string::npos,"API contains native dependencies");
    }
    {
        char arg0[]="strut";char arg1[]="api";char arg2[]="--json";char arg3[]="sqlite";char* argv[]={arg0,arg1,arg2,arg3};
        std::ostringstream out;std::ostringstream err;require(strut::run_cli(4,argv,out,err)==0,"filtered API JSON status");
        require(out.str().find("\"sqlite_open\"")!=std::string::npos,"filtered API includes match");
        require(out.str().find("\"http_get\"")==std::string::npos,"filtered API excludes non-match");
    }
    {
        char arg0[]="strut";char arg1[]="api";char arg2[]="checked-errors";char* argv[]={arg0,arg1,arg2};
        std::ostringstream out;std::ostringstream err;require(strut::run_cli(3,argv,out,err)==0,"human API status");
        require(out.str().find("throws HttpError")!=std::string::npos,"human API exposes checked errors");
    }
    {
        const auto old=std::filesystem::current_path();TestTempDirectory temp("strut-cli-init-project");std::filesystem::current_path(temp.path());
        std::ofstream(temp.path()/"main.p")<<"function main() -> int { return 0; }\n";
        char arg0[]="strut";char arg1[]="init";char* init_argv[]={arg0,arg1};std::ostringstream init_out,init_err;
        require(strut::run_cli(2,init_argv,init_out,init_err)==0,"init complete project");require(std::filesystem::exists(temp.path()/"strut.json"),"init creates manifest");
        char arg2[]="project";char arg3[]="--json";char* project_argv[]={arg0,arg2,arg3};std::ostringstream out,err;
        require(strut::run_cli(3,project_argv,out,err)==0,"project JSON status");require(out.str().find("\"schema_version\": 1")!=std::string::npos,"project JSON schema version");require(out.str().find("\"manifest_exists\": true")!=std::string::npos,"project JSON manifest state");
        const auto package_home=temp.path()/"package-home";
#ifdef _WIN32
        _putenv_s("STRUT_HOME",package_home.string().c_str());
#else
        setenv("STRUT_HOME",package_home.string().c_str(),1);
#endif
        const auto dependency=temp.path()/"dependency";std::filesystem::create_directories(dependency);std::ofstream(dependency/"strut.json")<<R"({"name":"demo-package","version":"1.2.3","entry":"main.p"})";std::ofstream(dependency/"main.p")<<"function demo() -> int { return 1; }\n";
        char add_command[]="add";auto dependency_argument=dependency.string();char* add_argv[]={arg0,add_command,dependency_argument.data()};std::ostringstream add_out,add_err;require(strut::run_cli(3,add_argv,add_out,add_err)==0,"add package for introspection");
        char packages_command[]="packages";char* packages_argv[]={arg0,packages_command,arg3};std::ostringstream packages_out,packages_err;require(strut::run_cli(3,packages_argv,packages_out,packages_err)==0,"packages JSON status");const auto package_json=packages_out.str();require(package_json.find("\"command\": \"packages\"")!=std::string::npos,"packages JSON command");require(package_json.find("\"requested_constraint\": \"1.2.3\"")!=std::string::npos,"packages requested constraint");require(package_json.find("\"resolved_version\": \"1.2.3\"")!=std::string::npos,"packages resolved version");require(package_json.find("\"offline_available\": true")!=std::string::npos,"packages offline state");require(package_json.find("\"direct\": true")!=std::string::npos,"packages direct state");
        std::ostringstream project_after,project_after_err;require(strut::run_cli(3,project_argv,project_after,project_after_err)==0,"extended project JSON status");const auto project_json=project_after.str();require(project_json.find("\"lockfile_schema_version\": 2")!=std::string::npos,"project lock schema");require(project_json.find("\"dependency_count\": 1")!=std::string::npos,"project dependency count");require(project_json.find("\"direct_dependency_count\": 1")!=std::string::npos,"project direct count");require(project_json.find("\"transitive_dependency_count\": 0")!=std::string::npos,"project transitive count");require(project_json.find("\"graph_fully_resolved\": true")!=std::string::npos,"project resolved graph");require(project_json.find("\"all_locked_packages_cached\": true")!=std::string::npos,"project cache state");
        char bad_option[]="--bad";char* bad_packages_argv[]={arg0,packages_command,bad_option};std::ostringstream bad_out,bad_err;require(strut::run_cli(3,bad_packages_argv,bad_out,bad_err)==2,"packages invalid option exit status");
        strut::PackageLock package_lock;std::string package_error;require(strut::load_package_lock_file(temp.path()/"strut.lock.json",package_lock,package_error),"load introspection lock");const auto& locked_package=package_lock.packages.front();std::filesystem::remove_all(strut::package_cache_root()/locked_package.name/locked_package.version/locked_package.checksum.substr(7));std::ostringstream missing_out,missing_err;require(strut::run_cli(3,packages_argv,missing_out,missing_err)==0,"missing cache keeps valid lock exit status");require(missing_out.str().find("\"offline_available\": false")!=std::string::npos,"missing cache offline state");
        std::ofstream(temp.path()/"strut.json")<<R"({"name":"strut-project","version":"0.1.0","dependencies":{"demo-package":"2.0.0"}})";std::ostringstream stale_out,stale_err;require(strut::run_cli(3,packages_argv,stale_out,stale_err)==1,"stale lock exit status");require(stale_out.str().find("\"lock_state\": \"stale\"")!=std::string::npos,"stale lock JSON state");
        std::ofstream(temp.path()/"strut.lock.json")<<"not json\n";std::ostringstream corrupt_out,corrupt_err;require(strut::run_cli(3,packages_argv,corrupt_out,corrupt_err)==1,"corrupt lock exit status");require(corrupt_out.str().find("\"lock_state\": \"corrupt\"")!=std::string::npos,"corrupt lock JSON state");
        std::filesystem::current_path(old);
    }
    return 0;
}
