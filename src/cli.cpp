#include "strut/cli.h"

#include <filesystem>
#include <iomanip>
#include <unordered_set>
#include <ostream>
#include <string>
#include <string_view>

#include "json.h"
#include "strut/lexer.h"
#include "strut/ir.h"
#include "strut/codegen.h"
#include "strut/diagnostic.h"
#include "strut/parser.h"
#include "strut/package.h"
#include "strut/project.h"
#include "strut/source.h"
#include "strut/sema.h"
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
        << "      --dump-tokens   Lex a .p/.h file and print its token stream\n"
        << "      --check         Parse/check a .p/.h file without code generation\n"
        << "  -o <path>           Write compiled executable to path\n"
        << "      --lib <name>     Link a native library using platform-default mode\n"
        << "      --static-lib <n> Link one native library statically where supported\n"
        << "      --dynamic-lib <n> Link one native library dynamically\n"
        << "      --lib-path <dir> Add a native library search path\n"
        << "      --static         Request a fully static final link where supported\n"
        << "      --dynamic        Prefer an ordinary dynamically linked final binary\n"
        << "      --release        Optimise, strip and enable dead-code elimination\n"
        << "      compile         Optional explicit compile command alias\n"
        << "      add <path>      Add a local package checkout to this project\n"
        << "      remove <name>   Remove a package dependency\n"
        << "      list            List project dependencies\n"
        << "      install         Resolve dependencies from the shared cache\n"
        << "      init            Create .strut/config.json build configuration\n";
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
        err << format_diagnostic(path.string(), diagnostic) << '\n';
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
bool load_program_recursive(const std::filesystem::path& path, const std::filesystem::path& project_root, Program& combined, std::unordered_set<std::string>& loaded,
                            std::unordered_set<std::string>& active, std::ostream& err) {
    std::error_code ec;
    const auto absolute = std::filesystem::absolute(path, ec).lexically_normal();
    const std::string key = (ec ? path : absolute).string();
    if (loaded.find(key) != loaded.end()) return true;
    if (!active.insert(key).second) { err << path.string() << ": error: include cycle detected\n"; return false; }
    std::string load_error; auto source = SourceFile::load(path, load_error);
    if (!source) { err << path.string() << ": error: " << load_error << '\n'; active.erase(key); return false; }
    Lexer lexer(*source); auto lexed=lexer.lex();
    for(const auto& d:lexed.diagnostics) err << format_diagnostic(path.string(),d) << '\n';
    if(!lexed.ok()){active.erase(key);return false;}
    Parser parser(lexed.tokens); auto parsed=parser.parse();
    for(const auto& d:parsed.diagnostics) err << format_diagnostic(path.string(),d) << '\n';
    if(!parsed.ok()){active.erase(key);return false;}
    std::vector<StmtPtr> own;
    for (auto& st : parsed.program.statements) {
        if (st->kind == Stmt::Kind::include_stmt) {
            if (st->include_is_package) {
                const auto slash = st->name.find('/'); const std::string package_name = st->name.substr(0, slash);
                PackageManifest project; std::string package_error;
                if (!load_package_manifest_file(project_root / "strut.json", project, package_error)) { err << path.string() << ": error: package include requires project strut.json: " << package_error << '\n'; active.erase(key); return false; }
                const auto requirement = project.dependencies.find(package_name); if (requirement == project.dependencies.end()) { err << path.string() << ": error: package '" << package_name << "' is not a project dependency\n"; active.erase(key); return false; }
                auto package_root = resolve_cached_package(package_name, requirement->second); if (!package_root) { err << path.string() << ": error: package '" << package_name << "' is not installed in the shared cache\n"; active.erase(key); return false; }
                PackageManifest package; if (!load_package_manifest_file(*package_root / "strut.json", package, package_error)) { err << path.string() << ": error: " << package_error << '\n'; active.erase(key); return false; }
                std::filesystem::path dep = *package_root; if (slash == std::string::npos) { if (package.entry.empty()) { err << path.string() << ": error: package '" << package_name << "' has no entry\n"; active.erase(key); return false; } dep /= package.entry; } else dep /= st->name.substr(slash + 1);
                if (!load_program_recursive(dep, project_root, combined, loaded, active, err)) { active.erase(key); return false; }
                continue;
            }
            auto dep = path.parent_path() / st->name;
            if (!load_program_recursive(dep, project_root, combined, loaded, active, err)) { active.erase(key); return false; }
        } else own.push_back(std::move(st));
    }
    for(auto& st:own) combined.statements.push_back(std::move(st));
    active.erase(key); loaded.insert(key); return true;
}

