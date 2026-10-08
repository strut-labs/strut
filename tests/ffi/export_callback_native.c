/* Native C library for the Strut->native callback direction. */
#include <stdint.h>
typedef int32_t (*cb)(void*, int32_t);
int32_t native_apply(cb fn, void* ctx, int32_t value) { return fn(ctx, value); }
