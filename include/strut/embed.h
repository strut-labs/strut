/* Strut C embedding API (FFI-8). An opaque, C-only surface for a native host to compile and
 * drive Strut source in-process (no CLI spawn; no C++ types across the boundary). This is an
 * EMBEDDING-layer value format distinct from the generated typed C FFI (FFI-1..7). */
#ifndef STRUT_FFI_EMBED_H
#define STRUT_FFI_EMBED_H
#include <stdint.h>
#include <stddef.h>
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
    STRUT_EMBED_VALUE_BYTES = 5
};
typedef struct strut_embed_value {
    int kind;
    int64_t i;
    double d;
    int b;
    struct { const char* data; size_t len; } s;
} strut_embed_value;

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

strut_embed_context* strut_embed_context_create(void);
void strut_embed_context_destroy(strut_embed_context* ctx);

int strut_embed_context_load_source(strut_embed_context* ctx, const char* source, size_t len, strut_embed_error** out_err);
int strut_embed_context_load_file(strut_embed_context* ctx, const char* path, strut_embed_error** out_err);

int strut_embed_invoke(strut_embed_context* ctx, const char* name, const strut_embed_value* args, size_t nargs, strut_embed_value* out, strut_embed_error** out_err);

/* Release a value returned by strut_embed_invoke. Module-owned string/bytes payloads are
 * freed through the module token captured at load; primitives/empty/void are no-ops. The value
 * is zeroed, so an accidental second release is a no-op.
 * Release a host- or module-owned error: routing is internal via the error's ownership tag, so
 * callers need not know which allocation domain produced it. ctx may be NULL for host-owned
 * errors; module-owned error payloads MUST be released before strut_embed_context_destroy. */
void strut_embed_value_free(strut_embed_context* ctx, strut_embed_value* v);
void strut_embed_error_release(strut_embed_context* ctx, strut_embed_error* e);

#ifdef __cplusplus
}
#endif
#endif