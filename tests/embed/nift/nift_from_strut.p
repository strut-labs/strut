extern "C" function strut_nift_eval(int_64 seed) -> int_64;

function main() -> void {
    unsafe {
        v := strut_nift_eval(1);
        print(v);
    }
    return;
}
