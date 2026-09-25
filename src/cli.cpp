#include <iostream>
#include "strut/cli.h"

#include <filesystem>
#include <iomanip>
#include <unordered_set>
#include <ostream>
#include <string>
#include <string_view>
#include <regex>
#include <algorithm>
#include <fstream>
#include <cstdlib>

#include "json.h"
#include "strut/lexer.h"
#include "strut/ir.h"
#include "strut/lsp.h"
#include "strut/codegen.h"
#include "strut/diagnostic.h"
#include "strut/formatter.h"
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
        << "Usage: strut <source.p> [compile options]\n"
        << "       strut <command> [options]\n\n"
        << "Commands:\n"
        << "  compile <file>    Explicit form of 'strut <file>'\n"
        << "  init              Create .strut/config.json\n"
        << "  make              Build the current project\n"
        << "  test [filter]     Build and run tests/**/*_test.p\n"
        << "  fmt [path]        Format source (project by default)\n"
        << "  add <path>        Add a local package checkout\n"
        << "  remove <name>     Remove a package dependency\n"
        << "  list              List project dependencies\n"
        << "  install           Resolve dependencies from the shared cache\n"
        << "  lsp               Run the Language Server Protocol server on stdio\n"
        << "  help [command]    Show general or command help\n\n"
        << "Compile options:\n"
        << "  -o <path>         Output executable path\n"
        << "  --check           Parse/type-check only\n"
        << "  --dump-tokens     Print lexer tokens\n"
        << "  --release         Optimise, strip and enable dead-code elimination\n"
        << "  --static          Request fully static final linking where supported\n"
        << "  --dynamic         Prefer ordinary dynamic final linking\n"
        << "  --lib <name>      Link a native library using platform-default mode\n"
        << "  --static-lib <n>  Link one native library statically\n"
        << "  --dynamic-lib <n> Link one native library dynamically\n"
        << "  --lib-path <dir>  Add a native library search path\n"
        << "  --verbose         Explain object rebuild/reuse decisions\n\n"
        << "Global options:\n"
        << "  -h, --help        Show help\n"
        << "  -v, --version     Show compiler version\n"
        << "  --json            With --version, emit JSON metadata\n\n"
        << "Exit codes: 0 success, 1 compile/build/test failure, 2 command-line usage/configuration error.\n";
}

void print_command_help(std::string_view command, std::ostream& out) {
    if (command == "compile") out << "Usage: strut compile <source.p> [-o path] [--release] [link options]\n";
    else if (command == "init") out << "Usage: strut init\nCreates .strut/config.json for the current project.\n";
    else if (command == "make") out << "Usage: strut make [--release] [--verbose]\nBuilds the configured project entrypoint using incremental object metadata.\n";
    else if (command == "test") out << "Usage: strut test [filter] [--verbose]\nDiscovers tests/**/*_test.p; tests run deterministically and sequentially.\n";
    else if (command == "fmt") out << "Usage: strut fmt [path] [--check]\nFormats .p/.h files; --check reports drift without writing.\n";
    else if (command == "add") out << "Usage: strut add <local-package-path>\n";
    else if (command == "remove") out << "Usage: strut remove <package-name>\n";
    else if (command == "list") out << "Usage: strut list\n";
    else if (command == "install") out << "Usage: strut install\n";
    else if (command == "lsp") out << "Usage: strut lsp\nRuns the Strut LSP server over stdin/stdout.\n";
    else print_help(out);
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
                            std::unordered_set<std::string>& active, std::ostream& err, std::vector<std::filesystem::path>* dependencies = nullptr) {
    std::error_code ec;
    const auto absolute = std::filesystem::absolute(path, ec).lexically_normal();
    const std::string key = (ec ? path : absolute).string();
    if (loaded.find(key) != loaded.end()) return true;
    if (dependencies) dependencies->push_back(absolute);
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
                if (dependencies) dependencies->push_back(project_root / "strut.json");
                if (!load_package_manifest_file(project_root / "strut.json", project, package_error)) { err << path.string() << ": error: package include requires project strut.json: " << package_error << '\n'; active.erase(key); return false; }
                const auto requirement = project.dependencies.find(package_name); if (requirement == project.dependencies.end()) { err << path.string() << ": error: package '" << package_name << "' is not a project dependency\n"; active.erase(key); return false; }
                auto package_root = resolve_cached_package(package_name, requirement->second); if (!package_root) { err << path.string() << ": error: package '" << package_name << "' is not installed in the shared cache\n"; active.erase(key); return false; }
                PackageManifest package; if (dependencies) dependencies->push_back(*package_root / "strut.json"); if (!load_package_manifest_file(*package_root / "strut.json", package, package_error)) { err << path.string() << ": error: " << package_error << '\n'; active.erase(key); return false; }
                std::filesystem::path dep = *package_root; if (slash == std::string::npos) { if (package.entry.empty()) { err << path.string() << ": error: package '" << package_name << "' has no entry\n"; active.erase(key); return false; } dep /= package.entry; } else dep /= st->name.substr(slash + 1);
                if (!load_program_recursive(dep, project_root, combined, loaded, active, err, dependencies)) { active.erase(key); return false; }
                continue;
            }
            auto dep = path.parent_path() / st->name;
            if (!load_program_recursive(dep, project_root, combined, loaded, active, err, dependencies)) { active.erase(key); return false; }
        } else own.push_back(std::move(st));
    }
    for(auto& st:own) combined.statements.push_back(std::move(st));
    active.erase(key); loaded.insert(key); return true;
}

