error CallbackError {
    string message;
    int code;
}
export "C" function call_cb(function<(int_32)->int_32 : CallbackError> cb, int_32 value) -> int_32 : CallbackError {
    return cb(value);
}
