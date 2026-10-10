export "C" function add(int_32 a, int_32 b) -> int_32 { return a + b; }
export "C" function nift_verify_bytes(bytes data) -> int_32 {
    if (data.length() != 5) { return -1; }
    if (data[0] != 97 || data[1] != 0 || data[2] != 98 || data[3] != 255 || data[4] != 128) { return -2; }
    return 42;
}
export "C" function nift_verify_string(string text) -> int_32 {
    if (text != "héllo ") { return 1000 + text.length(); }
    return 42;
}
