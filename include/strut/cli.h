#pragma once

#include <iosfwd>

namespace strut {
int run_cli(int argc, char** argv, std::ostream& out, std::ostream& err);
}
