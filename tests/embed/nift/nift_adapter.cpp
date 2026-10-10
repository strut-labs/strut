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

// Decode a complete JSON string literal into exact bytes with a success/failure contract.
// Returns false (leaving out untouched) for: leading/trailing garbage or a non-string JSON value,
// unterminated input, unknown or incomplete escapes, unescaped control bytes, isolated or
// truncated Unicode surrogate escapes, and trailing content after the closing quote. Embedded NUL
// is preserved via \u0000. Surrogate pairs are combined and encoded as UTF-8.
static bool json_string_decode(const char* data, size_t len, std::string& out) {
    if (!data || len < 2 || data[0] != '"' || data[len - 1] != '"') return false;
    out.clear();
    size_t i = 1;
    while (i < len) {
        unsigned char c = (unsigned char)data[i];
        if (c == '"') {
            if (i + 1 == len) return true;       // closing quote consumed, nothing trailing
            return false;                          // trailing content after closing quote
        }
        if (c == '\\') {
            ++i;
            if (i + 1 >= len) return false;        // escape at end of input
            char e = data[i];
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
                    if (i + 5 > len) return false;               // \ uXXXX needs 6 chars
                    unsigned int cp = 0;
                    for (int k = 1; k <= 4; ++k) {
                        char h = data[i + (size_t)k];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= (unsigned int)(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= (unsigned int)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= (unsigned int)(h - 'A' + 10);
                        else return false;
                    }
                    i += 5;                                       // consumed \uXXXX
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        // high surrogate: require a following low surrogate
                        if (i + 6 > len || data[i] != '\\' || data[i + 1] != 'u') return false;
                        unsigned int low = 0;
                        for (int k = 2; k <= 5; ++k) {
                            char h = data[i + (size_t)k];
                            low <<= 4;
                            if (h >= '0' && h <= '9') low |= (unsigned int)(h - '0');
                            else if (h >= 'a' && h <= 'f') low |= (unsigned int)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') low |= (unsigned int)(h - 'A' + 10);
                            else return false;
                        }
                        if (low < 0xDC00 || low > 0xDFFF) return false;
                        i += 6;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return false;              // isolated low surrogate
                    }
                    // UTF-8 encode
                    if (cp < 0x80) { out += (char)cp; }
                    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
                    else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
                    else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
                    break;
                }
                default: return false;             // unknown escape
            }
        } else {
            if (c < 0x20u) return false;           // unescaped control byte
            out += (char)c;
            ++i;
        }
    }
    return false;                                  // unterminated
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
// s + "!"; the JSON-encoded result is decoded into *out. Status/result contract mirrors the
// integer adapter: 0 success, 1 engine, 2 binding, 3 mechanical, 4 semantic, 5 conversion,
// 6 decode failure/type mismatch. *out is untouched unless 0 is returned, so an empty successful
// string and a failure are distinguishable.
extern "C" int strut_nift_string_op(strut_string value, strut_string* out) {
    if (!out) return 6;
    nift_engine* engine = nift_engine_new();
    if (!engine) return 1;
    if (nift_engine_set_string(engine, "s", 1, value.v.data(), value.v.size()) != NIFT_OK) {
        nift_engine_free(engine);
        return 2;
    }
    static const char* expression = "s + '!'";
    nift_script_result* r = nullptr;
    if (nift_engine_evaluate(engine, expression, strlen(expression), &r) != NIFT_OK || !r) {
        nift_engine_free(engine);
        return 3;
    }
    int status = 4;
    if (nift_script_result_ok(r)) {
        nift_string json = {0};
        status = 5;
        if (nift_script_result_value_json(r, &json) == NIFT_OK && json.data) {
            std::string decoded;
            if (json_string_decode(json.data, json.length, decoded)) {
                out->v = std::move(decoded);
                status = 0;
            } else {
                status = 6;
            }
        }
    }
    nift_script_result_free(r);
    nift_engine_free(engine);
    return status;
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

// Decoder regression over the real helper: each row is a JSON text plus the exact expected bytes
// (or "REJECT"). A NUL byte is captured through the explicit-length input, never strlen.
extern "C" int strut_nift_decode_check(void) {
    struct Row { const char* json; size_t len; const char* want; size_t want_len; int expect; };
    static const Row rows[] = {
        {"\"hi!\"", 5, "hi!", 3, 1},
        {"\"\"", 2, "", 0, 1},
        {"\"a\\\"b\"", 6, "a\"b", 3, 1},
        {"\"a\\\\b\"", 6, "a\\b", 3, 1},
        {"\"a\\nb\"", 6, "a\nb", 3, 1},
        {"\"\\u0000\"", 8, "\0", 1, 1},
        {"\"\\u00E9\"", 8, "\xC3\xA9", 2, 1},
        {"\"\\uD83D\\uDE00\"", 14, "\xF0\x9F\x98\x80", 4, 1},
        {"\"\\uD800\"", 8, "", 0, 0},
        {"\"\\uDC00\"", 8, "", 0, 0},
        {"\"\\x\"", 4, "", 0, 0},
        {"\"unterminated", 13, "", 0, 0},
        {"null", 4, "", 0, 0},
        {"42", 2, "", 0, 0},
        {"\"a\"b\"", 6, "", 0, 0},
        {"\"\\u12g4\"", 9, "", 0, 0},
    };
    for (const auto& row : rows) {
        std::string decoded = "SENTINEL";
        bool ok = json_string_decode(row.json, row.len, decoded);
        if (ok != (row.expect == 1)) return 0;
        if (ok && !(decoded.size() == row.want_len && memcmp(decoded.data(), row.want, row.want_len) == 0)) return 0;
    }
    return 1;
}


// FFI-10 binary increment: C-compatible byte boundary internal to the adapter. A Strut-origin
// call drives Nift's byte engine: nift_engine_set_bytes -> evaluate "b" -> value_bytes, verified
// byte-for-byte (explicit lengths; embedded NUL and high-bit bytes are handled, never via
// C-string functions). Status: 1 engine, 2 binding, 3 mechanical, 4 semantic, 5 conversion.
// The full (ptr,len) aggregate across the extern boundary is the next scoped step; this fixture
// keeps the C-compatible view inside the adapter translation unit.
extern "C" int strut_nift_bytes_check(void) {
    static const unsigned char payload[] = {0x61, 0x00, 0x62, 0xFF, 0x80};
    for (int pass = 0; pass < 3; ++pass) {          // repeated calls
        const unsigned char* data = payload;
        size_t len = 5;
        if (pass == 1) { data = nullptr; len = 0; } // empty buffer
        nift_engine* engine = nift_engine_new();
        if (!engine) return 1;
        if (nift_engine_set_bytes(engine, "b", 1, data, len) != NIFT_OK) {
            nift_engine_free(engine);
            return 2;
        }
        nift_script_result* r = nullptr;
        static const char* expression = "b";
        if (nift_engine_evaluate(engine, expression, strlen(expression), &r) != NIFT_OK || !r) {
            nift_engine_free(engine);
            return 3;
        }
        if (!nift_script_result_ok(r)) {
            nift_script_result_free(r);
            nift_engine_free(engine);
            return 4;
        }
        nift_bytes out = {0};
        if (nift_script_result_value_bytes(r, &out) != NIFT_OK) {
            nift_script_result_free(r);
            nift_engine_free(engine);
            return 5;
        }
        bool exact = out.length == len && (len == 0 || memcmp(out.data, data, len) == 0);
        nift_script_result_free(r);
        nift_engine_free(engine);
        if (!exact) return 6;
    }
    return 0;
}


// Genuine Strut-source byte marshalling: Strut supplies a pointer+length (raw_ptr<uint_8> from a
// bytes value's .data()) and an output buffer+capacity; the adapter routes the exact bytes
// through Nift's byte engine (set_bytes -> evaluate "b" -> value_bytes) and copies the result
// back into caller-owned storage. Statuses: 1..5 as above, 7 insufficient output capacity
// (no writes, *out_len untouched). No C-string conversion; every byte compared by the caller.
extern "C" int strut_nift_bytes_roundtrip(const uint8_t* input, int32_t input_length,
                                          uint8_t* output, int32_t output_capacity,
                                          int32_t* output_length) {
    // Strict native-boundary validation BEFORE any signed-to-unsigned conversion. All failures
    // leave *output_length (when non-null) and caller-owned storage untouched.
    if (!output_length) return 8;
    if (input_length < 0) return 9;
    if (output_capacity < 0) return 10;
    if (input_length > 0 && !input) return 11;
    nift_engine* engine = nift_engine_new();
    if (!engine) return 1;
    if (nift_engine_set_bytes(engine, "b", 1, input, (size_t)input_length) != NIFT_OK) {
        nift_engine_free(engine);
        return 2;
    }
    nift_script_result* r = nullptr;
    static const char* expression = "b";
    if (nift_engine_evaluate(engine, expression, strlen(expression), &r) != NIFT_OK || !r) {
        nift_engine_free(engine);
        return 3;
    }
    if (!nift_script_result_ok(r)) {
        nift_script_result_free(r);
        nift_engine_free(engine);
        return 4;
    }
    nift_bytes out = {0};
    if (nift_script_result_value_bytes(r, &out) != NIFT_OK) {
        nift_script_result_free(r);
        nift_engine_free(engine);
        return 5;
    }
    int status = 5;
    if (out.length > (size_t)output_capacity) {
        status = 7;                                       // insufficient capacity: no writes
    } else if (out.length > 0 && !output) {
        status = 11;                                      // NULL output with positive length
    } else {
        if (out.length) memcpy(output, out.data, out.length);
        *output_length = (int32_t)out.length;             // success only updates the contract
        status = 0;
    }
    nift_script_result_free(r);
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