bool load_program(const std::filesystem::path& path, Program& combined, std::ostream& err, std::vector<std::filesystem::path>* dependencies = nullptr) {
    std::unordered_set<std::string> loaded, active;
    std::filesystem::path root = path.parent_path().empty() ? std::filesystem::current_path() : std::filesystem::absolute(path.parent_path());
    for (auto probe = root; !probe.empty(); probe = probe.parent_path()) { if (std::filesystem::exists(probe / "strut.json")) { root = probe; break; } if (probe == probe.root_path()) break; }
    return load_program_recursive(path, root, combined, loaded, active, err, dependencies);
}

int check_source(const std::filesystem::path& path, std::ostream& out, std::ostream& err) {
    (void)out; Program program; if(!load_program(path,program,err)) return 1;
    SemanticAnalyzer sema; auto checked=sema.analyze(program);
    for(const auto& d:checked.diagnostics) err<<format_diagnostic(path.string(),d)<<'\n';
    for(const auto& w:checked.warnings) err<<format_warning(path.string(),w)<<'\n';
    return checked.ok()?0:1;
}

void collect_embed_dependencies(const std::filesystem::path& source_path,std::vector<std::filesystem::path>& dependencies){
    std::ifstream f(source_path);if(!f)return;std::ostringstream ss;ss<<f.rdbuf();const std::string text=ss.str();const std::regex pattern("(embed_file|embed_dir)\\s*\\(\\s*\"([^\"]+)\"");
    for(std::sregex_iterator it(text.begin(),text.end(),pattern),end;it!=end;++it){std::filesystem::path p=(*it)[2].str();if(p.is_relative())p=std::filesystem::current_path()/p;std::error_code ec;if(std::filesystem::is_directory(p,ec)){for(const auto&e:std::filesystem::recursive_directory_iterator(p,ec)){if(ec)break;if(e.is_regular_file())dependencies.push_back(std::filesystem::absolute(e.path()));}}else dependencies.push_back(std::filesystem::absolute(p));}
}

