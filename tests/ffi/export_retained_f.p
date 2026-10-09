error RetainedErr { string message; }
function risky(int_32 x) -> int_32 : RetainedErr {
    if (x < 0) { throw RetainedErr { message: "neg" }; }
    return x * 2;
}
export "C" function make_fallible() -> retained_callback<(int_32)->int_32 : RetainedErr> {
    return retained_callback(risky);
}
