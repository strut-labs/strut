// FFI-10A: Nift-origin call into a Strut-exported C function. The Nift SCRIPT (evaluated through
// libnift_c's engine) performs ffi_open/ffi_call/ffi_close itself; the only C here is driving the
// engine and reading its result. The Strut invocation therefore originates from Nift source.
#include <nift/c_abi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc != 2) { fprintf(stderr, "usage: nift_to_strut <libbase>\n"); return 2; }
    const char* libbase = argv[1];

    static const int pairs[][2] = {{20, 22}, {10, 5}, {-3, 1}, {0, 0}};
    static const int expected[] = {42, 15, -2, 0};
    nift_engine* engine = nift_engine_new();
    if (!engine) { fprintf(stderr, "engine null\n"); return 2; }
    for (int i = 0; i < 4; ++i) {
        char script[256];
        int n = snprintf(script, sizeof script,
            "ffi_call(ffi_open(\"%s\"), \"add\", \"i32(i32,i32)\", %d, %d)\n",
            libbase, pairs[i][0], pairs[i][1]);
        if (n < 0 || (size_t)n >= sizeof script) { fprintf(stderr, "script too large\n"); nift_engine_free(engine); return 2; }
        nift_script_result* result = NULL;
        nift_status st = nift_engine_evaluate(engine, script, (size_t)n, &result);
        if (st != NIFT_OK || !result) { fprintf(stderr, "evaluate %d status %d\n", i, (int)st); nift_engine_free(engine); return 1; }
        int okv = nift_script_result_ok(result);
        if (!okv) {
            nift_string msg = {0};
            nift_script_result_error_message(result, &msg);
            fprintf(stderr, "nift script %d failed: %.*s\n", i, (int)msg.length, msg.data ? msg.data : "");
            nift_script_result_free(result);
            nift_engine_free(engine);
            return 1;
        }
        nift_string json = {0};
        char want[32] = {0};
        snprintf(want, sizeof want, "%d", expected[i]);
        nift_status vj = nift_script_result_value_json(result, &json);
        int matches = (vj == NIFT_OK && json.length == strlen(want) && memcmp(json.data, want, strlen(want)) == 0);
        if (!matches) { fprintf(stderr, "nift->strut pair %d result mismatch (%.*s want %s)\n",
                                i, (int)json.length, json.data ? json.data : "", want);
                         nift_script_result_free(result); nift_engine_free(engine); return 1; }
        nift_script_result_free(result);
    }
    // Nift-origin binary into a Strut-exported function: the payload bytes are constructed in
    // the Nift script (ffi_buffer) and cross into the Strut export nift_verify_bytes which checks
    // .length()==5 and every byte (embedded NUL + high bit). Empty payload must return -1.
    {
        char script[320];
        int n = snprintf(script, sizeof script,
            "ffi_call(ffi_open(\"%s\"), \"nift_verify_bytes\", \"i32(buffer,u64)\", ffi_buffer([97, 0, 98, 255, 128]), 5)",
            libbase);
        nift_script_result* result = NULL;
        if (nift_engine_evaluate(engine, script, (size_t)n, &result) != NIFT_OK || !result || !nift_script_result_ok(result)) {
            fprintf(stderr, "nift->strut bytes evaluate failed\n");
            nift_engine_free(engine); return 1;
        }
        nift_string json = {0};
        int okb = (nift_script_result_value_json(result, &json) == NIFT_OK && json.length == 2 &&
                   memcmp(json.data, "42", 2) == 0);
        nift_script_result_free(result);
        n = snprintf(script, sizeof script,
            "ffi_call(ffi_open(\"%s\"), \"nift_verify_bytes\", \"i32(buffer,u64)\", ffi_buffer([]), 0)",
            libbase);
        nift_script_result* r2 = NULL;
        if (nift_engine_evaluate(engine, script, (size_t)n, &r2) != NIFT_OK || !r2 || !nift_script_result_ok(r2)) {
            fprintf(stderr, "nift->strut empty bytes evaluate failed\n");
            nift_engine_free(engine); return 1;
        }
        nift_string j2 = {0};
        int okv = (nift_script_result_value_json(r2, &j2) == NIFT_OK && j2.length == 2 &&
                   memcmp(j2.data, "-1", 2) == 0);
        nift_script_result_free(r2);
        if (!okb || !okv) {
            fprintf(stderr, "nift->strut bytes verify mismatch okb=%d okv=%d\n", okb, okv);
            nift_engine_free(engine); return 1;
        }
    }
    // Nift-origin UTF-8 into a Strut export: real UTF-8 bytes in the script, byte length passed
    // explicitly. "h<C3 A9>llo " is 6 chars / 7 bytes; ASCII "hello" is 5 bytes.
    {
        struct Case { const char* text; int len; };
        static const struct Case cases[] = {
            {"\"h\xC3\xA9llo \"", 7},
            {"\"hello\"", 5},
        };
        int all_ok = 1;
        for (int i = 0; i < 2; ++i) {
            char script[320];
            int n = snprintf(script, sizeof script,
                "ffi_call(ffi_open(\"%s\"), \"nift_verify_string\", \"i32(cstr,u64)\", %s, %d)",
                libbase, cases[i].text, cases[i].len);
            nift_script_result* result = NULL;
            if (nift_engine_evaluate(engine, script, (size_t)n, &result) != NIFT_OK || !result) {
                all_ok = 0; break;
            }
            if (!nift_script_result_ok(result)) { nift_script_result_free(result); all_ok = 0; break; }
            nift_string json = {0};
            int okv = (nift_script_result_value_json(result, &json) == NIFT_OK && json.length == 2 &&
                       memcmp(json.data, "42", 2) == 0);
            nift_script_result_free(result);
            if (!okv) { fprintf(stderr, "nift->strut string case %d mismatch\n", i); all_ok = 0; break; }
        }
        if (!all_ok) { nift_engine_free(engine); return 1; }
    }
    nift_engine_free(engine);
    printf("nift strut ok\n");
    return 0;
}