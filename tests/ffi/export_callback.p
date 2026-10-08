export "C" function apply_twice(function<(int_32)->int_32> f, int_32 v) -> int_32 {
    return f(v) + f(v);
}
export "C" function apply_ctx(function<(int_32)->int_32> f, int_32 v) -> int_32 {
    return f(v) + f(v + 1);
}
