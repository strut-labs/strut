extern "C" function strut_nift_add25_seventeen() -> int_64;
extern "C" function strut_nift_eval(int_64 seed) -> int_64;

function main() -> void {
    unsafe {
        print(strut_nift_add25_seventeen());
        print(strut_nift_eval(0));
        print(strut_nift_eval(-25));
        print(strut_nift_eval(100));
    }
    return;
}
