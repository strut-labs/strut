#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "strut/cli.h"

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

    return 0;
}