int compile_source(const std::filesystem::path& path, const std::filesystem::path& output, const NativeLinkOptions& link, std::ostream& out, std::ostream& err, bool verbose) {
    Program program; std::vector<std::filesystem::path> dependencies; if(!load_program(path,program,err,&dependencies)) return 1;
    for (const auto& dep : std::vector<std::filesystem::path>(dependencies)) collect_embed_dependencies(dep, dependencies);
    std::sort(dependencies.begin(), dependencies.end()); dependencies.erase(std::unique(dependencies.begin(), dependencies.end()), dependencies.end());
    SemanticAnalyzer sema; auto checked=sema.analyze(program);
    for(const auto& d:checked.diagnostics) err<<format_diagnostic(path.string(),d)<<'\n';
    for(const auto& w:checked.warnings) err<<format_warning(path.string(),w)<<'\n';
    if(!checked.ok())return 1;
    IRLowerer lowerer; auto lowered=lowerer.lower(program); if(!lowered.ok())return 1; lowered.program.source_path=std::filesystem::absolute(path).generic_string();
    CppBackend backend; std::string backend_error;
    const auto root = find_project_root(path);
    const auto config_path = root / ".strut" / "config.json";
    if (std::filesystem::exists(config_path)) {
        BuildConfig config; std::string config_error;
        if (!load_build_config(config_path, config, config_error)) { err << path.string() << ": error: " << config_error << '\n'; return 1; }
        std::error_code ec; auto rel = std::filesystem::relative(std::filesystem::absolute(path), root, ec);
        if (!ec && rel.extension() == ".p") {
#ifdef _WIN32
            const char* object_ext = ".obj";
#else
            const char* object_ext = ".o";
#endif
            const std::string mode = link.release ? "release" : config.mode;
            auto unit = rel; unit.replace_extension("");
            const auto object = root / ".strut" / "obj" / config.target / mode / unit; auto object_with_ext=object; object_with_ext += object_ext;
            auto generated = root / ".strut" / "gen" / config.target / mode / unit; generated += ".cpp";
            auto info_path = root / ".strut" / "info" / config.target / mode / unit; info_path += ".info.json";
            ObjectBuildInfo info; info.source=rel.generic_string(); info.object=std::filesystem::relative(object_with_ext,root,ec).generic_string(); info.compiler_version=std::string(version); info.target=config.target; info.mode=mode; info.fingerprint=build_fingerprint(config,link.release);
            for(const auto& dep:dependencies){auto relative=std::filesystem::relative(dep,root,ec);info.dependencies.push_back(ec?dep.generic_string():relative.generic_string());ec.clear();}
            std::vector<std::string> reasons;const bool current=object_build_is_current(root,info_path,info,reasons);
            if(!current){if(verbose){out<<"rebuild "<<rel.generic_string();for(const auto&r:reasons)out<<"\n  - "<<r;out<<'\n';}if(!backend.compile_object(lowered.program,object_with_ext,generated,backend_error,link)){err<<path.string()<<": error: "<<backend_error<<'\n';return 1;}std::string info_error;if(!write_object_build_info(info_path,info,info_error)){err<<path.string()<<": error: "<<info_error<<'\n';return 1;}}
            else if(verbose) out<<"reuse "<<info.object<<'\n';
            if(current && std::filesystem::exists(output)){std::error_code time_ec;const auto out_time=std::filesystem::last_write_time(output,time_ec);const auto obj_time=std::filesystem::last_write_time(object_with_ext,time_ec);if(!time_ec&&out_time>=obj_time){if(verbose)out<<"output up to date "<<output.generic_string()<<'\n';return 0;}}
            if(!backend.link_objects(lowered.program,{object_with_ext},output,backend_error,link)){err<<path.string()<<": error: "<<backend_error<<'\n';return 1;}return 0;
        }
    }
    if(!backend.compile(lowered.program,output,backend_error,link)){err<<path.string()<<": error: "<<backend_error<<'\n';return 1;}
    return 0;
}


