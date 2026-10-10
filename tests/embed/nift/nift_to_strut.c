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

    char script[1024];
    int n = snprintf(script, sizeof script,
        "ffi_call(ffi_open(\"%s\"), \"add\", \"i32(i32,i32)\", 20, 22)\n", libbase);
    if (n < 0 || (size_t)n >= sizeof script) { fprintf(stderr, "script too large\n"); return 2; }

    nift_engine* engine = nift_engine_new();
    if (!engine) { fprintf(stderr, "engine null\n"); return 2; }
    nift_script_result* result = NULL;
    nift_status st = nift_engine_evaluate(engine, script, (size_t)n, &result);
    if (st != NIFT_OK || !result) { fprintf(stderr, "evaluate status %d\n", (int)st); nift_engine_free(engine); return 1; }

    int ok = nift_script_result_ok(result);
    if (!ok) {
        nift_string msg = {0};
        nift_script_result_error_message(result, &msg);
        fprintf(stderr, "nift script failed: %.*s\n", (int)msg.length, msg.data ? msg.data : "");
        nift_script_result_free(result);
        nift_engine_free(engine);
        return 1;
    }
    nift_string json = {0};
    nift_status vj = nift_script_result_value_json(result, &json);
    int matches = (vj == NIFT_OK && json.length == 2 && memcmp(json.data, "42", 2) == 0);
    nift_script_result_free(result);
    nift_engine_free(engine);
    if (!matches) { fprintf(stderr, "nift->strut result mismatch (%.*s)\n", (int)json.length, json.data ? json.data : ""); return 1; }
    printf("nift strut ok\n");
    return 0;
}