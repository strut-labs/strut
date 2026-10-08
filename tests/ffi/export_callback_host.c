/* FFI-5: synchronous borrowed C callbacks (function pointer + context) passed by an
 * independent host to an exported Strut function. The generated header is pure C. */
#include "export_callback.h"
#include "export_callback.h"
#include <stdio.h>
#include <stdint.h>

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

static int32_t twice(void* ctx, int32_t x) { (void)ctx; return x * 2; }
static int32_t plus(void* ctx, int32_t x) { return x + *(int32_t*)ctx; }

int main(void) {
    CHECK(apply_twice(twice, 0, 10) == 40);           /* repeated synchronous invocation */
    int32_t base = 100;
    CHECK(apply_ctx(plus, &base, 10) == 221);          /* context pointer round-trips */
    printf("cb ok\n");
    return 0;
}
