/* FFI-6: exported Strut checked errors -> C status + module-owned opaque error handle.
 * The header is pure C; the error is queried then released through this module's symbols. */
#include "export_error.h"
#include "export_error.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

int main(void) {
    int32_t v = 0; EXPORT_ERROR_FFI_ERROR* e = 0;
    CHECK(maybe_fail(5, &v, &e) == 0 && v == 10 && e == 0); /* success: no error */
    CHECK(maybe_fail(-3, &v, &e) != 0 && e != 0);           /* failure: structured error */
    const char* t = 0; size_t tl = 0; const char* m = 0; size_t ml = 0; int32_t c = 0;
    EXPORT_ERROR_FFI_ERROR_QUERY(e, &t, &tl, &m, &ml, &c);
    CHECK(tl == 9 && memcmp(t, "DemoError", 9) == 0);       /* concrete error identity */
    CHECK(ml == 8 && memcmp(m, "negative", 8) == 0);        /* message */
    CHECK(c == 7);                                          /* code */
    EXPORT_ERROR_FFI_ERROR_RELEASE(e);

    EXPORT_ERROR_FFI_ERROR* e2 = 0;                          /* void checked-error function */
    CHECK(do_it(1, &e2) == 0 && e2 == 0);
    CHECK(do_it(-1, &e2) != 0 && e2 != 0);
    EXPORT_ERROR_FFI_ERROR_RELEASE(e2);

    int i; for (i = 0; i < 10000; ++i) {                     /* failure/release stress */ 
        EXPORT_ERROR_FFI_ERROR* r = 0;
        CHECK(maybe_fail(-1, &v, &r) != 0 && r != 0);
        EXPORT_ERROR_FFI_ERROR_RELEASE(r);
    }
    int k; for (k = 1; k <= 2; ++k) {                    /* multiple declared errors: concrete identity */
        EXPORT_ERROR_FFI_ERROR* pe = 0; int32_t pv = 0;
        CHECK(pick(k, &pv, &pe) != 0 && pe != 0);
        const char* pt = 0; size_t ptl = 0; const char* pm = 0; size_t pml = 0; int32_t pc = 0;
        EXPORT_ERROR_FFI_ERROR_QUERY(pe, &pt, &ptl, &pm, &pml, &pc);
        if (k == 1) CHECK(ptl == 9 && memcmp(pt, "DemoError", 9) == 0 && pc == 1);
        else CHECK(ptl == 10 && memcmp(pt, "OtherError", 10) == 0 && pc == 2);
        EXPORT_ERROR_FFI_ERROR_RELEASE(pe);
    }
    printf("err ok\n");
    return 0;
}
