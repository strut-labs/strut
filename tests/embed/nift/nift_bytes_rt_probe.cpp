// FFI-10 binary adapter negative-branch coverage that cannot be expressed in Strut source:
// NULL input/output pointers and negative capacities are exercised by calling the adapter
// directly. All branches verify status codes and that *output_length survives failures.
#include <stdint.h>
#include <stdio.h>

extern "C" int strut_nift_bytes_roundtrip(const uint8_t* input, int32_t input_length,
                                          uint8_t* output, int32_t output_capacity,
                                          int32_t* output_length);

static const uint8_t payload[5] = {0x61, 0x00, 0x62, 0xFF, 0x80};

int main(void) {
    uint8_t out[5] = {0};
    int32_t olen;
    int ok = 1;

    olen = 123;
    if (strut_nift_bytes_roundtrip(payload, 5, out, -1, &olen) != 10) ok = 0;      // negative capacity
    else if (olen != 123) ok = 0;

    if (strut_nift_bytes_roundtrip(payload, 5, out, 5, NULL) != 8) ok = 0;         // NULL out_len

    olen = 123;
    if (strut_nift_bytes_roundtrip(NULL, 5, out, 5, &olen) != 11) ok = 0;          // NULL input + positive len
    else if (olen != 123) ok = 0;

    olen = 123;
    if (strut_nift_bytes_roundtrip(payload, 5, NULL, 5, &olen) != 11) ok = 0;      // NULL output + positive result
    else if (olen != 123) ok = 0;

    olen = 99;
    if (strut_nift_bytes_roundtrip(NULL, 0, NULL, 0, &olen) != 0) ok = 0;          // valid empty round trip
    else if (olen != 0) ok = 0;

    if (!ok) { fprintf(stderr, "nift bytes probe FAILED\n"); return 1; }
    printf("probe-ok\n");
    return 0;
}