/* Strut C embedding API (FFI-8). An opaque, C-only surface for a native host to compile and
 * drive Strut source in-process (no CLI spawn; no C++ types across the boundary). This is an
 * EMBEDDING-layer value format distinct from the generated typed C FFI (FFI-1..7). */
#ifndef STRUT_FFI_EMBED_H
#define STRUT_FFI_EMBED_H
#include <stdint.h>
#include <stddef.h>
#ifndef STRUT_EMBED_API
/* Export the public embedding symbols only when a Windows DLL is being built; definitions in
 * src/embed.cpp inherit this linkage from the declaration. Static builds and non-Windows
 * toolchains keep STRUT_EMBED_API empty; consumers may override it to __declspec(dllimport). */
#if defined(_WIN32) && defined(STRUT_EMBED_BUILD_SHARED)
#define STRUT_EMBED_API __declspec(dllexport)
#else
#define STRUT_EMBED_API
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct strut_embed_context strut_embed_context;

enum {
    STRUT_EMBED_VALUE_VOID = 0,
    STRUT_EMBED_VALUE_BOOL = 1,
    STRUT_EMBED_VALUE_INT = 2,
    STRUT_EMBED_VALUE_FLOAT = 3,
    STRUT_EMBED_VALUE_STRING = 4,
    STRUT_EMBED_VALUE_BYTES = 5,
    STRUT_EMBED_VALUE_RETAINED = 6,
    STRUT_EMBED_VALUE_CALLBACK = 7
};
/* Typed borrowed-callback function pointer for the SUPPORTED embedding callback signature
 * (int_32 -> int_32). No function pointer is converted through an object pointer; the native
 * context is the separate cb_ctx member. Other callback signatures are not embedding-callable
 * and produce an unsupported-signature diagnostic. */
typedef int32_t (*strut_embed_callback_i32_fn)(void* ctx, int32_t arg);

typedef struct strut_embed_value {
    int kind;
    int64_t i;
    double d;
    int b;
    struct { const char* data; size_t len; } s;
    void* retained;   /* RETAINED: module-owned opaque retained-callback handle (refcount 1) */
    strut_embed_callback_i32_fn cb_fn;   /* CALLBACK: borrowed native callback function (int_32 -> int_32), valid
                                   only for the enclosing embedding invocation */
    void* cb_ctx;      /* CALLBACK: borrowed opaque native context (first callback argument) */
} strut_embed_value;

enum {
    /* For category STRUT_EMBED_ERR_INVOKE, `code` distinguishes the failure subclass so
     * bindings can branch on stable values, not messages. Checked Strut errors keep code = the
     * Strut error code and set type/message. */
    STRUT_EMBED_INVOKE_ARITY = 1,
    STRUT_EMBED_INVOKE_KIND = 2,
    STRUT_EMBED_INVOKE_NOTFOUND = 3,
    STRUT_EMBED_INVOKE_UNSUPPORTED = 4
};
enum {
    STRUT_EMBED_ERR_PARSE = 1,
    STRUT_EMBED_ERR_SEMANTIC = 2,
    STRUT_EMBED_ERR_LOAD = 3,
    STRUT_EMBED_ERR_INVOKE = 4,
    STRUT_EMBED_ERR_INTERNAL = 5
};
typedef struct strut_embed_error {
    int category;
    void* owner;   /* module token that allocated type/message; null = host-allocated */
    char* type;
    char* message;
    int code;
} strut_embed_error;
/* Typed borrowed-callback function pointer for the SUPPORTED embedding callback signature
 * (int_32 -> int_32). No function pointer is converted through an object pointer; the native
 * context is the separate cb_ctx member. Other callback signatures are not embedding-callable
 * and produce an unsupported-signature diagnostic. */
typedef int32_t (*strut_embed_callback_i32_fn)(void* ctx, int32_t arg);

STRUT_EMBED_API strut_embed_context* strut_embed_context_create(void);
/* Returns 0 on success; nonzero (BUSY) if outstanding RETAINED values still hold module
 * leases -- release them first. BUSY leaves the context fully intact and usable. A module with
 * outstanding leases stays loaded (module lease) so retained callbacks remain safely invokable
 * after successful module replacement, and is unloaded exactly once after the last release. */
STRUT_EMBED_API int strut_embed_context_destroy(strut_embed_context* ctx);

STRUT_EMBED_API int strut_embed_context_load_source(strut_embed_context* ctx, const char* source, size_t len, strut_embed_error** out_err);
STRUT_EMBED_API int strut_embed_context_load_file(strut_embed_context* ctx, const char* path, strut_embed_error** out_err);

STRUT_EMBED_API int strut_embed_invoke(strut_embed_context* ctx, const char* name, const strut_embed_value* args, size_t nargs, strut_embed_value* out, strut_embed_error** out_err);

/* Release a value returned by strut_embed_invoke. Module-owned string/bytes payloads are
 * freed through the module token captured at load; primitives/empty/void are no-ops. The value
 * is zeroed, so an accidental second release is a no-op.
 * Release a host- or module-owned error: routing is internal via the error's ownership tag, so
 * callers need not know which allocation domain produced it. ctx may be NULL for host-owned
 * errors; module-owned error payloads MUST be released before strut_embed_context_destroy. */
STRUT_EMBED_API void strut_embed_value_free(strut_embed_context* ctx, strut_embed_value* v);
STRUT_EMBED_API void strut_embed_error_release(strut_embed_context* ctx, strut_embed_error* e);

/* Retained callback interop (FFI-7 contract preserved: the caller owns a live reference; in
 * this layer that means a non-NULL RETAINED value from invoke, released exactly once through
 * strut_embed_value_free. RETAINED values are single-owner: copying them is prohibited. Invoke
 * routes to the callback's OWNING module, so it stays correct after that module was replaced. */
STRUT_EMBED_API int strut_embed_retained_invoke(strut_embed_context* ctx, const strut_embed_value* self,
                                const strut_embed_value* args, size_t nargs,
                                strut_embed_value* out, strut_embed_error** out_err);

#ifdef __cplusplus
}
#endif
#endif