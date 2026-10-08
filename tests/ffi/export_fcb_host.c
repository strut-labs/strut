/* FFI-6: synchronous fallible callback (native -> exported Strut). A C callback reports a
 * structured failure; the Strut export catches it and propagates it through the ordinary
 * opaque error handle. Two error boundaries, no C++ exception across C. */
#include "export_fcb.h"
#include "export_fcb.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

static int32_t mycb(void* ctx, int32_t a, int32_t* out_value, EXPORT_FCB_CALLBACK_ERROR* out_error) {
    (void)ctx;
    if (a < 0) {
        out_error->type_data = "CallbackError"; out_error->type_len = 13;
        out_error->message_data = "bad callback"; out_error->message_len = 12;
        out_error->code = 17;
        return 1;
    }
    *out_value = a * 3;
    return 0;
}

int main(void) {
    int32_t v = 0; EXPORT_FCB_FFI_ERROR* e = 0;
    CHECK(call_cb(mycb, 0, 7, &v, &e) == 0 && v == 21 && e == 0);   /* callback success */
    CHECK(call_cb(mycb, 0, -1, &v, &e) != 0 && e != 0);             /* callback failure -> host error */
    const char* t = 0; size_t tl = 0; const char* m = 0; size_t ml = 0; int32_t c = 0;
    EXPORT_FCB_FFI_ERROR_QUERY(e, &t, &tl, &m, &ml, &c);
    CHECK(tl == 13 && memcmp(t, "CallbackError", 13) == 0);
    CHECK(ml == 12 && memcmp(m, "bad callback", 12) == 0);
    CHECK(c == 17);
    EXPORT_FCB_FFI_ERROR_RELEASE(e);
    int i; for (i = 0; i < 10000; ++i) {                            /* fallible-callback stress */
        EXPORT_FCB_FFI_ERROR* r = 0;
        CHECK(call_cb(mycb, 0, -1, &v, &r) != 0 && r != 0);
        EXPORT_FCB_FFI_ERROR_RELEASE(r);
    }
    printf("fcb ok\n");
    return 0;
}
