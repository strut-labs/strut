// Strut C embedding API (FFI-8): in-process compile + native-host invocation with no CLI
// spawn and no C++ types across the boundary. Logical module identity (never an absolute
// checkout path) drives the digest-qualified ABI names, consistent with FFI-1..7.
//
// Module lifetime: every loaded module is a `module_ref`. The context's ACTIVE module is the
// one dispatched to by invoke(). Ordinary results/errors are copied into embedding-owned
// storage, so they never depend on a module. RETAINED values hold a lease on their owning
// module: a replaced (inactive) module is NOT unloaded while leases remain, and is unloaded
// exactly once after the last lease is released. context_destroy is BUSY while leases exist.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
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

void close_library(void*);

struct embed_module {
    void* library = nullptr;
    void (*release_fn)(void*) = nullptr;
    int (*retained_invoke_fn)(void*, const strut_embed_value*, std::size_t, strut_embed_value*, strut_embed_error*) = nullptr;
    void (*retained_release_fn)(void*) = nullptr;
    int leases = 0;    // outstanding retained handles from this module
    bool active = false;
};

struct embed_context {
    embed_module* current = nullptr;
    std::vector<embed_module*> modules;                            // all loaded (active + leased inactive)
    std::unordered_map<const void*, embed_module*> retained_owner; // retained handle -> owning module
    std::filesystem::path tmp;
    std::string module_id;
    int (*invoke_fn)(const char*, const strut_embed_value*, std::size_t, strut_embed_value*, strut_embed_error*) = nullptr;
    unsigned long load_seq = 0;
    int in_flight_ = 0;

    embed_context() = default;
    embed_context(const embed_context&) = delete;
    embed_context& operator=(const embed_context&) = delete;
    ~embed_context() = default;

    void unload(embed_module* m) {
        close_library(m->library);
        for (auto it = modules.begin(); it != modules.end(); ++it) {
            if (*it == m) { modules.erase(it); break; }
        }
        delete m;
    }
};

std::string slug_of(const std::string& module_id) {
    return strut::abi_module_slug(module_id.empty() ? std::string("embed") : module_id);
}

void* load_library(const std::filesystem::path& lib, std::string& error_text) {
    (void)error_text;
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
    return library ? dlsym(library, name.c_str()) : nullptr;
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

struct inflight_guard { int* p; inflight_guard(int* q):p(q){++*p;} ~inflight_guard(){--*p;} };

// Copy a module-owned buffer into EMBEDDING-library-owned storage; release the module copy
// NOW while the module is guaranteed loaded.
char* adopt_buffer(const char* data, std::size_t len, void (*release_fn)(void*)) {
    char* copy = nullptr;
    if (len) { copy = static_cast<char*>(std::malloc(len + 1)); if (data && len) std::memcpy(copy, data, len); copy[len] = 0; }
    if (data) { if (release_fn) release_fn(const_cast<char*>(data)); else std::free(const_cast<char*>(data)); }
    return copy;
}

} // namespace

