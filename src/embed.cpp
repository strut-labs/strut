// Strut C embedding API (FFI-8): in-process compile + native-host invocation with no CLI
// spawn and no C++ types across the boundary. Logical module identity (never an absolute
// checkout path) drives the digest-qualified ABI names, consistent with FFI-1..7.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "strut/embed.h"
#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/sema.h"
#include "strut/ir.h"
#include "strut/codegen.h"
#include "strut/abi_type.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

struct embed_context {
    void* library = nullptr;
    std::filesystem::path tmp;
    std::string slug;
    std::string module_id;
    int (*invoke_fn)(const char*, const struct strut_embed_value*, std::size_t, struct strut_embed_value*, struct strut_embed_error*) = nullptr;
    void (*release_fn)(void*) = nullptr;
private:
    embed_context() = default;
public:
    static embed_context* create() { return new embed_context(); }
    void* owner_token() const { return release_fn ? (void*)release_fn : nullptr; }
    void release_buffer(void* p) { if (release_fn) release_fn(p); else std::free(p); }
};

std::string slug_of(const std::string& module_id) {
    return strut::abi_module_slug(module_id.empty() ? std::string("embed") : module_id);
}

void* load_library(const std::filesystem::path& lib, std::string& error_text) {
#if defined(_WIN32)
    HMODULE m = LoadLibraryW(lib.c_str());
    if (!m) return nullptr;
    return (void*)m;
#else
    void* h = dlopen(lib.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h) { error_text = dlerror() ? dlerror() : "dlopen failed"; return nullptr; }
    return h;
#endif
}

void* find_symbol(void* library, const std::string& name) {
#if defined(_WIN32)
    return (void*)GetProcAddress((HMODULE)library, name.c_str());
#else
    return dlsym(library, name.c_str());
#endif
}

void close_library(void* library) {
#if defined(_WIN32)
    if (library) FreeLibrary((HMODULE)library);
#else
    if (library) dlclose(library);
#endif
}

strut_embed_error* make_error(int category, std::string type, std::string message, int code) {
    auto* e = new strut_embed_error;
    e->category = category;
    e->owner = nullptr;
    e->type = nullptr;
    e->message = nullptr;
    e->code = code;
    if (!type.empty()) { e->type = static_cast<char*>(std::malloc(type.size() + 1)); std::memcpy(e->type, type.data(), type.size()); e->type[type.size()] = 0; }
    if (!message.empty()) { e->message = static_cast<char*>(std::malloc(message.size() + 1)); std::memcpy(e->message, message.data(), message.size()); e->message[message.size()] = 0; }
    return e;
}

// In-process front end + IR lowering from source text with a STABLE logical module id.
bool build_module(const std::string& module_id, const std::string& source,
                  strut::IRResult& lowered, std::vector<strut::Diagnostic>& out_errors,
                  int& out_category) {
    strut::SourceFile sf(module_id, source);
    strut::Lexer lexer(sf);
    auto lx = lexer.lex();
    strut::Parser parser(lx.tokens);
    auto parsed = parser.parse();
    if (!parsed.ok()) { out_errors = parsed.diagnostics; out_category = STRUT_EMBED_ERR_PARSE; return false; }
    strut::SemanticAnalyzer sema;
    auto checked = sema.analyze(parsed.program);
    if (!checked.ok()) { out_errors = checked.diagnostics; out_category = STRUT_EMBED_ERR_SEMANTIC; return false; }
    strut::IRLowerer lowerer;
    lowered = lowerer.lower(parsed.program);
    if (!lowered.ok()) { out_errors = lowered.diagnostics; out_category = STRUT_EMBED_ERR_LOAD; return false; }
    lowered.program.module_id = module_id;
    lowered.program.source_path = module_id;
    out_category = 0;
    return true;
}

std::string diagnostic_summary(const std::vector<strut::Diagnostic>& errors) {
    std::string out;
    for (const auto& d : errors) {
        if (!out.empty()) out += "\n";
        out += d.message;
    }
    return out;
}

} // namespace

