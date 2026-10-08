/* Two independently generated Strut shared libraries in ONE host.
 * Proves the module-qualified release symbols do not collide: each library's buffer is
 * released through that library's own <module>_ffi_free_* symbol. */
#include "export_lib_a.h"
#include "export_lib_b.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(void) {
    char *a = 0, *b = 0; size_t na = 0, nb = 0;
    fa_make(&a, &na);
    fb_make(&b, &nb);
    if (!(na == 5 && a && memcmp(a, "alpha", 5) == 0)) { printf("multilib FAIL a\n"); return 1; }
    if (!(nb == 4 && b && memcmp(b, "beta", 4) == 0)) { printf("multilib FAIL b\n"); return 1; }
    EXPORT_LIB_A_FFI_FREE_STRING(a);
    EXPORT_LIB_B_FFI_FREE_STRING(b);

    uint8_t *ab = 0, *bb = 0; size_t an = 0, bn = 0;
    fa_bytes(&ab, &an);
    fb_bytes(&bb, &bn);
    if (!(an == 1 && ab && ab[0] == 'A')) { printf("multilib FAIL ab\n"); return 1; }
    if (!(bn == 1 && bb && bb[0] == 'B')) { printf("multilib FAIL bb\n"); return 1; }
    EXPORT_LIB_A_FFI_FREE_BYTES(ab);
    EXPORT_LIB_B_FFI_FREE_BYTES(bb);

    printf("multilib ok\n");
    return 0;
}
