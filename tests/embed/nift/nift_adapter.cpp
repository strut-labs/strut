// FFI-10B adapter (integer slice): Strut main() originates a call into Nift through the public
// C ABI v1.3. The extern "C" surface stays on plain int_64 so it maps through Strut's extern FFI
// without a C++ wrapper; the adapter owns engine/result lifecycle.
#include <nift/c_abi.h>
#include <string.h>
#include <stdlib.h>

extern "C" int64_t strut_nift_eval(int64_t seed) {
    static const char* expression = "40 + 2";
    int64_t expected = 42;
    (void)seed;
    nift_engine* engine = nift_engine_new();
    if (!engine) return -100;
    nift_script_result* result = nullptr;
    if (nift_engine_evaluate(engine, expression, strlen(expression), &result) != NIFT_OK || !result) {
        nift_engine_free(engine);
        return -101;
    }
    int64_t value = -102;
    if (nift_script_result_ok(result)) {
        nift_string json{};
        if (nift_script_result_value_json(result, &json) == NIFT_OK && json.data && json.length) {
            char buffer[64] = {0};
            size_t n = json.length < sizeof(buffer) - 1 ? json.length : sizeof(buffer) - 1;
            memcpy(buffer, json.data, n);
            value = strtoll(buffer, nullptr, 10);
        }
    }
    nift_script_result_free(result);
    nift_engine_free(engine);
    return value == expected ? value : -103;
}
