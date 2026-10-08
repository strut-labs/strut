export "C" function ff_add(int a, int b) -> int {
    return a + b;
}
export "C" function ff_mul(int_64 a, int_64 b) -> int_64 {
    return a * b;
}
export "C" function ff_scale(double_32 value, double_32 factor) -> double_32 {
    return value * factor;
}
export "C" function ff_scale64(double_64 value, double_64 factor) -> double_64 {
    return value * factor;
}
export "C" function ff_byte(uint_8 value) -> uint_8 {
    return value;
}
export "C" function ff_zero() -> int {
    return 7;
}
export "C" function ff_note(int value) -> void {
    return;
}
export "C" function ff_str_echo(string s) -> string {
    return s;
}
export "C" function ff_str_dup(string s) -> string {
    return s + s;
}
export "C" function ff_str_len(string s) -> int {
    return s.length;
}
export "C" function ff_bytes_echo(bytes b) -> bytes {
    return b;
}