bool load_program(const std::filesystem::path& path, Program& combined, std::ostream& err) {
    std::unordered_set<std::string> loaded, active;
    std::filesystem::path root = path.parent_path().empty() ? std::filesystem::current_path() : std::filesystem::absolute(path.parent_path());
    for (auto probe = root; !probe.empty(); probe = probe.parent_path()) { if (std::filesystem::exists(probe / "strut.json")) { root = probe; break; } if (probe == probe.root_path()) break; }
    return load_program_recursive(path, root, combined, loaded, active, err);
}

int check_source(const std::filesystem::path& path, std::ostream& out, std::ostream& err) {
    (void)out; Program program; if(!load_program(path,program,err)) return 1;
    SemanticAnalyzer sema; auto checked=sema.analyze(program);
    for(const auto& d:checked.diagnostics) err<<format_diagnostic(path.string(),d)<<'\n';
    for(const auto& w:checked.warnings) err<<format_warning(path.string(),w)<<'\n';
    return checked.ok()?0:1;
}

int compile_source(const std::filesystem::path& path, const std::filesystem::path& output, const NativeLinkOptions& link, std::ostream& err) {
    Program program; if(!load_program(path,program,err)) return 1;
    SemanticAnalyzer sema; auto checked=sema.analyze(program);
    for(const auto& d:checked.diagnostics) err<<format_diagnostic(path.string(),d)<<'\n';
    for(const auto& w:checked.warnings) err<<format_warning(path.string(),w)<<'\n';
    if(!checked.ok())return 1;
    IRLowerer lowerer; auto lowered=lowerer.lower(program); if(!lowered.ok())return 1;
    CppBackend backend; std::string backend_error;
    if(!backend.compile(lowered.program,output,backend_error,link)){err<<path.string()<<": error: "<<backend_error<<'\n';return 1;}
    return 0;
}

int run_package_command(const std::string& command, const std::string& argument, std::ostream& out, std::ostream& err) {
    const auto root = std::filesystem::current_path(); PackageManifest project; std::string error;
    if (!load_package_manifest_file(root / "strut.json", project, error)) { err << "strut: " << error << '\n'; return 2; }
    if (command == "list") { for (const auto& dep : project.dependencies) out << dep.first << " " << dep.second << '\n'; return 0; }
    if (command == "add") { if (argument.empty()) { err << "strut: add requires a local package path\n"; return 2; } PackageManifest package; std::filesystem::path cached; if (!cache_local_package(argument,cached,package,error)) { err << "strut: " << error << '\n'; return 1; } project.dependencies[package.name]=package.version; if(!write_package_manifest_file(root/"strut.json",project,error)||!write_lockfile(root,project,error)){err<<"strut: "<<error<<'\n';return 1;} out<<"added "<<package.name<<" "<<package.version<<'\n'; return 0; }
    if (command == "remove") { if (argument.empty()) { err << "strut: remove requires a package name\n"; return 2; } if(!project.dependencies.erase(argument)){err<<"strut: package '"<<argument<<"' is not a dependency\n";return 2;} if(!write_package_manifest_file(root/"strut.json",project,error)){err<<"strut: "<<error<<'\n';return 1;} if(!project.dependencies.empty()&&!write_lockfile(root,project,error)){err<<"strut: "<<error<<'\n';return 1;} if(project.dependencies.empty()){std::error_code ec;std::filesystem::remove(root/"strut.lock.json",ec);} out<<"removed "<<argument<<'\n';return 0; }
    if (command == "install") { if(!write_lockfile(root,project,error)){err<<"strut: "<<error<<'\n';return 1;} out<<"dependencies resolved from cache\n";return 0; }
    return 2;
}

}

