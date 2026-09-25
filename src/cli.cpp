#include "strut/cli.h"

#include <filesystem>
#include <iomanip>
#include <ostream>
#include <string>
#include <string_view>

#include "json.h"
#include "strut/lexer.h"
#include "strut/source.h"
#include "strut/token.h"
#include "strut/version.h"

namespace strut {
namespace {
void print_help(std::ostream& out) {
    out << "Strut " << version << "\n"
        << "Usage: strut [options] [source.p|source.h]\n\n"
        << "Options:\n"
        << "  -h, --help          Show this help\n"
        << "  -v, --version       Show compiler version\n"
        << "      --json          With --version, emit JSON metadata\n"
        << "      --dump-tokens   Lex a .p/.h file and print its token stream\n";
}

std::string escaped_lexeme(std::string_view value) {
    std::string out;
    for (const char c : value) {
        switch (c) {
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            default: out += c; break;
        }
    }
    return out;
}

int dump_tokens(const std::filesystem::path& path, std::ostream& out, std::ostream& err) {
    std::string load_error;
    auto source = SourceFile::load(path, load_error);
    if (!source) {
        err << path.string() << ": error: " << load_error << '\n';
        return 2;
    }

    Lexer lexer(*source);
    auto result = lexer.lex();
    for (const auto& diagnostic : result.diagnostics) {
        err << path.string() << ':' << diagnostic.span.begin.line << ':' << diagnostic.span.begin.column
            << ": error: " << diagnostic.message << '\n';
    }
    if (!result.ok()) return 1;

    for (const auto& token : result.tokens) {
        out << token.span.begin.line << ':' << token.span.begin.column << ' '
            << token_kind_name(token.kind);
        if (token.kind != TokenKind::end_of_file) {
            out << " \"" << escaped_lexeme(token.lexeme) << '"';
        }
        out << '\n';
    }
    return 0;
}
}

int run_cli(int argc, char** argv, std::ostream& out, std::ostream& err) {
    bool want_version = false;
    bool want_json = false;
    bool want_dump_tokens = false;
    std::filesystem::path source_path;

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
        if (arg == "--dump-tokens") {
            want_dump_tokens = true;
            continue;
        }
        if (!arg.empty() && arg.front() == '-') {
            err << "strut: unsupported option '" << arg << "'\n";
            return 2;
        }
        if (!source_path.empty()) {
            err << "strut: only one source file is accepted at this stage\n";
            return 2;
        }
        source_path = std::filesystem::path(std::string(arg));
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
    if (want_dump_tokens) {
        if (source_path.empty()) {
            err << "strut: --dump-tokens requires a .p or .h source file\n";
            return 2;
        }
        return dump_tokens(source_path, out, err);
    }
    if (!source_path.empty()) {
        if (!SourceFile::has_strut_extension(source_path)) {
            err << source_path.string() << ": error: expected a Strut .p or .h source file\n";
            return 2;
        }
        err << "strut: compilation is not implemented yet; use --dump-tokens while bootstrapping\n";
        return 2;
    }

    return 0;
}
}
