extern "C" function native_apply(function<(int_32)->int_32> f, int_32 v) -> int_32;
function main() -> int {
    base := 3;
    function<(int_32)->int_32> f := (x) => x * base;
    unsafe {
        println(native_apply(f, 7));
    }
    return 0;
}
