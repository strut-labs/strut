#include "export_lib.h"
#include "export_lib.h"
#include <stdio.h>
int main(void) {
    int32_t a = ff_add(20, 22);
    int64_t m = ff_mul(6, 7);
    float s = ff_scale(1.5f, 2.0f);
    double s64 = ff_scale64(1.5, 2.0);
    uint8_t b = ff_byte(200);
    int32_t z = ff_zero();
    ff_note(1);
    printf("%d %lld %.2f %.2f %u %d\n", (int)a, (long long)m, (double)s, s64, (unsigned)b, (int)z);
    return (a == 42 && m == 42 && s == 3.0f && s64 == 3.0 && b == 200 && z == 7) ? 0 : 1;
}
