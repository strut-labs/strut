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
error OtherError {
    string message;
    int code;
}
export "C" function pick(int_32 x) -> int_32 : (DemoError, OtherError) {
    if (x == 1) {
        throw DemoError { message: "one", code: 1 };
    }
    if (x == 2) {
        throw OtherError { message: "two", code: 2 };
    }
    return x;
}
