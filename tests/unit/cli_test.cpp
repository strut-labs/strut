#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
#include <fstream>

#include "strut/cli.h"
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
        std::filesystem::current_path(old);
    }
    return 0;
}
