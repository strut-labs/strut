#include <iostream>
#include "strut/cli.h"

#include <filesystem>
#include <iomanip>
#include <initializer_list>
#include <unordered_set>
#include <ostream>
#include <string>
#include <string_view>
#include <regex>
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <cctype>

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
#include "strut/api_registry.h"

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
        << "  install [package] Install the lock graph or an official package\n"
        << "  update            Refresh dependency resolutions and the lockfile\n"
        << "  packages [--json] Inspect the resolved package graph and cache state\n"
        << "  project [--json]  Inspect project root, manifest and package cache\n"
        << "  api [query]       Browse the authoritative API index (--json supported)\n"
        << "  lsp               Run the Language Server Protocol server on stdio\n"
        << "  help [command]    Show general or command help\n\n"
        << "Compile options:\n"
        << "  -o <path>         Output executable path\n"
        << "  --check           Parse/type-check only\n"
        << "  --dump-tokens     Print lexer tokens\n"
        << "  --release         Optimise, strip and enable dead-code elimination\n"
        << "  --target <name>   native/linux-x64/linux-arm64/macos-arm64/macos-x64/windows-x64\n"
        << "  --static          Request fully static final linking where supported\n"
        << "  --dynamic         Prefer ordinary dynamic final linking\n"
        << "  --lib <name>      Link a native library using platform-default mode\n"
        << "  --static-lib <n>  Link one native library statically\n"
        << "  --dynamic-lib <n> Link one native library dynamically\n"
        << "  --lib-path <dir>  Add a native library search path\n"
        << "  --verbose         Explain object rebuild/reuse decisions\n"
        << "  --timings         Print compiler phase timings\n"
        << "  --emit-cpp <path> Emit generated C++ and stop before native compilation\n\n"
        << "Global options:\n"
        << "  -h, --help        Show help\n"
        << "  -v, --version     Show compiler version\n"
        << "  --json            Emit machine-readable output where supported\n\n"
        << "Quick start:\n"
        << "  function main() -> int { return 0; }\n"
        << "  function main(string cmd, string[] args) -> int { return 0; }\n"
        << "  Compile with 'strut app.p -o app', then run the native output.\n"
        << "  Range loops use 'for (item : items)'. Checked errors are handled or listed after ':'.\n\n"
        << "Documentation: https://strut-labs.github.io/docs.html\n"
        << "Diagnostics are source-mapped and include corrective help where available.\n\n"
        << "Exit codes: 0 success, 1 compile/build/test failure, 2 command-line usage/configuration error.\n";
}

void print_command_help(std::string_view command, std::ostream& out) {
    if (command == "compile") out << "Usage: strut compile <source.p> [-o path] [--release] [link options]\n";
    else if (command == "init") out << "Usage: strut init\nCreates .strut/config.json and strut.json for the current project.\n";
    else if (command == "make") out << "Usage: strut make [--release] [--verbose]\nBuilds the configured project entrypoint using incremental object metadata.\n";
    else if (command == "test") out << "Usage: strut test [filter] [--verbose]\nDiscovers tests/**/*_test.p; tests run deterministically and sequentially.\n";
    else if (command == "fmt") out << "Usage: strut fmt [path] [--check]\nFormats .p/.h files; --check reports drift without writing.\n";
    else if (command == "add") out << "Usage: strut add <local-package-path>\n";
    else if (command == "remove") out << "Usage: strut remove <package-name>\n";
    else if (command == "list") out << "Usage: strut list\n";
    else if (command == "install") out << "Usage: strut install [<package>[@<version-requirement>] | --offline]\nBare package names resolve only from https://github.com/strut-packages/<package>; explicit Git dependencies remain available in strut.json.\n";
    else if (command == "update") out << "Usage: strut update\nRe-resolves manifest dependencies and rewrites strut.lock.json.\n";
    else if (command == "packages") out << "Usage: strut packages [--json]\nShows the locked dependency graph, immutable identities, and offline cache availability.\n";
    else if (command == "project") out << "Usage: strut project [--json]\nShows the discovered project root, build configuration, manifest and package cache.\n";
    else if (command == "api") out << "Usage: strut api [--json] [query]\nBrowses built-ins, methods, modules, checked errors and native dependencies. Query by name, module, category, summary, or `checked-errors`.\n";
    else if (command == "lsp") out << "Usage: strut lsp\nRuns the Strut LSP server over stdin/stdout with contextual completion, signature help, hover, diagnostics, symbols, formatting, and go-to-definition.\n";
    else print_help(out);
}

