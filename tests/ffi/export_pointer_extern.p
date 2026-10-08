extern "C" function native_inc_raw(raw_ptr<int_32> p) -> void;
extern "C" function native_inc_ref(ref<int_32> p) -> void;
extern "C" function native_read(raw_ptr<int_32> p) -> int_32;
function main() -> int {
    int* owner := new(41);
    unsafe {
        raw_ptr<int_32> rp := ptr(owner);
        native_inc_raw(rp);
        println(native_read(rp));
        native_inc_ref(ref(*owner));
    }
    println(*owner);
    return 0;
}
