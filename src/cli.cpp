#include "strut/cli.h"

#include <ostream>
#include <string_view>

#include "json.h"
#include "strut/version.h"

namespace strut {
namespace {
void print_help(std::ostream& out) {
    out << "Strut " << version << "\n"
        << "Usage: strut [options] [source.p]\n\n"
        << "Options:\n"
        << "  -h, --help       Show this help\n"
        << "  -v, --version    Show compiler version\n"
        << "      --json       With --version, emit JSON metadata\n";
}
}

int run_cli(int argc, char** argv, std::ostream& out, std::ostream& err) {
    bool want_version = false;
    bool want_json = false;

    if (argc == 1) {
        print_help(out);
        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "-h" || arg == "--help") {
            print_help(out);
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            want_version = true;
            continue;
        }
        if (arg == "--json") {
            want_json = true;
            continue;
        }

        err << "strut: unsupported argument '" << arg << "'\n";
        return 2;
    }

    if (want_version) {
        if (!want_json) {
            out << "strut " << version << '\n';
            return 0;
        }

        json::Document metadata = json::Document::make_object();
        metadata["name"] = "strut";
        metadata["version"] = std::string(version);
        metadata["jsonic"] = std::string(json::version);
        out << metadata.dump(2) << '\n';
        return 0;
    }

    if (want_json) {
        err << "strut: --json currently requires --version\n";
        return 2;
    }

    return 0;
}
}