extern "C" {

strut_embed_context* strut_embed_context_create(void) {
    auto* ctx = new embed_context;
    std::error_code ec;
    ctx->tmp = std::filesystem::temp_directory_path();
    for (int i = 0; i < 100000; ++i) {
        auto candidate = ctx->tmp / ("strut-embed-" + std::to_string(std::rand()) + "-" + std::to_string(i));
        if (std::filesystem::create_directories(candidate, ec) && !ec) { ctx->tmp = candidate; break; }
    }
    if (!std::filesystem::is_directory(ctx->tmp)) { delete ctx; return nullptr; }
    return reinterpret_cast<strut_embed_context*>(ctx);
}

int strut_embed_context_destroy(strut_embed_context* c) {
    if (!c) return 1;
    embed_context* ctx = reinterpret_cast<embed_context*>(c);
    if (ctx->in_flight_ > 0) return 1;           // BUSY: an invocation is in flight on this context
    if (!ctx->retained_owner.empty()) return 1;   // BUSY: outstanding retained leases; context intact
    for (auto* m : ctx->modules) close_library(m->library);   // active + lease-free inactive
    for (auto* m : ctx->modules) delete m;
    ctx->modules.clear();
    ctx->retained_owner.clear();
    std::error_code ec;
    std::filesystem::remove_all(ctx->tmp, ec);
    delete ctx;
    return 0;
}

int strut_embed_context_load_source(strut_embed_context* c, const char* source, std::size_t len, strut_embed_error** out_err) {
    if (out_err) *out_err = nullptr;
    if (!c || !source) { if (out_err) *out_err = make_error(STRUT_EMBED_ERR_LOAD, "embed", "null context or source", 0); return 1; }
    embed_context* ctx = reinterpret_cast<embed_context*>(c);
    if (ctx->in_flight_ > 0) { if (out_err) *out_err = make_error(STRUT_EMBED_ERR_LOAD, "embed", "cannot reload this context while an invocation is in flight on it", 0); return 1; }
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
    std::filesystem::path lib = ctx->tmp / ("libembed_" + slug + "_" + std::to_string(ctx->load_seq++) + ".so");
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
    void* new_library = load_library(lib, load_error);
    if (!new_library) {
        if (out_err) *out_err = make_error(STRUT_EMBED_ERR_LOAD, "load", load_error, 0);
        return 1;
    }
    auto new_invoke = reinterpret_cast<int (*)(const char*, const struct strut_embed_value*, std::size_t, struct strut_embed_value*, struct strut_embed_error*)>(
        find_symbol(new_library, "strut_embed_invoke_" + slug));
    if (!new_invoke) {
        close_library(new_library);
        if (out_err) *out_err = make_error(STRUT_EMBED_ERR_LOAD, "build", "embedding dispatcher not emitted (no supported exported functions)", 0);
        return 1;
    }
    auto* m = new embed_module();
    m->library = new_library;
    m->release_fn = reinterpret_cast<void (*)(void*)>(find_symbol(new_library, "strut_embed_release_" + slug));
    m->retained_invoke_fn = reinterpret_cast<int (*)(void*, const struct strut_embed_value*, std::size_t, struct strut_embed_value*, struct strut_embed_error*)>(
        find_symbol(new_library, "strut_embed_retained_invoke_" + slug));
    m->retained_release_fn = reinterpret_cast<void (*)(void*)>(find_symbol(new_library, "strut_embed_retained_release_" + slug));
    m->active = true;
    ctx->modules.push_back(m);
    // Successful load atomically replaces the active module. The previous active module is
    // unloaded NOW only if it has no outstanding retained leases; otherwise it stays loaded
    // (retained callbacks keep invoking its code) and unloads with its final lease.
    if (ctx->current) {
        ctx->current->active = false;
        if (ctx->current->leases == 0) ctx->unload(ctx->current);
    }
    ctx->current = m;
    ctx->invoke_fn = new_invoke;
    return 0;
}

int strut_embed_context_load_file(strut_embed_context* c, const char* path, strut_embed_error** out_err) {
    if (!path) return 1;
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
    inflight_guard strut_ig(&ctx->in_flight_);
    *out = strut_embed_value{};
    strut_embed_error local;
    int status = ctx->invoke_fn(name, args, nargs, out, &local);
    if (status == 0) {
        if (out->kind == STRUT_EMBED_VALUE_STRING || out->kind == STRUT_EMBED_VALUE_BYTES) {
            const std::size_t n = out->s.len;
            char* copy = adopt_buffer(out->s.data, n, ctx->current ? ctx->current->release_fn : nullptr);
            out->s.data = copy;
            out->s.len = n;
        } else if (out->kind == STRUT_EMBED_VALUE_RETAINED && out->retained) {
            // The host now owns one FFI-7 reference (refcount 1) PLUS one module lease.
            if (ctx->current) ctx->current->leases++;
            ctx->retained_owner[out->retained] = ctx->current;
        }
        return 0;
    }
    if (status == 1) {
        if (out_err) {
            std::string ty = local.type ? local.type : "";
            std::string msg = local.message ? local.message : "";
            if (local.type && ctx->current) ctx->current->release_fn(local.type); else if (local.type) std::free(local.type);
            if (local.message && ctx->current) ctx->current->release_fn(local.message); else if (local.message) std::free(local.message);
            *out_err = make_error(local.category, ty, msg, local.code);
        } else {
            if (local.type && ctx->current) ctx->current->release_fn(local.type); else if (local.type) std::free(local.type);
            if (local.message && ctx->current) ctx->current->release_fn(local.message); else if (local.message) std::free(local.message);
        }
        return 1;
    }
    int err_code = 0; std::string err_msg;
    switch (status) {
        case 2: err_code = STRUT_EMBED_INVOKE_ARITY; err_msg = "wrong argument count"; break;
        case 5: err_code = STRUT_EMBED_INVOKE_KIND; err_msg = "wrong argument kind for '" + std::string(name) + "'"; break;
        case 3: err_code = STRUT_EMBED_INVOKE_NOTFOUND; err_msg = "no such exported function '" + std::string(name) + "'"; break;
        case 4: err_code = STRUT_EMBED_INVOKE_UNSUPPORTED; err_msg = "function '" + std::string(name) + "' exists but its signature is not embedding-callable"; break;
        default: break;
    }
    if (err_code != 0) { if (out_err) *out_err = make_error(STRUT_EMBED_ERR_INVOKE, "embed", err_msg, err_code); return 1; }
    return 0;
}

int strut_embed_retained_invoke(strut_embed_context* c, const strut_embed_value* self, const strut_embed_value* args, std::size_t nargs, strut_embed_value* out, strut_embed_error** out_err) {
    if (out_err) *out_err = nullptr;
    if (!c || !self || self->kind != STRUT_EMBED_VALUE_RETAINED || !self->retained) { if (out_err) *out_err = make_error(STRUT_EMBED_ERR_INVOKE, "embed", "invalid retained callback reference", 0); return 1; }
    embed_context* ctx = reinterpret_cast<embed_context*>(c);
    inflight_guard strut_ig(&ctx->in_flight_);
    auto it = ctx->retained_owner.find(self->retained);
    if (it == ctx->retained_owner.end() || !it->second->retained_invoke_fn) {
        if (out_err) *out_err = make_error(STRUT_EMBED_ERR_INVOKE, "embed", "live retained callback reference required", 0);
        return 1;
    }
    embed_module* m = it->second;
    *out = strut_embed_value{};
    strut_embed_error local;
    int status = m->retained_invoke_fn(self->retained, args, nargs, out, &local);
    if (status == 1) {
        if (out_err) {
            std::string ty = local.type ? local.type : "";
            std::string msg = local.message ? local.message : "";
            if (local.type && m->release_fn) m->release_fn(local.type); else if (local.type) std::free(local.type);
            if (local.message && m->release_fn) m->release_fn(local.message); else if (local.message) std::free(local.message);
            *out_err = make_error(local.category, ty, msg, local.code);
        } else {
            if (local.type && m->release_fn) m->release_fn(local.type); else if (local.type) std::free(local.type);
            if (local.message && m->release_fn) m->release_fn(local.message); else if (local.message) std::free(local.message);
        }
        return 1;
    }
    int err_code = 0; std::string err_msg;
    switch (status) {
        case 2: err_code = STRUT_EMBED_INVOKE_ARITY; err_msg = "wrong argument count"; break;
        case 5: err_code = STRUT_EMBED_INVOKE_KIND; err_msg = "wrong argument kind"; break;
        default: break;
    }
    if (err_code != 0) { if (out_err) *out_err = make_error(STRUT_EMBED_ERR_INVOKE, "embed", err_msg, err_code); return 1; }
    return 0;
}

void strut_embed_value_free(strut_embed_context* c, strut_embed_value* v) {
    if (!v) return;
    if (v->kind == STRUT_EMBED_VALUE_RETAINED && v->retained) {
        if (c) {
            embed_context* ctx = reinterpret_cast<embed_context*>(c);
            auto it = ctx->retained_owner.find(v->retained);
            if (it != ctx->retained_owner.end()) {
                embed_module* m = it->second;
                // Destruction order: resolve module -> FFI-7 release (module still loaded due to
                // its lease) -> drop record -> drop lease -> unload inactive module if final.
                if (m->retained_release_fn) m->retained_release_fn(v->retained);
                else if (m->release_fn) m->release_fn(nullptr);
                ctx->retained_owner.erase(it);
                if (--m->leases == 0 && !m->active) ctx->unload(m);
            }
        }
        *v = strut_embed_value{};
        return;
    }
    if ((v->kind == STRUT_EMBED_VALUE_STRING || v->kind == STRUT_EMBED_VALUE_BYTES) && v->s.data) {
        std::free(const_cast<char*>(v->s.data));   /* embedding-owned copy */
    }
    *v = strut_embed_value{};
}

void strut_embed_error_release(strut_embed_context* c, strut_embed_error* e) {
    (void)c;
    if (!e) return;
    if (e->type) std::free(e->type);      /* embedding-owned copy */
    if (e->message) std::free(e->message);
    delete e;
}

} // extern "C"