int run_make_command(bool release_override, bool verbose, std::ostream& out, std::ostream& err) {
    const auto root = find_project_root(std::filesystem::current_path());
    const auto config_path = root / ".strut" / "config.json";
    if (!std::filesystem::exists(config_path)) {
        err << "strut: no .strut/config.json found; run 'strut init' first\n";
        return 2;
    }
    BuildConfig config; std::string config_error;
    if (!load_build_config(config_path, config, config_error)) { err << "strut: " << config_error << '\n'; return 1; }
    const auto source = root / config.entrypoint;
    if (!std::filesystem::exists(source)) { err << "strut: project entrypoint not found: " << source.string() << '\n'; return 1; }
    auto output = root / config.output;
#ifdef _WIN32
    if (output.extension().empty()) output += ".exe";
#endif
    NativeLinkOptions link;
    link.release = release_override || config.mode == "release";
    link.fully_static = config.linking == "static";
    link.prefer_dynamic = config.linking == "dynamic";
    if (verbose) out << "project " << root.generic_string() << '\n';
    return compile_source(source, output, link, out, err, verbose);
}


std::string shell_quote_path(const std::filesystem::path& path) {
#ifdef _WIN32
    return std::string("\"") + path.string() + "\"";
#else
    std::string q = "'";
    for (char c : path.string()) q += c == '\'' ? "'\\''" : std::string(1, c);
    q += "'"; return q;
#endif
}

int run_test_command(const std::string& filter, bool verbose, std::ostream& out, std::ostream& err) {
    const auto root = find_project_root(std::filesystem::current_path());
    const auto tests_root = root / "tests";
    if (!std::filesystem::exists(tests_root)) { err << "strut: no tests directory found\n"; return 2; }
    std::vector<std::filesystem::path> tests;
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(tests_root, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        const auto p = entry.path(); const auto name = p.filename().string();
        if (p.extension() != ".p" || name.size() < 7 || name.substr(name.size()-7) != "_test.p") continue;
        const auto rel = std::filesystem::relative(p, root, ec).generic_string(); ec.clear();
        if (!filter.empty() && rel.find(filter) == std::string::npos) continue;
        tests.push_back(p);
    }
    std::sort(tests.begin(), tests.end());
    if (tests.empty()) { err << "strut: no tests matched" << (filter.empty() ? "" : " filter '" + filter + "'") << "\n"; return 2; }
    std::size_t passed = 0;
    for (const auto& test : tests) {
        auto rel = std::filesystem::relative(test, tests_root, ec); ec.clear();
        auto exe = root / ".strut" / "tests" / rel; exe.replace_extension("");
#ifdef _WIN32
        exe += ".exe";
#endif
        std::filesystem::create_directories(exe.parent_path(), ec); ec.clear();
        NativeLinkOptions link;
        if (verbose) out << "build " << rel.generic_string() << '\n';
        if (compile_source(test, exe, link, out, err, verbose) != 0) { err << "FAIL " << rel.generic_string() << " (compile)\n"; continue; }
        if (verbose) out << "run " << rel.generic_string() << '\n';
        const int rc = std::system(shell_quote_path(exe).c_str());
        if (rc == 0) { ++passed; out << "PASS " << rel.generic_string() << '\n'; }
        else err << "FAIL " << rel.generic_string() << " (exit " << rc << ")\n";
    }
    out << passed << "/" << tests.size() << " tests passed\n";
    return passed == tests.size() ? 0 : 1;
}


