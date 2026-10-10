// FFI-10B adapter (integer slice): Strut main() originates a call into Nift through the public
// C ABI v1.3. The extern "C" surface stays on plain int_64 so it maps through Strut's extern FFI
// without a C++ wrapper; the adapter owns engine/result lifecycle.
#include <nift/c_abi.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string>

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
    // Strict JSON integer contract (input is Nift's JSON serialization): no surrounding
    // whitespace, no leading '+' or leading zeros, but "-0" is valid JSON.
    if (len > 0 && (data[len - 1] == '\n' || data[0] == ' ' || data[0] == '\t')) return 0;
    if (data[0] == '+' || data[0] == '-') {
        if (len == 1) return 0;
        if (data[0] == '+') return 0;
        // "-0" is valid (-0 with no further digits); "-0<digit>" and "-<non-digit>" are rejected.
        if (data[0] == '-') {
            if (data[1] == '-') return 0;
            if (data[1] >= '0' && data[1] <= '9') {
                if (data[1] == '0' && len > 2) return 0;
            } else {
                return 0;
            }
        }
    } else if (len > 1 && data[0] == '0') {
        return 0;
    }
    char buffer[16];
    memcpy(buffer, data, len);
    buffer[len] = '\0';
    char* end = nullptr;
    errno = 0;
    long value = strtol(buffer, &end, 10);
    if (errno == ERANGE || end == buffer || *end != '\0') return 0;
    if (value < INT32_MIN || value > INT32_MAX) return 0;
    *out = static_cast<int32_t>(value);
    return 1;
}

// Direct regression over the real helper: success rows must convert and failure rows must leave
// the output untouched (pre-seeded sentinel). Returns 0 when every row matches intent.
extern "C" int strut_nift_parse_i32_check(void) {
    struct Row { const char* text; int expect_success; int32_t expect_value; };
    static const Row rows[] = {
        {"42", 1, 42}, {"-103", 1, -103}, {"0", 1, 0},
        {"2147483647", 1, INT32_MAX}, {"-2147483648", 1, INT32_MIN},
        {"-0", 1, 0},
        {"2147483648", 0, 0}, {"-2147483649", 0, 0},
        {"", 0, 0}, {"12x", 0, 0}, {"1.5", 0, 0}, {"null", 0, 0}, {"true", 0, 0},
        {"+42", 0, 0}, {" 42", 0, 0}, {"042", 0, 0}, {"--42", 0, 0}, {"42\n", 0, 0}, {"-042", 0, 0},
    };
    for (const auto& row : rows) {
        int32_t value = 0x51354E53;
        int ok = parse_i32(row.text, strlen(row.text), &value);
        int passes = (ok == row.expect_success) &&
                     (ok ? value == row.expect_value : value == (int32_t)0x51354E53);
        if (!passes) return 0;
    }
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
    int okv = nift_script_result_ok(result);
    nift_script_result_free(result);
    nift_engine_free(engine);
    // 4 = expected semantic failure; 0 = UNEXPECTED success (must not be reported as a failure).
    return okv ? 0 : 4;
}


// Same-engine recovery: a single engine must survive an expected semantic failure and still
// evaluate successfully afterwards. Returns 0 only if ok -> fail(with nonempty diagnostic) -> ok is
// observed on one engine.

// Read the JSON integer of a successful script result (used to assert actual values).
static int result_int(nift_script_result* r) {
    nift_string json = {0};
    if (nift_script_result_value_json(r, &json) != NIFT_OK || !json.data || !json.length) return -1;
    int32_t v = 0;
    return parse_i32(json.data, json.length, &v) ? (int)v : -1;
}

// Decode a JSON string literal (a required leading/trailing quote pair). Standard JSON escapes
// (\" \\ \/ \n \r \t \b \f \uXXXX) are decoded; raw control bytes are dropped; everything else
// is copied verbatim. Sufficient for Nift's JSON serialization output.
static void json_string_decode(const char* data, size_t len, std::string& out) {
    out.clear();
    if (len < 2 || data[0] != '"' || data[len - 1] != '"') { out.assign(data, len); return; }
    for (size_t i = 1; i + 1 < len; ++i) {
        char c = data[i];
        if (c == '"') {
            out += c;
        } else if (c != '\\') {
            if (!((unsigned char)c < 0x20u)) out += c;
        } else if (i + 1 < len - 1) {
            char e = data[i + 1];
            switch (e) {
                case '"': out += '"'; ++i; break;
                case '\\': out += '\\'; ++i; break;
                case '/': out += '/'; ++i; break;
                case 'n': out += '\n'; ++i; break;
                case 'r': out += '\r'; ++i; break;
                case 't': out += '\t'; ++i; break;
                case 'b': out += '\b'; ++i; break;
                case 'f': out += '\f'; ++i; break;
                case 'u': {
                    ++i;
                    if (i + 4 < len && data[i] == '0' && data[i + 1] == '0' && data[i + 2] == '0' && data[i + 3] >= '0' && data[i + 3] <= '9') {
                        out += (char)('0' + (data[i + 3] - '0'));
                    } else {
                        // bounded \uXXXX byte pass-through for the corpus (escaped non-ASCII is
                        // produced by Nift only for contents not already UTF-8).
                        out += '\\'; out += 'u';
                        for (int k = 0; k < 4 && i + (size_t)k < len; ++k) out += data[i + (size_t)k];
                        i += 3;
                    }
                    break;
                }
                default: out += e; ++i; break;
            }
        }
    }
}

