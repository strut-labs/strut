struct Pair {
    int_32 a;
    int_32 b;
}
extern "C" function native_pair_make(int_32 a, int_32 b) -> Pair;
extern "C" function native_pair_sum(Pair p) -> int_32;
function main() -> int {
    unsafe {
        p := native_pair_make(3, 4);
        println(native_pair_sum(p));
        q := native_pair_make(5, 6);
        println(native_pair_sum(q));
    }
    return 0;
}
