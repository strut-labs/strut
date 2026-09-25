struct Pair { int a; int b; }
extern "C" function c_add(int a, int b) -> int;
extern "C" function c_pair_sum(Pair p) -> int;
function main() -> void {
    unsafe {
        print(c_add(20, 22));
        print(c_pair_sum(Pair { a: 3, b: 4 }));
    }
    return;
}
