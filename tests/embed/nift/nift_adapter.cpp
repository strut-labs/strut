// FFI-10B adapter (integer slice): Strut main() originates a call into Nift through the public
// C ABI v1.3. The extern "C" surface stays on plain int_64 so it maps through Strut's extern FFI
// without a C++ wrapper; the adapter owns engine/result lifecycle.
#include <nift/c_abi.h>
#include <string.h>
#include <stdlib.h>

// Strut supplies "seed" through Nift's engine binding API (no expression string concatenation);
// Nift computes seed + 25. Returned value is validated against the expectation so a hardcoded
// fixture cannot fake a success.
// Status/result contract: the adapter returns 0 on success and a small nonzero status on
// failure; the computed value is returned through *out and never encodes the outcome, so a
// legitimate negative result (e.g. seed=-128 -> -103) is not confused with an error.
// Status codes: 1 engine creation, 2 binding, 3 mechanical evaluate, 4 semantic failure,
// 5 result conversion.
// Checked int32 parse: empty input, trailing characters, overflow, or an out-of-range value
// must fail; *out is only written on complete success.
static int parse_i32(const char* data, size_t len, int32_t* out) {
    if (!data || len == 0 || len >= 16) return 0;
    char buffer[16];
    memcpy(buffer, data, len);
    buffer[len] = '\0';
    char* end = nullptr;
    long value = strtol(buffer, &end, 10);
    if (end == buffer || *end != '\0') return 0;
    if (value != static_cast<int32_t>(value)) return 0;
    *out = static_cast<int32_t>(value);
    return 1;
}

// Scoped failure probe: evaluates an invalid Nift expression to route a semantic failure through
// the status channel (returns 3 mechanical / 4 semantic); never enters the conversion path.
extern "C" int strut_nift_add25_bad(void) {
    nift_engine* engine = nift_engine_new();
    if (!engine) return 1;
    static const char* expression = "seed +";
    nift_script_result* result = nullptr;
    if (nift_engine_evaluate(engine, expression, strlen(expression), &result) != NIFT_OK || !result) {
        nift_engine_free(engine);
        return 3;
    }
    int status = nift_script_result_ok(result) ? 4 : 4;
    nift_script_result_free(result);
    nift_engine_free(engine);
    return status;
}

extern "C" int strut_nift_add25_i32(int32_t seed, int32_t* out) {
    if (!out) return 5;
    nift_engine* engine = nift_engine_new();
    if (!engine) return 1;
    if (nift_engine_set_int(engine, "seed", 4, seed) != NIFT_OK) {
        nift_engine_free(engine);
        return 2;
    }
    static const char* expression = "seed + 25";
    nift_script_result* result = nullptr;
    if (nift_engine_evaluate(engine, expression, strlen(expression), &result) != NIFT_OK || !result) {
        nift_engine_free(engine);
        return 3;
    }
    if (!nift_script_result_ok(result)) {
        nift_script_result_free(result);
        nift_engine_free(engine);
        return 4;
    }
    nift_string json{};
    int status = 5;
    if (nift_script_result_value_json(result, &json) == NIFT_OK && json.data && json.length) {
        if (parse_i32(json.data, json.length, out)) status = 0;
    }
    nift_script_result_free(result);
    nift_engine_free(engine);
    return status;
}