int run_fmt_command(const std::filesystem::path& requested, bool check_only, std::ostream& out, std::ostream& err) {
    const auto root = find_project_root(std::filesystem::current_path());
    const auto target = requested.empty() ? root : (requested.is_absolute() ? requested : std::filesystem::current_path() / requested);
    if (!std::filesystem::exists(target)) { err << "strut: format path not found: " << target.string() << '\n'; return 2; }
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    auto add = [&](const std::filesystem::path& p){ if (SourceFile::has_strut_extension(p)) files.push_back(p); };
    if (std::filesystem::is_regular_file(target, ec)) add(target);
    else for (const auto& e : std::filesystem::recursive_directory_iterator(target, ec)) {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        if (e.path().string().find((root / ".strut").string()) == 0) continue;
        add(e.path());
    }
    std::sort(files.begin(), files.end());
    bool differs=false;
    for(const auto& file:files){
        std::string load_error; auto source=SourceFile::load(file,load_error); if(!source){err<<"strut: "<<load_error<<'\n';return 1;}
        Program parsed; std::ostringstream parse_errors; if(!load_program(file,parsed,parse_errors)){err<<parse_errors.str();return 1;}
        const auto formatted=format_source_text(source->text()); if(formatted==source->text())continue; differs=true;
        if(check_only){out<<std::filesystem::relative(file,root,ec).generic_string()<<" needs formatting\n";ec.clear();continue;}
        std::ofstream f(file,std::ios::binary|std::ios::trunc);if(!f){err<<"strut: unable to write "<<file.string()<<'\n';return 1;}f<<formatted;
        out<<"formatted "<<std::filesystem::relative(file,root,ec).generic_string()<<'\n';ec.clear();
    }
    return check_only&&differs?1:0;
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
    if (argc >= 2 && std::string_view(argv[1]) == "lsp") return run_lsp(std::cin, out, err);
    bool want_version = false;
    bool want_json = false;
    bool want_dump_tokens = false;
    bool want_check = false;
    std::filesystem::path source_path;
    std::filesystem::path output_path;
    bool explicit_compile = false;
    bool verbose = false;
    NativeLinkOptions link_options;

    if (argc == 1) {
        print_help(out);
        return 0;
    }

    if (argc >= 2) {
        const std::string command(argv[1]);
        if (command == "help") {
            if (argc > 3) { err << "strut: help accepts at most one command\n"; return 2; }
            if (argc == 2) print_help(out); else print_command_help(argv[2], out);
            return 0;
        }
        if (argc >= 3 && (std::string_view(argv[2]) == "--help" || std::string_view(argv[2]) == "-h") && command != "compile") {
            print_command_help(command, out); return 0;
        }
        if (command == "init") {
            if (argc != 2) { err << "strut: init takes no arguments\n"; return 2; }
            std::string init_error;
            if (!init_project_build_state(std::filesystem::current_path(), init_error)) { err << "strut: " << init_error << '\n'; return 1; }
            out << "created .strut/config.json\n"; return 0;
        }
        if (command == "fmt") {
            std::filesystem::path format_path; bool check_only = false;
            for(int i=2;i<argc;++i){const std::string_view arg(argv[i]); if(arg=="--check")check_only=true; else if(!arg.empty()&&arg.front()=='-'){err<<"strut: unsupported fmt option '"<<arg<<"'\n";return 2;} else if(format_path.empty())format_path=std::string(arg); else{err<<"strut: fmt accepts at most one path\n";return 2;}}
            return run_fmt_command(format_path,check_only,out,err);
        }
        if (command == "test") {
            std::string filter; bool test_verbose = false;
            for (int i = 2; i < argc; ++i) {
                const std::string_view arg(argv[i]);
                if (arg == "--verbose") test_verbose = true;
                else if (!arg.empty() && arg.front() == '-') { err << "strut: unsupported test option '" << arg << "'\n"; return 2; }
                else if (filter.empty()) filter = std::string(arg);
                else { err << "strut: test accepts at most one filter\n"; return 2; }
            }
            return run_test_command(filter, test_verbose, out, err);
        }
        if (command == "make") {
            bool release = false; bool make_verbose = false;
            for (int i = 2; i < argc; ++i) {
                const std::string_view arg(argv[i]);
                if (arg == "--release") release = true;
                else if (arg == "--verbose") make_verbose = true;
                else { err << "strut: unsupported make option '" << arg << "'\n"; return 2; }
            }
            return run_make_command(release, make_verbose, out, err);
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
        if (arg == "--verbose") { verbose = true; continue; }
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
        return compile_source(source_path, output_path, link_options, out, err, verbose);
    }

    (void)explicit_compile;
    return 0;
}
}
