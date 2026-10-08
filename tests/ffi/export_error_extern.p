error ParseError { string message; int code; }
error OtherError { string message; int code; }
extern "C" function native_parse(int_32 x) -> int_32 : ParseError;
extern "C" function native_pick(int_32 x) -> int_32 : (ParseError, OtherError);
function main() -> int {
    unsafe {
        try {
            println(native_parse(3));
            println(native_parse(-1));
        } catch (ParseError e) {
            println(e.message);
            println(e.code);
        }
        try {
            println(native_pick(1));
        } catch (ParseError e) {
            println(e.message);
            println(e.code);
        }
        try {
            println(native_pick(2));
        } catch (OtherError e) {
            println(e.message);
            println(e.code);
        }
    }
    return 0;
}
