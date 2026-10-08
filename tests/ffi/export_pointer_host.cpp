/* FFI-4: borrowed primitive raw_ptr<T> (nullable) and ref<T> (non-null) across the C ABI.
 * The generated header is pure C (`int32_t*`), no C++ reference or shared_ptr. */
#include "export_pointer.h"
#include "export_pointer.h"
#include <stdio.h>
#include <stdint.h>

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

int main(void) {
    int32_t x = 41;
    pt_inc(&x);
    CHECK(x == 42); /* raw_ptr mutation */

    int32_t y = 10;
    rf_inc(&y);
    CHECK(y == 11); /* ref (non-null) mutation */

    int32_t z = 7;
    CHECK(pt_read(&z) == 7); /* read through raw_ptr */

    CHECK(pt_is_null(0) == 1);       /* raw_ptr is nullable */
    CHECK(pt_is_null(&z) == 0);

    printf("ptr ok\n");
    return 0;
}