json::Document api_index(std::string_view query) {
    json::Document root=json::Document::make_object(); root["schema_version"]=1; root["language_version"]=std::string(version);
    root["query"]=std::string(query);
    json::Document modules=json::Document::make_array();
    for(const auto& name:standard_modules())modules.array.emplace_back(name);
    root["standard_modules"]=modules;
    json::Document functions=json::Document::make_array();
    json::Document methods=json::Document::make_array();
    for(const auto& callable:api_callables()){
        if(!api_matches(callable,query))continue;
        json::Document item=json::Document::make_object();item["name"]=callable.name;item["category"]=callable.category;item["module"]=callable.module;item["owner"]=callable.owner;item["summary"]=callable.summary;
        item["deprecated"]=callable.deprecated;item["reference_url"]=callable.reference_url;json::Document generics=json::Document::make_array();for(const auto& parameter:callable.generic_parameters)generics.array.emplace_back(parameter);item["generic_parameters"]=generics;json::Document platforms=json::Document::make_array();for(const auto& platform:callable.platforms)platforms.array.emplace_back(platform);item["platforms"]=platforms;
        json::Document signatures=json::Document::make_array();for(const auto& overload:callable.overloads)signatures.array.emplace_back(api_signature(callable,overload));item["signatures"]=signatures;if(!callable.overloads.empty())item["signature"]=api_signature(callable,callable.overloads.front());
        json::Document errors=json::Document::make_array();for(const auto& error:callable.checked_errors)errors.array.emplace_back(error);item["checked_errors"]=errors;
        json::Document components=json::Document::make_array();json::Document dependencies=json::Document::make_array();
        for(auto id:callable.runtime_components)if(const auto* component=runtime_component(id)){components.array.emplace_back(std::string(component->name));for(auto lib:component->link_libraries){std::string dependency(lib);if(dependency=="curl")dependency="libcurl";else if(dependency=="sqlite3")dependency="SQLite3";dependencies.array.emplace_back(dependency);}}
        item["runtime_components"]=components;item["native_dependencies"]=dependencies;
        (callable.owner.empty()?functions:methods).array.push_back(std::move(item));
    }
    root["functions"]=functions;
    root["methods"]=methods;
    json::Document commands=json::Document::make_array();for(const char* c:{"compile","init","make","test","fmt","add","remove","list","install","update","packages","project","api","lsp"})commands.array.emplace_back(c);root["cli_commands"]=commands;
    json::Document options=json::Document::make_object();auto option_list=[&](std::initializer_list<const char*> values){json::Document list=json::Document::make_array();for(const auto* value:values)list.array.emplace_back(value);return list;};options["install"]=option_list({"--offline","<package>[@<version-requirement>]"});options["packages"]=option_list({"--json"});options["project"]=option_list({"--json"});options["api"]=option_list({"--json"});options["fmt"]=option_list({"--check"});options["make"]=option_list({"--release","--verbose"});root["cli_options"]=options;
    json::Document notes=json::Document::make_object();notes["range_loop"]="for (item : items)";notes["core_array"]="T[] (no include required)";notes["official_packages"]="strut install <name> resolves only https://github.com/strut-packages/<name> by immutable semantic-version tags";notes["custom_checked_errors"]="Custom error declarations are not currently supported; use documented built-in checked error types.";root["language_notes"]=notes;
    return root;
}

void print_api_index(std::string_view query,std::ostream& out){
    std::size_t matches=0;
    for(const auto& callable:api_callables()){
        if(!api_matches(callable,query))continue;
        ++matches;
        for(const auto& overload:callable.overloads)out<<api_signature(callable,overload)<<'\n';
        out<<"  "<<callable.summary;
        if(!callable.module.empty())out<<" ["<<callable.module<<']';
        if(!callable.checked_errors.empty()){out<<" throws ";for(std::size_t i=0;i<callable.checked_errors.size();++i){if(i)out<<", ";out<<callable.checked_errors[i];}}
        out<<"\n\n";
    }
    if(!matches)out<<"No API entries match '"<<query<<"'.\n";
}

