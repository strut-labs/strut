struct Pair { int a; int b; }
extern "C" function c_add(int a, int b) -> int;
extern "C" function c_pair_sum(Pair p) -> int;
extern "C" function c_pair_make(int a, int b) -> Pair;
extern "C" function c_scale(double_64 value, double_64 factor) -> double_64;
extern "C" function c_read_int(raw_ptr<int> value) -> int;
extern "C" function c_increment(raw_ptr<int> value) -> void;
function main() -> void {
    unsafe {
        print(c_add(20, 22));
        print(c_pair_sum(Pair { a: 3, b: 4 }));
        made := c_pair_make(5, 6);
        print(made.a + made.b);
        print(c_scale(1.5, 2.0));
        owner := ptr(9);
        raw_ptr<int> raw_value := raw(owner);
        print(c_read_int(raw_value));
        c_increment(raw_value);
        print(*owner);
    }
    return;
}
