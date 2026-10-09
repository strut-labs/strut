export "C" function make_adder(int_32 base) -> retained_callback<(int_32)->int_32> {
    return retained_callback((int_32 x) => x + base);
}
export "C" function apply_retained(retained_callback<(int_32)->int_32> c, int_32 v) -> int_32 {
    return c(v);
}
