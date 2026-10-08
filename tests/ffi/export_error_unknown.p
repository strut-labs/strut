error ParseError { string message; int code; }
extern "C" function native_bad(int_32 x) -> int_32 : ParseError;
function main() -> int {
    unsafe {
        try {
            println(native_bad(1));
        } catch (ParseError e) {
            println("caught");
        }
    }
    return 0;
}
