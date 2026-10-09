error RetainedErr { string message; }
function risky(int_32 x) -> int_32 : RetainedErr {
    if (x < 0) { throw RetainedErr { message: "neg" }; }
    return x * 2;
}
retained_callback<(int_32)->int_32> g_bridge := retained_callback((int_32 x) => x);
function outerfn(int_32 x) -> int_32 : RetainedErr {
    r := g_bridge(x);
    if (x < 0) { throw RetainedErr { message: "outer" }; }
    return x * 3;
}
export "C" function make_fallible() -> retained_callback<(int_32)->int_32 : RetainedErr> {
    return retained_callback(risky);
}
export "C" function make_reentrant(native_callback<(int_32)->int_32> br) -> retained_callback<(int_32)->int_32 : RetainedErr> {
    g_bridge = retained_callback(br);
    return retained_callback(outerfn);
}