struct PackageInspection {
    PackageManifest manifest;
    PackageLock lock;
    std::string lock_state="missing";
    std::string detail;
    bool manifest_valid=false;
    bool graph_fully_resolved=false;
    bool all_cached=false;
    std::size_t direct_count=0;
    std::size_t transitive_count=0;
    std::vector<bool> cached;
};

PackageInspection inspect_packages(const std::filesystem::path& root){
    PackageInspection result;std::string error;
    result.manifest_valid=load_package_manifest_file(root/"strut.json",result.manifest,error);
    if(!result.manifest_valid){result.detail=error;return result;}
    const auto lock_path=root/"strut.lock.json";
    if(!std::filesystem::exists(lock_path))return result;
    if(!load_package_lock_file(lock_path,result.lock,error)){result.lock_state="corrupt";result.detail=error;return result;}
    if(!validate_package_lock(result.lock,&result.manifest,error)){result.lock_state="stale";result.detail=error;return result;}
    result.lock_state="locked";result.graph_fully_resolved=true;result.all_cached=true;
    for(const auto& package:result.lock.packages){
        if(package.direct)++result.direct_count;else ++result.transitive_count;
        const auto path=package_cache_root()/package.name/package.version/package.checksum.substr(7);std::string checksum,why;
        const bool present=verify_cached_package(path,checksum,why)&&checksum==package.checksum;
        result.cached.push_back(present);result.all_cached=result.all_cached&&present;
        if(!present&&result.detail.empty())result.detail=why;
    }
    return result;
}

int print_packages(bool as_json,std::ostream& out,std::ostream& err){
    const auto root=find_project_root(std::filesystem::current_path());const auto state=inspect_packages(root);
    if(as_json){
        json::Document d=json::Document::make_object();d["schema_version"]=1;d["command"]="packages";d["lock_state"]=state.lock_state;d["graph_fully_resolved"]=state.graph_fully_resolved;d["offline_available"]=state.graph_fully_resolved&&state.all_cached;
        if(!state.detail.empty())d["diagnostic"]=state.detail;
        json::Document packages=json::Document::make_array();
        for(std::size_t i=0;i<state.lock.packages.size();++i){const auto& package=state.lock.packages[i];const bool cached=i<state.cached.size()&&state.cached[i];json::Document item=json::Document::make_object();item["name"]=package.name;item["direct"]=package.direct;item["requested_constraint"]=package.requested;item["resolved_version"]=package.version;item["revision"]=package.revision;item["checksum"]=package.checksum;item["source_kind"]=package.source_kind;item["source_url"]=package.source;item["official"]=package.source_kind=="official";item["source_owner"]=package.source_kind=="official"?"strut-packages":"";item["source_repo"]=package.source_kind=="official"?package.name:"";item["canonical_source"]=package.source;item["lock_state"]="locked";item["cache_state"]=cached?"verified":"missing_or_corrupt";item["cached"]=cached;item["offline_available"]=cached;packages.push_back(item);}d["packages"]=packages;out<<d.dump(2)<<'\n';
    }else{
        out<<"lock state: "<<state.lock_state<<'\n';for(std::size_t i=0;i<state.lock.packages.size();++i){const auto& package=state.lock.packages[i];out<<package.name<<' '<<package.version<<" ("<<(package.direct?"direct":"transitive")<<", requested "<<package.requested<<", "<<package.source_kind<<", "<<(i<state.cached.size()&&state.cached[i]?"cached":"missing")<<")\n  revision "<<package.revision<<"\n  checksum "<<package.checksum<<"\n  source "<<package.source<<'\n';}
        out<<"offline available: "<<(state.graph_fully_resolved&&state.all_cached?"yes":"no")<<'\n';if(!state.detail.empty())err<<"strut: "<<state.detail<<'\n';
    }
    return state.lock_state=="locked"?0:1;
}

