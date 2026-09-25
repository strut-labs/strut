#include <iostream>

#include "strut/cli.h"

int main(int argc, char** argv) {
    return strut::run_cli(argc, argv, std::cout, std::cerr);
}
