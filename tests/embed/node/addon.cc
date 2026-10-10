// FFI-9 Node N-API addon: a thin, dependency-free bridge exposing the Strut embedding ABI to
// JavaScript. Values are marshalled by inference (number->int, string->string, Buffer->bytes,
// function->borrowed callback); results are copied into JS-owned storage before the native
// value is released; errors translate to thrown Error objects carrying category/code/type.
#include <node_api.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "strut/embed.h"

namespace {

struct CbCtx {
    napi_env env;
    napi_ref ref;
    CbCtx(napi_env e, napi_ref r) : env(e), ref(r) {}
};

static void setString(napi_env e, napi_value o, const char* k, const char* v) {
    napi_value s;
    napi_create_string_utf8(e, v, NAPI_AUTO_LENGTH, &s);
    napi_set_named_property(e, o, k, s);
}
static void setInt64(napi_env e, napi_value o, const char* k, int64_t v) {
    napi_value s;
    napi_create_int64(e, v, &s);
    napi_set_named_property(e, o, k, s);
}
static void setAddr(napi_env e, napi_value o, const char* k, uint64_t v) {
    napi_value s;
    napi_create_double(e, static_cast<double>(v), &s);
    napi_set_named_property(e, o, k, s);
}

// Synchronous borrowed-callback trampoline. Runs on the caller's (JS owning) thread because the
// callback is only invoked from inside the native invocation; a NUL value routes through
// napi_call_function on this env, then yields the int_32 result.
static int32_t strut_node_tramp(void* ctx, int32_t x) {
    CbCtx* c = static_cast<CbCtx*>(ctx);
    napi_value global, fn, arg, out;
    if (napi_get_global(c->env, &global) != napi_ok) return -777;
    if (napi_get_reference_value(c->env, c->ref, &fn) != napi_ok) return -777;
    if (napi_create_int32(c->env, x, &arg) != napi_ok) return -777;
    if (napi_call_function(c->env, global, fn, 1, &arg, &out) != napi_ok) return -777;
    int32_t r;
    if (napi_get_value_int32(c->env, out, &r) != napi_ok) return -777;
    return r;
}

static void throwEmbedError(napi_env env, strut_embed_error* e) {
    napi_value err;
    int cat = e ? e->category : 0, code = e ? e->code : 0;
    const char* ty = (e && e->type) ? e->type : "";
    const char* msg = (e && e->message) ? e->message : "embedding error";
    napi_create_object(env, &err);
    setInt64(env, err, "category", cat);
    setInt64(env, err, "code", code);
    setString(env, err, "msg", msg);
    setString(env, err, "type", ty);
    napi_throw(env, err);
    strut_embed_error_release(nullptr, e);
}

static strut_embed_context* ctxOf(napi_env env, napi_value v) {
    double d;
    napi_get_value_double(env, v, &d);
    return reinterpret_cast<strut_embed_context*>(static_cast<uint64_t>(d));
}

// Marshal one JS argument into a value. Borrowed-callback descriptors are heap-allocated and
// intentionally outlive this call; the corpus exercises no borrowed callback escape paths.
bool marshal(napi_env env, napi_value arg, strut_embed_value* v, std::string* scratch) {
    napi_valuetype t;
    napi_typeof(env, arg, &t);
    if (t == napi_number) {
        double d;
        napi_get_value_double(env, arg, &d);
        v->kind = STRUT_EMBED_VALUE_INT;
        v->i = static_cast<int64_t>(d);
        return true;
    }
    if (t == napi_string) {
        size_t n;
        napi_get_value_string_utf8(env, arg, nullptr, 0, &n);
        scratch->assign(n, '\0');
        napi_get_value_string_utf8(env, arg, &(*scratch)[0], n + 1, &n);
        v->kind = STRUT_EMBED_VALUE_STRING;
        v->s.data = scratch->c_str();
        v->s.len = n;
        return true;
    }
    if (t == napi_object) {
        bool isbuf = false, ista = false;
        napi_is_buffer(env, arg, &isbuf);
        napi_is_typedarray(env, arg, &ista);
        if (isbuf || ista) {
            void* data;
            size_t len;
            if (napi_get_buffer_info(env, arg, &data, &len) == napi_ok) {
                v->kind = STRUT_EMBED_VALUE_BYTES;
                v->s.data = static_cast<const char*>(data);
                v->s.len = len;
                return true;
            }
        }
    }
    if (t == napi_function) {
        napi_ref ref;
        napi_create_reference(env, arg, 1, &ref);
        CbCtx* c = new CbCtx(env, ref);
        v->kind = STRUT_EMBED_VALUE_CALLBACK;
        v->cb_fn = strut_node_tramp;
        v->cb_ctx = reinterpret_cast<void*>(c);
        return true;
    }
    napi_throw_type_error(env, nullptr, "unsupported argument type");
    return false;
}

napi_value StrutOpen(napi_env env, napi_callback_info) {
    strut_embed_context* ctx = strut_embed_context_create();
    napi_value r;
    napi_create_double(env, static_cast<double>(reinterpret_cast<uint64_t>(ctx)), &r);
    return r;
}

napi_value StrutClose(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value a[1];
    napi_get_cb_info(env, info, &argc, a, nullptr, nullptr);
    int rc = strut_embed_context_destroy(ctxOf(env, a[0]));
    napi_value r;
    napi_create_int32(env, rc, &r);
    return r;
}

napi_value StrutLoad(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value a[2];
    napi_get_cb_info(env, info, &argc, a, nullptr, nullptr);
    size_t n;
    napi_get_value_string_utf8(env, a[1], nullptr, 0, &n);
    std::string src(n, '\0');
    napi_get_value_string_utf8(env, a[1], &src[0], n + 1, &n);
    strut_embed_error* e = nullptr;
    int rc = strut_embed_context_load_source(ctxOf(env, a[0]), src.c_str(), src.size(), &e);
    napi_value r;
    napi_create_int32(env, rc, &r);
    if (rc != 0) throwEmbedError(env, e);
    return r;
}

napi_value StrutInvoke(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value a[3];
    napi_get_cb_info(env, info, &argc, a, nullptr, nullptr);
    char name[64];
    size_t nn;
    napi_get_value_string_utf8(env, a[1], name, sizeof name, &nn);
    uint32_t na = 0;
    napi_get_array_length(env, a[2], &na);
    if (na > 8) na = 8;
    strut_embed_value vals[8];
    memset(vals, 0, sizeof vals);
    std::string scratch[8];
    strut_embed_context* ctx = ctxOf(env, a[0]);
    for (uint32_t i = 0; i < na; ++i) {
        napi_value el;
        napi_get_element(env, a[2], i, &el);
        if (!marshal(env, el, &vals[i], &scratch[i])) return nullptr;
    }
    strut_embed_value out{};
    strut_embed_error* e = nullptr;
    int rc = strut_embed_invoke(ctx, name, vals, na, &out, &e);
    napi_value r;
    if (rc != 0) {
        throwEmbedError(env, e);
        return nullptr;
    }
    if (out.kind == STRUT_EMBED_VALUE_INT) {
        napi_create_int64(env, out.i, &r);
        return r;
    }
    if (out.kind == STRUT_EMBED_VALUE_STRING || out.kind == STRUT_EMBED_VALUE_BYTES) {
        void* buf;
        napi_create_buffer(env, out.s.len, &buf, &r);
        if (out.s.len) memcpy(buf, out.s.data, out.s.len);
        // Copy is JS-owned; release the native value now.
        strut_embed_value_free(ctx, &out);
        return r;
    }
    if (out.kind == STRUT_EMBED_VALUE_RETAINED) {
        napi_value obj;
        napi_create_object(env, &obj);
        setAddr(env, obj, "retained", reinterpret_cast<uint64_t>(out.retained));
        return obj;
    }
    napi_get_undefined(env, &r);
    return r;
}

napi_value StrutRetained(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value a[3];
    napi_get_cb_info(env, info, &argc, a, nullptr, nullptr);
    strut_embed_value self{};
    double h;
    napi_get_value_double(env, a[1], &h);
    self.kind = STRUT_EMBED_VALUE_RETAINED;
    self.retained = reinterpret_cast<void*>(static_cast<uint64_t>(h));
    uint32_t na = 0;
    napi_get_array_length(env, a[2], &na);
    if (na > 8) na = 8;
    strut_embed_value vals[8];
    memset(vals, 0, sizeof vals);
    std::string scratch[8];
    strut_embed_context* ctx = ctxOf(env, a[0]);
    for (uint32_t i = 0; i < na; ++i) {
        napi_value el;
        napi_get_element(env, a[2], i, &el);
        if (!marshal(env, el, &vals[i], &scratch[i])) return nullptr;
    }
    strut_embed_value out{};
    strut_embed_error* e = nullptr;
    int rc = strut_embed_retained_invoke(ctx, &self, vals, na, &out, &e);
    napi_value r;
    if (rc != 0) {
        throwEmbedError(env, e);
        return nullptr;
    }
    napi_create_int64(env, out.i, &r);
    return r;
}

napi_value StrutFree(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value a[2];
    napi_get_cb_info(env, info, &argc, a, nullptr, nullptr);
    double h;
    napi_get_value_double(env, a[1], &h);
    strut_embed_value v{};
    v.kind = STRUT_EMBED_VALUE_RETAINED;
    v.retained = reinterpret_cast<void*>(static_cast<uint64_t>(h));
    strut_embed_value_free(ctxOf(env, a[0]), &v);
    return nullptr;
}

}  // namespace

#define DECL(fn) \
    { #fn, nullptr, fn, nullptr, nullptr, nullptr, napi_default, nullptr }

extern "C" {
NAPI_MODULE_INIT() {
    napi_property_descriptor props[] = {
        DECL(StrutOpen), DECL(StrutClose), DECL(StrutLoad), DECL(StrutInvoke),
        DECL(StrutRetained), DECL(StrutFree),
    };
    napi_define_properties(env, exports, sizeof props / sizeof props[0], props);
    return exports;
}
}