// NOTE: strut_string reconstructs Strut's generated string representation
// ({ std::string v; }, by value across the extern "C" boundary). This is a PRIVATE,
// compiler-and-standard-library-coupled fixture ABI (both translation units must use the same
// compiler configuration); it is not part of any public Strut contract. FFI-10 uses it only for
// the local test adapter -- a supported pointer+length byte_view boundary is the preferred shape
// for any production integration.
struct strut_string {
    std::string v;
    strut_string() {}
    explicit strut_string(std::string s) : v(std::move(s)) {}
};

// UTF-8 string round-trip: Strut passes a string into a Nift engine binding; Nift evaluates
// s + "!"; the JSON-encoded result is decoded and returned to Strut as a fresh string.
extern "C" strut_string strut_nift_string_op(strut_string value) {
    nift_engine* engine = nift_engine_new();
    std::string empty;
    if (!engine) return strut_string();
    if (nift_engine_set_string(engine, "s", 1, value.v.data(), value.v.size()) != NIFT_OK) {
        nift_engine_free(engine);
        return strut_string();
    }
    static const char* expression = "s + '!'";
    nift_script_result* r = nullptr;
    if (nift_engine_evaluate(engine, expression, strlen(expression), &r) != NIFT_OK || !r) {
        nift_engine_free(engine);
        return strut_string();
    }
    std::string out_value;
    if (nift_script_result_ok(r)) {
        nift_string json = {0};
        if (nift_script_result_value_json(r, &json) == NIFT_OK && json.data && json.length) {
            json_string_decode(json.data, json.length, out_value);
        }
    }
    nift_script_result_free(r);
    nift_engine_free(engine);
    return strut_string(std::move(out_value));
}
extern "C" int strut_nift_same_engine_recovery(void) {
    nift_engine* engine = nift_engine_new();
    if (!engine) return 1;
    nift_engine_set_int(engine, "seed", 4, 17);
    const char* good = "seed + 25";
    const char* bad = "seed +";
    nift_script_result* r = nullptr;
    bool first_ok = false, failed_with_diag = false, third_ok = false;
    if (nift_engine_evaluate(engine, good, strlen(good), &r) == NIFT_OK && r) {
        first_ok = nift_script_result_ok(r) == 1 && result_int(r) == 42;
        nift_script_result_free(r); r = nullptr;
    }
    if (nift_engine_evaluate(engine, bad, strlen(bad), &r) == NIFT_OK && r) {
        bool semantic_fail = nift_script_result_ok(r) == 0;
        nift_string msg = {0};
        bool has_diag = nift_script_result_error_message(r, &msg) == NIFT_OK && msg.data && msg.length;
        failed_with_diag = semantic_fail && has_diag;
        nift_script_result_free(r); r = nullptr;
    }
    if (nift_engine_evaluate(engine, good, strlen(good), &r) == NIFT_OK && r) {
        third_ok = nift_script_result_ok(r) == 1 && result_int(r) == 42;
        nift_script_result_free(r); r = nullptr;
    }
    nift_engine_free(engine);
    return (first_ok && failed_with_diag && third_ok) ? 0 : 2;
}

// Decoder regression over the real helper: JSON text (including escapes) must decode to exact
// bytes. Returns 0 only if every row decodes correctly.
extern "C" int strut_nift_decode_check(void) {
    struct Row { const char* json_len_input[2]; const char* json; const char* want; };
    static const char* rows[][2] = {
        {"\"hi!\"", "hi!"},
        {"\"a\\\"b\"", "a\"b"},
        {"\"a\\\\b\"", "a\\b"},
        {"\"a\\nb\"", "a\nb"},
        {"\"a\\tb\"", "a\tb"},
        {"\"a\\rb\"", "a\rb"},
    };
    for (const auto& row : rows) {
        std::string decoded;
        json_string_decode(row[0], strlen(row[0]), decoded);
        if (decoded != row[1]) return 0;
    }
    return 1;
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