int print_project_info(bool as_json,std::ostream& out,std::ostream& err){
    const auto root=find_project_root(std::filesystem::current_path());const auto config=root/".strut"/"config.json";const auto manifest=root/"strut.json";const auto lock_path=root/"strut.lock.json";const auto cache=package_cache_root();const auto packages=inspect_packages(root);
    if(as_json){json::Document d=json::Document::make_object();d["schema_version"]=1;d["command"]="project";d["project_root"]=root.generic_string();d["build_config"]=config.generic_string();d["build_config_exists"]=std::filesystem::exists(config);d["manifest"]=manifest.generic_string();d["manifest_exists"]=std::filesystem::exists(manifest);d["package_cache"]=cache.generic_string();d["lockfile"]=lock_path.generic_string();d["lockfile_exists"]=std::filesystem::exists(lock_path);d["lockfile_schema_version"]=packages.lock_state=="missing"||packages.lock_state=="corrupt"?0:static_cast<int>(packages.lock.schema_version);d["lock_state"]=packages.lock_state;d["package_cache_state"]=packages.all_cached?"ready":"missing_or_corrupt";d["dependency_count"]=static_cast<int>(packages.lock.packages.size());d["direct_dependency_count"]=static_cast<int>(packages.direct_count);d["transitive_dependency_count"]=static_cast<int>(packages.transitive_count);std::size_t official=0;for(const auto& package:packages.lock.packages)if(package.source_kind=="official")++official;d["official_dependency_count"]=static_cast<int>(official);d["graph_fully_resolved"]=packages.graph_fully_resolved;d["all_locked_packages_cached"]=packages.all_cached;d["offline_available"]=packages.graph_fully_resolved&&packages.all_cached;if(!packages.detail.empty())d["package_diagnostic"]=packages.detail;out<<d.dump(2)<<'\n';return 0;}
    out<<"project root: "<<root.generic_string()<<'\n'<<"build config: "<<config.generic_string()<<(std::filesystem::exists(config)?"":" (missing)")<<'\n'<<"manifest: "<<manifest.generic_string()<<(std::filesystem::exists(manifest)?"":" (missing)")<<'\n'<<"package cache: "<<cache.generic_string()<<'\n';
    if(!std::filesystem::exists(config)&&!std::filesystem::exists(manifest)) err<<"help: run `strut init` to initialize this directory\n";
    return 0;
}

std::string rich_diagnostic(const std::filesystem::path& path, const Diagnostic& diagnostic, bool warning=false);

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
        err << rich_diagnostic(path, diagnostic) << '\n';
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

std::string rich_diagnostic(const std::filesystem::path& path, const Diagnostic& diagnostic, bool warning) {
    std::string load_error; auto source=SourceFile::load(path,load_error);
    if(!source) return warning?format_warning(path.string(),diagnostic):format_diagnostic(path.string(),diagnostic);
    return format_diagnostic_with_source(path.string(),source->text(),diagnostic,warning,true);
}

