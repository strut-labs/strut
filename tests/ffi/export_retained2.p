export "C" function make_adder2(int_32 base) -> retained_callback<(int_32)->int_32> {
    return retained_callback((int_32 x) => x + base);
}
