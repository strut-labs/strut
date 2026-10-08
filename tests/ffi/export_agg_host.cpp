/* FFI-3: plain POD aggregate round-trip + independent C layout certification.
 * The generated header's typedef is the ABI authority; the host must not duplicate it. */
#include "export_agg.h"
#include "export_agg.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

int main(void) {
    /* layout certification from the C compiler's own view */
    CHECK(sizeof(EXPORT_AGG_Pair) == 8);
    CHECK(offsetof(EXPORT_AGG_Pair, a) == 0);
    CHECK(offsetof(EXPORT_AGG_Pair, b) == 4);
    CHECK(sizeof(EXPORT_AGG_Mixed) == 24);
    CHECK(offsetof(EXPORT_AGG_Mixed, a) == 0);
    CHECK(offsetof(EXPORT_AGG_Mixed, b) == 8);
    CHECK(offsetof(EXPORT_AGG_Mixed, c) == 16);

    /* struct as input */
    EXPORT_AGG_Pair p = { 20, 22 };
    CHECK(agg_sum(p) == 42);

    /* struct as return */
    EXPORT_AGG_Pair q = agg_make(3, 4);
    CHECK(q.a == 3 && q.b == 4);

    /* mixed primitive fields: struct input + struct return */
    EXPORT_AGG_Mixed m = { 7, 2.5, 200 };
    CHECK(agg_mixed_weight(m) == 2.5);
    EXPORT_AGG_Mixed r = agg_mixed_make(1, 3.5, 9);
    CHECK(r.a == 1 && r.b == 3.5 && r.c == 9);

    /* internal Strut call to an exported function must hit the private impl, not the wrapper */
    CHECK(agg_double_sum(p) == 84);

    printf("agg ok\n");
    return 0;
}