int run_cli(int argc, char** argv, std::ostream& out, std::ostream& err) {
    bool want_version = false;
    bool want_json = false;
    bool want_dump_tokens = false;
    bool want_check = false;
    std::filesystem::path source_path;
    std::filesystem::path output_path;
    bool explicit_compile = false;
    NativeLinkOptions link_options;

    if (argc == 1) {
        print_help(out);
        return 0;
    }

    if (argc >= 2) {
        const std::string command(argv[1]);
        if (command == "init") {
            if (argc != 2) { err << "strut: init takes no arguments\n"; return 2; }
            std::string init_error;
            if (!init_project_build_state(std::filesystem::current_path(), init_error)) { err << "strut: " << init_error << '\n'; return 1; }
            out << "created .strut/config.json\n"; return 0;
        }
        if (command == "add" || command == "remove" || command == "list" || command == "install") {
            const std::string argument = argc >= 3 ? argv[2] : std::string();
            if ((command == "list" || command == "install") && argc > 2) { err << "strut: " << command << " takes no argument\n"; return 2; }
            if ((command == "add" || command == "remove") && argc != 3) { err << "strut: " << command << " requires exactly one argument\n"; return 2; }
            return run_package_command(command, argument, out, err);
        }
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
        if (arg == "--dump-tokens") { want_dump_tokens = true; continue; }
        if (arg == "--check") { want_check = true; continue; }
        if (arg == "--static") { link_options.fully_static = true; link_options.prefer_dynamic = false; continue; }
        if (arg == "--dynamic") { link_options.prefer_dynamic = true; link_options.fully_static = false; continue; }
        if (arg == "--release") { link_options.release = true; continue; }
        if (arg == "--lib" || arg == "--static-lib" || arg == "--dynamic-lib") {
            if (i + 1 >= argc) { err << "strut: " << arg << " requires a library name or path\n"; return 2; }
            NativeLinkMode mode = NativeLinkMode::platform_default; if(arg=="--static-lib")mode=NativeLinkMode::static_link;else if(arg=="--dynamic-lib")mode=NativeLinkMode::dynamic_link;
            std::string value(argv[++i]); std::filesystem::path lp(value);
            if ((lp.has_parent_path() || lp.is_absolute()) && !std::filesystem::exists(lp)) { err << "strut: native library not found: " << value << '\n'; return 2; }
            link_options.libraries.push_back(NativeLibrary{std::move(value),mode}); continue;
        }
        if (arg == "--lib-path") { if(i+1>=argc){err<<"strut: --lib-path requires a directory\n";return 2;} link_options.search_paths.emplace_back(argv[++i]); continue; }
        if (arg == "compile" && source_path.empty()) { explicit_compile = true; continue; }
        if (arg == "-o") {
            if (i + 1 >= argc) { err << "strut: -o requires an output path\n"; return 2; }
            output_path = std::filesystem::path(argv[++i]); continue;
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
    if (want_check) {
        if (source_path.empty()) { err << "strut: --check requires a .p or .h source file\n"; return 2; }
        return check_source(source_path, out, err);
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
        if (output_path.empty()) {
            output_path = source_path.parent_path() / source_path.stem();
#ifdef _WIN32
            output_path += ".exe";
#endif
        }
        return compile_source(source_path, output_path, link_options, err);
    }

    (void)explicit_compile;
    return 0;
}
}
