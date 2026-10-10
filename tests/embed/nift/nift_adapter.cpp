// FFI-10B adapter (integer slice): Strut main() originates a call into Nift through the public
// C ABI v1.3. The extern "C" surface stays on plain int_64 so it maps through Strut's extern FFI
// without a C++ wrapper; the adapter owns engine/result lifecycle.
#include <nift/c_abi.h>
#include <string.h>
#include <stdlib.h>

// Strut supplies "seed" through Nift's engine binding API (no expression string concatenation);
// Nift computes seed + 25. Returned value is validated against the expectation so a hardcoded
// fixture cannot fake a success.
static int64_t nift_add25(int64_t seed, int64_t expected) {
    nift_engine* engine = nift_engine_new();
    if (!engine) return -100;
    nift_engine_set_int(engine, "seed", 4, static_cast<int32_t>(seed));
    static const char* expression = "seed + 25";
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

extern "C" int64_t strut_nift_eval(int64_t seed) {
    return nift_add25(seed, seed + 25);
}
extern "C" int64_t strut_nift_add25_seventeen(void) {
    return nift_add25(17, 42);
}