extern "C" {

strut_embed_context* strut_embed_context_create(void) {
    auto* ctx = embed_context::create();
    std::error_code ec;
    ctx->tmp = std::filesystem::temp_directory_path();
    for (int i = 0; i < 100000; ++i) {
        auto candidate = ctx->tmp / ("strut-embed-" + std::to_string(std::rand()) + "-" + std::to_string(i));
        if (std::filesystem::create_directories(candidate, ec) && !ec) { ctx->tmp = candidate; break; }
    }
    if (!std::filesystem::is_directory(ctx->tmp)) { delete ctx; return nullptr; }
    return reinterpret_cast<strut_embed_context*>(ctx);
}

void strut_embed_context_destroy(strut_embed_context* c) {
    if (!c) return;
    embed_context* ctx = reinterpret_cast<embed_context*>(c);
    close_library(ctx->library);
    std::error_code ec;
    std::filesystem::remove_all(ctx->tmp, ec);
    delete ctx;
}

int strut_embed_context_load_source(strut_embed_context* c, const char* source, std::size_t len, strut_embed_error** out_err) {
    if (out_err) *out_err = nullptr;
    if (!c || !source) { if (out_err) *out_err = make_error(STRUT_EMBED_ERR_LOAD, "embed", "null context or source", 0); return 1; }
    embed_context* ctx = reinterpret_cast<embed_context*>(c);
    if (ctx->module_id.empty()) ctx->module_id = "embed";
    const std::string text(source, len);
    strut::IRResult lowered;
    std::vector<strut::Diagnostic> errors;
    int category = 0;
    if (!build_module(ctx->module_id, text, lowered, errors, category)) {
        if (out_err) *out_err = make_error(category, category == STRUT_EMBED_ERR_PARSE ? "parse" : (category == STRUT_EMBED_ERR_SEMANTIC ? "semantic" : "load"), diagnostic_summary(errors), 0);
        return 1;
    }
    std::string slug = slug_of(ctx->module_id);
    std::filesystem::path lib = ctx->tmp / ("libembed_" + slug + ".so");
    strut::NativeLinkOptions link;
    link.shared = true;
    link.release = true;
    std::string compile_error;
    strut::CppBackend backend;
    if (!backend.compile(lowered.program, lib, compile_error, link)) {
        if (out_err) *out_err = make_error(STRUT_EMBED_ERR_LOAD, "build", compile_error, 0);
        return 1;
    }
    std::string load_error;
    ctx->library = load_library(lib, load_error);
    if (!ctx->library) {
        if (out_err) *out_err = make_error(STRUT_EMBED_ERR_LOAD, "load", load_error, 0);
        return 1;
    }
    ctx->slug = slug;
    ctx->invoke_fn = reinterpret_cast<int (*)(const char*, const struct strut_embed_value*, std::size_t, struct strut_embed_value*, struct strut_embed_error*)>(
        find_symbol(ctx->library, "strut_embed_invoke_" + slug));
    ctx->release_fn = reinterpret_cast<void (*)(void*)>(find_symbol(ctx->library, "strut_embed_release_" + slug));
    if (!ctx->invoke_fn) {
        if (out_err) *out_err = make_error(STRUT_EMBED_ERR_LOAD, "build", "embedding dispatcher not emitted (no supported exported functions)", 0);
        return 1;
    }
    return 0;
}

int strut_embed_context_load_file(strut_embed_context* c, const char* path, strut_embed_error** out_err) {
    if (!path) return 1;
    std::error_code ec;
    std::ifstream in(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) { if (out_err) *out_err = make_error(STRUT_EMBED_ERR_LOAD, "load", "cannot read file", 0); return 1; }
    std::filesystem::path p(path);
    embed_context* ctx = reinterpret_cast<embed_context*>(c);
    ctx->module_id = p.filename().stem().string();
    return strut_embed_context_load_source(c, text.data(), text.size(), out_err);
}

int strut_embed_invoke(strut_embed_context* c, const char* name, const strut_embed_value* args, std::size_t nargs, strut_embed_value* out, strut_embed_error** out_err) {
    if (out_err) *out_err = nullptr;
    if (!c) return 1;
    embed_context* ctx = reinterpret_cast<embed_context*>(c);
    if (!ctx->invoke_fn) { if (out_err) *out_err = make_error(STRUT_EMBED_ERR_INVOKE, "embed", "no loaded module", 0); return 1; }
    *out = strut_embed_value{};
    strut_embed_error local;
    int status = ctx->invoke_fn(name, args, nargs, out, &local);
    if (status == 1) {
        if (out_err) {
            auto* h = make_error(local.category, local.type ? local.type : "", local.message ? local.message : "", local.code);
            h->owner = ctx->release_fn ? (void*)ctx->release_fn : nullptr;
            *out_err = h;
        }
        ctx->release_buffer(local.type);
        ctx->release_buffer(local.message);
        return 1;
    }
    if (status == 3) { if (out_err) *out_err = make_error(STRUT_EMBED_ERR_INVOKE, "embed", std::string("no such exported function '" + std::string(name) + "'").c_str(), 0); return 1; }
    if (status == 2) { if (out_err) *out_err = make_error(STRUT_EMBED_ERR_INVOKE, "embed", "argument count/kind mismatch", 0); return 1; }
    return 0;
}

void strut_embed_value_free(strut_embed_context* c, strut_embed_value* v) {
    if (!v) return;
    if (v->kind == STRUT_EMBED_VALUE_STRING && v->s.data) {
        if (c) reinterpret_cast<embed_context*>(c)->release_buffer(const_cast<char*>(v->s.data));
        else std::free(const_cast<char*>(v->s.data));
    }
    *v = strut_embed_value{};
}

void strut_embed_error_release(strut_embed_context* c, strut_embed_error* e) {
    if (!e) return;
    if (c && e->owner) {
        embed_context* ctx = reinterpret_cast<embed_context*>(c);
        if (e->type) ctx->release_buffer(e->type);
        if (e->message) ctx->release_buffer(e->message);
    } else {
        if (e->type) std::free(e->type);
        if (e->message) std::free(e->message);
    }
    delete e;
}

} // extern "C"