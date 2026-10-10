extern "C" function strut_nift_bytes_roundtrip(raw_ptr<uint_8> input, int_32 input_length, raw_ptr<uint_8> output, int_32 output_capacity, ptr<int_32> output_length) -> int_32;

function main() -> void {
    unsafe {
        bytes payload := [97, 0, 98, 255, 128];
        bytes output_b := bytes(payload.length());
        ol := new(0);
        ptr<int_32> pol := ptr(ol);
        rc := strut_nift_bytes_roundtrip(payload.data(), 5, output_b.data(), 5, pol);
        if (rc != 0) { print("RT-ERR"); }
        else if (output_b.length() != 5) { print("RT-LEN"); }
        else if (output_b[0] == 97 && output_b[1] == 0 && output_b[2] == 98 && output_b[3] == 255 && output_b[4] == 128 && *ol == 5) { print("BYTES-RT-OK"); }
        else { print("BYTES-RT-BAD"); }
        bytes eb := bytes();
        rc = strut_nift_bytes_roundtrip(eb.data(), 0, eb.data(), 0, pol);
        if (rc != 0 || *ol != 0) { print("EMPTY-BAD"); } else { print("EMPTY-OK"); }
        // Negative input length must be rejected (status 9) and *ol left unchanged.
        rc = strut_nift_bytes_roundtrip(payload.data(), -1, output_b.data(), 5, pol);
        if (rc != 9 || *ol != 0) { print("NEG-IN-BAD"); } else { print("NEG-IN-OK"); }
        // Insufficient capacity (1-byte sentinel-filled buffer) must return 7 without writing.
        bytes small := bytes(1);
        small[0] = 77;
        rc = strut_nift_bytes_roundtrip(payload.data(), 5, small.data(), 1, pol);
        if (rc != 7 || small[0] != 77) { print("CAP-BAD"); } else { print("CAP-OK"); }
    }
    return;
}