bool is_standard_module(std::string_view name) {
    const auto& modules=standard_modules();
    return std::find(modules.begin(),modules.end(),name)!=modules.end();
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
    for(const auto& d:lexed.diagnostics) err << rich_diagnostic(path,d) << '\n';
    if(!lexed.ok()){active.erase(key);return false;}
    Parser parser(lexed.tokens); auto parsed=parser.parse();
    for(const auto& d:parsed.diagnostics) err << rich_diagnostic(path,d) << '\n';
    if(!parsed.ok()){active.erase(key);return false;}
    std::vector<StmtPtr> own;
    for (auto& st : parsed.program.statements) {
        if (st->kind == Stmt::Kind::include_stmt) {
            if (st->include_is_package && is_standard_module(st->name)) {
                if (std::find(combined.standard_modules.begin(), combined.standard_modules.end(), st->name) == combined.standard_modules.end()) combined.standard_modules.push_back(st->name);
                continue;
            }
            if (st->include_is_package) {
                const auto slash = st->name.find('/'); const std::string package_name = st->name.substr(0, slash);
                PackageManifest project;PackageLock lock;std::string package_error;
                if (dependencies) { dependencies->push_back(project_root / "strut.json"); dependencies->push_back(project_root / "strut.lock.json"); }
                if (!load_package_manifest_file(project_root / "strut.json", project, package_error)) { err << path.string() << ": error: package dependency include <" << st->name << "> requires a project strut.json: " << package_error << "\nhelp: run `strut init`, then add the package dependency\n"; active.erase(key); return false; }
                if(!load_package_lock_file(project_root/"strut.lock.json",lock,package_error)||!validate_package_lock(lock,&project,package_error)){err<<path.string()<<": error: package graph is not locked: "<<package_error<<"\nhelp: run `strut install`\n";active.erase(key);return false;}
                auto locked=std::find_if(lock.packages.begin(),lock.packages.end(),[&](const LockedPackage& package){return package.name==package_name;});if(locked==lock.packages.end()){err<<path.string()<<": error: package '"<<package_name<<"' is not present in the locked dependency graph\nhelp: declare it directly or through a package dependency, then run `strut update`\n";active.erase(key);return false;}
                const auto exact_root=package_cache_root()/locked->name/locked->version/locked->checksum.substr(7);std::string actual;auto package_root=std::optional<std::filesystem::path>{};if(verify_cached_package(exact_root,actual,package_error)&&actual==locked->checksum)package_root=exact_root;if(!package_root){err<<path.string()<<": error: locked package dependency '"<<package_name<<"' "<<locked->version<<" is not present in the verified cache\nhelp: run `strut install`\n";active.erase(key);return false;}
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
    combined.enforce_standard_modules = true;
    return load_program_recursive(path, root, combined, loaded, active, err, dependencies);
}

int check_source(const std::filesystem::path& path, std::ostream& out, std::ostream& err) {
    (void)out; Program program; if(!load_program(path,program,err)) return 1;
    SemanticAnalyzer sema; auto checked=sema.analyze(program);
    for(const auto& d:checked.diagnostics) err<<rich_diagnostic(path,d)<<'\n';
    for(const auto& w:checked.warnings) err<<rich_diagnostic(path,w,true)<<'\n';
    return checked.ok()?0:1;
}

void collect_embed_dependencies(const std::filesystem::path& source_path,std::vector<std::filesystem::path>& dependencies){
    std::ifstream f(source_path);if(!f)return;std::ostringstream ss;ss<<f.rdbuf();const std::string text=ss.str();const std::regex pattern("(embed_file|embed_dir)\\s*\\(\\s*\"([^\"]+)\"");
    for(std::sregex_iterator it(text.begin(),text.end(),pattern),end;it!=end;++it){std::filesystem::path p=(*it)[2].str();if(p.is_relative())p=std::filesystem::current_path()/p;std::error_code ec;if(std::filesystem::is_directory(p,ec)){for(const auto&e:std::filesystem::recursive_directory_iterator(p,ec)){if(ec)break;if(e.is_regular_file())dependencies.push_back(std::filesystem::absolute(e.path()));}}else dependencies.push_back(std::filesystem::absolute(p));}
}

int compile_source(const std::filesystem::path& path, const std::filesystem::path& output, const NativeLinkOptions& link, std::ostream& out, std::ostream& err, bool verbose, bool timings=false, const std::filesystem::path& emit_cpp={}) {
    using clock = std::chrono::steady_clock;
    const auto total_begin=clock::now();
    const auto load_begin=clock::now();
    Program program; std::vector<std::filesystem::path> dependencies; if(!load_program(path,program,err,&dependencies)) return 1;
    const auto load_end=clock::now();
    for (const auto& dep : std::vector<std::filesystem::path>(dependencies)) collect_embed_dependencies(dep, dependencies);
    std::sort(dependencies.begin(), dependencies.end()); dependencies.erase(std::unique(dependencies.begin(), dependencies.end()), dependencies.end());
    const auto sema_begin=clock::now(); SemanticAnalyzer sema; auto checked=sema.analyze(program); const auto sema_end=clock::now();
    for(const auto& d:checked.diagnostics) err<<rich_diagnostic(path,d)<<'\n';
    for(const auto& w:checked.warnings) err<<rich_diagnostic(path,w,true)<<'\n';
    if(!checked.ok())return 1;
    const auto ir_begin=clock::now(); IRLowerer lowerer; auto lowered=lowerer.lower(program); const auto ir_end=clock::now(); if(!lowered.ok())return 1; lowered.program.source_path=std::filesystem::absolute(path).generic_string();
    CppBackend backend; std::string backend_error;
    auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
    if(!emit_cpp.empty()){const auto cg_begin=clock::now();auto generated=backend.generate(lowered.program);if(!generated.ok()){err<<path.string()<<": error: "<<generated.error<<'\n';return 1;}std::ofstream f(emit_cpp,std::ios::binary|std::ios::trunc);if(!f){err<<path.string()<<": error: cannot write generated C++ to "<<emit_cpp.string()<<'\n';return 1;}f<<generated.cpp;f.close();const auto done=clock::now();if(timings){out<<std::fixed<<std::setprecision(3)<<"timing load_parse_ms="<<ms(load_begin,load_end)<<'\n'<<"timing semantic_ms="<<ms(sema_begin,sema_end)<<'\n'<<"timing ir_ms="<<ms(ir_begin,ir_end)<<'\n'<<"timing codegen_write_ms="<<ms(cg_begin,done)<<'\n'<<"timing total_ms="<<ms(total_begin,done)<<'\n';}return 0;}
    const auto backend_begin=clock::now();
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
            if(!backend.link_objects(lowered.program,{object_with_ext},output,backend_error,link)){err<<path.string()<<": error: "<<backend_error<<'\n';return 1;}const auto done=clock::now();if(timings){out<<std::fixed<<std::setprecision(3)<<"timing load_parse_ms="<<ms(load_begin,load_end)<<'\n'<<"timing semantic_ms="<<ms(sema_begin,sema_end)<<'\n'<<"timing ir_ms="<<ms(ir_begin,ir_end)<<'\n'<<"timing native_backend_ms="<<ms(backend_begin,done)<<'\n'<<"timing total_ms="<<ms(total_begin,done)<<'\n';}return 0;
        }
    }
    if(!backend.compile(lowered.program,output,backend_error,link)){err<<path.string()<<": error: "<<backend_error<<'\n';return 1;}
    const auto done=clock::now();if(timings){out<<std::fixed<<std::setprecision(3)<<"timing load_parse_ms="<<ms(load_begin,load_end)<<'\n'<<"timing semantic_ms="<<ms(sema_begin,sema_end)<<'\n'<<"timing ir_ms="<<ms(ir_begin,ir_end)<<'\n'<<"timing native_backend_ms="<<ms(backend_begin,done)<<'\n'<<"timing total_ms="<<ms(total_begin,done)<<'\n';}
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
    link.target = config.target;
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
    if(command=="install"&&!argument.empty()&&argument!="--offline"){
        const auto separator=argument.rfind('@');const std::string name=separator==std::string::npos?argument:argument.substr(0,separator);const std::string requirement=separator==std::string::npos?"*":argument.substr(separator+1);
        PackageSource source;std::string resolved_version;if(!resolve_official_package_source(name,requirement,source,resolved_version,error)){err<<"strut: "<<error<<'\n';return 1;}
        project.dependencies[name]=requirement;project.dependency_sources[name]=source;
        if(!write_package_manifest_file(root/"strut.json",project,error)||!write_lockfile(root,project,error)){err<<"strut: "<<error<<'\n';return 1;}
        PackageLock lock;if(!install_packages(root,false,false,lock,error)){err<<"strut: "<<error<<'\n';return 1;}out<<"installed official package "<<name<<' '<<resolved_version<<" from "<<source.url<<'\n';return 0;
    }
    if(command=="install"||command=="update"){PackageLock lock;const bool offline=argument=="--offline";if(!install_packages(root,offline,command=="update",lock,error)){err<<"strut: "<<error<<'\n';return 1;}out<<(command=="update"?"updated ":"installed ")<<lock.packages.size()<<" locked package(s)"<<(offline?" offline":"")<<'\n';return 0;}
    if (command == "list") { for (const auto& dep : project.dependencies) out << dep.first << " " << dep.second << '\n'; return 0; }
    if (command == "add") { if (argument.empty()) { err << "strut: add requires a local package path\n"; return 2; } PackageManifest package; std::filesystem::path cached; if (!cache_local_package(argument,cached,package,error)) { err << "strut: " << error << '\n'; return 1; } project.dependencies[package.name]=package.version;project.dependency_sources[package.name]={"local-cache","local:"+package.name,package.version}; if(!write_package_manifest_file(root/"strut.json",project,error)||!write_lockfile(root,project,error)){err<<"strut: "<<error<<'\n';return 1;} out<<"added "<<package.name<<" "<<package.version<<'\n'; return 0; }
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
    bool timings = false;
    std::filesystem::path emit_cpp_path;
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
        if(command=="api"){
            bool as_json=false;std::string query;
            for(int i=2;i<argc;++i){const std::string_view arg(argv[i]);if(arg=="--json")as_json=true;else if(!arg.empty()&&arg.front()=='-'){err<<"strut: unsupported api option '"<<arg<<"'\n";return 2;}else if(query.empty())query=arg;else{err<<"strut: api accepts at most one query\n";return 2;}}
            if(as_json)out<<api_index(query).dump(2)<<'\n';else print_api_index(query,out);return 0;
        }
        if(command=="project"){
            if(argc>3||(argc==3&&std::string_view(argv[2])!="--json")){err<<"strut: project accepts only --json\n";return 2;}
            return print_project_info(argc==3,out,err);
        }
        if(command=="packages"){
            if(argc>3||(argc==3&&std::string_view(argv[2])!="--json")){err<<"strut: packages accepts only --json\n";return 2;}
            return print_packages(argc==3,out,err);
        }
        if (argc >= 3 && (std::string_view(argv[2]) == "--help" || std::string_view(argv[2]) == "-h") && command != "compile") {
            print_command_help(command, out); return 0;
        }
        if (command == "init") {
            if (argc != 2) { err << "strut: init takes no arguments\n"; return 2; }
            std::string init_error;
            if (!init_project_build_state(std::filesystem::current_path(), init_error)) { err << "strut: " << init_error << '\n'; return 1; }
            const auto manifest_path=std::filesystem::current_path()/"strut.json";
            if(!std::filesystem::exists(manifest_path)){
                PackageManifest manifest;manifest.name=std::filesystem::current_path().filename().string();
                std::transform(manifest.name.begin(),manifest.name.end(),manifest.name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
                for(char& c:manifest.name)if(!(std::isalnum(static_cast<unsigned char>(c))||c=='-'||c=='_'))c='-';
                if(!valid_package_name(manifest.name)) manifest.name="strut-project";
                manifest.version="0.1.0";
                BuildConfig config;std::string ignored;if(load_build_config(std::filesystem::current_path()/".strut/config.json",config,ignored))manifest.entry=config.entrypoint;
                if(!write_package_manifest_file(manifest_path,manifest,init_error)){err<<"strut: "<<init_error<<'\n';return 1;}
            }
            out << "created .strut/config.json and strut.json\n"; return 0;
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
        if (command == "add" || command == "remove" || command == "list" || command == "install" || command == "update") {
            const std::string argument = argc >= 3 ? argv[2] : std::string();
            if(command=="list"&&argc>2){err<<"strut: list takes no argument\n";return 2;}
            if(command=="install"&&argc>3){err<<"strut: install accepts one package requirement or --offline\n";return 2;}
            if(command=="update"&&argc>2){err<<"strut: update takes no argument\n";return 2;}
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
        if (arg == "--target") { if(i+1>=argc){err<<"strut: --target requires a target name\n";return 2;} link_options.target=argv[++i]; const std::unordered_set<std::string> valid={"native","linux-x64","linux-arm64","macos-arm64","macos-x64","windows-x64"}; if(!valid.count(link_options.target)){err<<"strut: unsupported target '"<<link_options.target<<"'\n";return 2;} continue; }
        if (arg == "--verbose") { verbose = true; continue; }
        if (arg == "--timings") { timings = true; continue; }
        if (arg == "--emit-cpp") { if(i+1>=argc){err<<"strut: --emit-cpp requires an output path\n";return 2;} emit_cpp_path=argv[++i]; continue; }
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
if (link_options.target == "windows-x64") output_path += ".exe";
#ifdef _WIN32
            else if (link_options.target == "native") output_path += ".exe";
#endif
        }
        return compile_source(source_path, output_path, link_options, out, err, verbose, timings, emit_cpp_path);
    }

    (void)explicit_compile;
    return 0;
}
}
