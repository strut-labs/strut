/* Native C library for the Strut->native fallible-callback direction. */
#include <stdint.h>
#include <stddef.h>
typedef struct { const char* type_data; size_t type_len; const char* message_data; size_t message_len; int32_t code; } cb_error;
typedef struct { int32_t code; const char* type; size_t type_len; const char* message; size_t message_len; } native_error;
typedef int32_t (*fcb)(void* ctx, int32_t a, int32_t* out_value, cb_error* out_error);
int32_t native_apply_fallible(fcb fn, void* ctx, int32_t value, int32_t* out_value, native_error* out_error){
    cb_error ce = { 0, 0, 0, 0, 0 };
    int32_t st = fn(ctx, value, out_value, &ce);
    if(st != 0){ out_error->code=ce.code; out_error->type=ce.type_data; out_error->type_len=ce.type_len; out_error->message=ce.message_data; out_error->message_len=ce.message_len; return st; }
    return 0;
}
