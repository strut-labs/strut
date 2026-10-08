#include "export_lib.h"
#include <stdio.h>
int main(void) {
    int32_t a = ff_add(20, 22);
    int64_t m = ff_mul(6, 7);
    double s = ff_scale(1.5, 2.0);
    int32_t n = ff_negate(1);
    printf("%d %lld %.1f %d\n", (int)a, (long long)m, s, (int)n);
    return (a == 42 && m == 42 && s == 3.0 && n == 1) ? 0 : 1;
}
