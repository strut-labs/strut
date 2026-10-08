error DemoError {
    string message;
    int code;
}
export "C" function maybe_fail(int_32 x) -> int_32 : DemoError {
    if (x < 0) {
        throw DemoError { message: "negative", code: 7 };
    }
    return x * 2;
}
export "C" function do_it(int_32 x) -> void : DemoError {
    if (x < 0) {
        throw DemoError { message: "void-negative", code: 9 };
    }
    return;
}
