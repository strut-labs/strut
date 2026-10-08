export "C" function ff_add(int a, int b) -> int {
    return a + b;
}
export "C" function ff_mul(int_64 a, int_64 b) -> int_64 {
    return a * b;
}
export "C" function ff_scale(double_64 value, double_64 factor) -> double_64 {
    return value * factor;
}
export "C" function ff_negate(bool flag) -> int {
    if (flag) { return 1; }
    return 0;
}
