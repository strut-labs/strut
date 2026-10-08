error CallbackError { string message; int code; }
extern "C" function native_apply_fallible(function<(int_32)->int_32 : CallbackError> f, int_32 v) -> int_32 : CallbackError;
function mycb(int_32 x) -> int_32 : CallbackError {
    if (x < 0) {
        throw CallbackError { message: "thrown", code: 5 };
    }
    return x * 4;
}
function main() -> int {
    unsafe {
        try {
            println(native_apply_fallible(mycb, 3));
            println(native_apply_fallible(mycb, -1));
        } catch (CallbackError e) {
            println(e.message);
            println(e.code);
        }
    }
    return 0;
}
