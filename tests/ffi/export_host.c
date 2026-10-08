#include "export_lib.h"
#include "export_lib.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

int main(void) {
    /* ---- FFI-1 primitives ---- */
    int32_t a = ff_add(20, 22);
    int64_t m = ff_mul(6, 7);
    float s = ff_scale(1.5f, 2.0f);
    double s64 = ff_scale64(1.5, 2.0);
    uint8_t b = ff_byte(200);
    int32_t z = ff_zero();
    ff_note(1);
    printf("%d %lld %.2f %.2f %u %d\n", (int)a, (long long)m, (double)s, s64, (unsigned)b, (int)z);
    CHECK(a == 42 && m == 42 && s == 3.0f && s64 == 3.0 && b == 200 && z == 7);

    /* ---- FFI-2 string input is borrowed (const) + output is owned/explicit ---- */
    {
        char* out = (char*)0x1; size_t n = 999;
        ff_str_echo("hello", 5, &out, &n);
        CHECK(n == 5 && out != NULL && memcmp(out, "hello", 5) == 0);
        EXPORT_LIB_FFI_FREE_STRING(out);
    }
    {
        /* transform: concatenation */
        char* out = 0; size_t n = 0;
        ff_str_dup("ab", 2, &out, &n);
        CHECK(n == 4 && out != NULL && memcmp(out, "abab", 4) == 0);
        EXPORT_LIB_FFI_FREE_STRING(out);
    }
    {
        /* embedded NUL is preserved; length is BYTES (not NUL-terminated, not code points) */
        const char raw[3] = { 'a', '\0', 'b' };
        char* out = 0; size_t n = 0;
        ff_str_echo(raw, 3, &out, &n);
        CHECK(n == 3 && memcmp(out, raw, 3) == 0);
        EXPORT_LIB_FFI_FREE_STRING(out);
    }
    {
        /* non-ASCII UTF-8: length is byte count */
        const char* utf8 = "caf\xc3\xa9"; /* café => 5 bytes */
        CHECK(ff_str_len(utf8, 5) == 5);
        char* out = 0; size_t n = 0;
        ff_str_echo(utf8, 5, &out, &n);
        CHECK(n == 5 && memcmp(out, utf8, 5) == 0);
        EXPORT_LIB_FFI_FREE_STRING(out);
    }
    {
        /* empty string: canonical (data==NULL, len==0) both in and out */
        char* out = (char*)0x1; size_t n = 999;
        ff_str_echo(NULL, 0, &out, &n);
        CHECK(n == 0 && out == NULL);
        EXPORT_LIB_FFI_FREE_STRING(out); /* free(NULL) must be safe */
    }
    {
        /* input lifetime: Strut must not retain the host buffer past the call */
        char mut[5] = { 'h', 'e', 'l', 'l', 'o' };
        char* out = 0; size_t n = 0;
        ff_str_echo(mut, 5, &out, &n);
        mut[0] = 'X'; mut[1] = 'Y'; /* mutate host buffer after the call returns */
        CHECK(n == 5 && memcmp(out, "hello", 5) == 0);
        EXPORT_LIB_FFI_FREE_STRING(out);
    }
    {
        /* owned output survives the call + release; stress ownership (no leak/double-free) */
        int i;
        for (i = 0; i < 20000; ++i) {
            char* out = 0; size_t n = 0;
            ff_str_echo("stress", 6, &out, &n);
            if (!(n == 6 && out != NULL && memcmp(out, "stress", 6) == 0)) { printf("FAIL stress line %d\n", __LINE__); return 1; }
            EXPORT_LIB_FFI_FREE_STRING(out);
        }
    }

    /* ---- FFI-2 bytes: truly binary, embedded NUL, full 0x00..0xff ---- */
    {
        const uint8_t bin[6] = { 0x00, 0x01, 0x7f, 0x80, 0xff, 0x00 };
        uint8_t* out = (uint8_t*)0x1; size_t n = 999;
        ff_bytes_echo(bin, 6, &out, &n);
        CHECK(n == 6 && out != NULL && memcmp(out, bin, 6) == 0);
        EXPORT_LIB_FFI_FREE_BYTES(out);
    }
    {
        /* single byte 0xff */
        const uint8_t one[1] = { 0xff };
        uint8_t* out = 0; size_t n = 0;
        ff_bytes_echo(one, 1, &out, &n);
        CHECK(n == 1 && out != NULL && out[0] == 0xff);
        EXPORT_LIB_FFI_FREE_BYTES(out);
    }
    {
        /* empty bytes: canonical (NULL, 0) */
        uint8_t* out = (uint8_t*)0x1; size_t n = 999;
        ff_bytes_echo(NULL, 0, &out, &n);
        CHECK(n == 0 && out == NULL);
        EXPORT_LIB_FFI_FREE_BYTES(out);
    }
    {
        /* bytes output lifetime stress */
        int i;
        for (i = 0; i < 20000; ++i) {
            const uint8_t bin[3] = { 0x00, 0x80, 0xff };
            uint8_t* out = 0; size_t n = 0;
            ff_bytes_echo(bin, 3, &out, &n);
            if (!(n == 3 && out != NULL && memcmp(out, bin, 3) == 0)) { printf("FAIL bytes stress line %d\n", __LINE__); return 1; }
            EXPORT_LIB_FFI_FREE_BYTES(out);
        }
    }

    printf("ffi2 ok\n");
    return 0;
}
