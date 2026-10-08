export "C" function pt_inc(raw_ptr<int_32> p) -> void {
    unsafe { *p = *p + 1; }
}
export "C" function rf_inc(ref<int_32> p) -> void {
    *p = *p + 1;
}
export "C" function pt_read(raw_ptr<int_32> p) -> int_32 {
    unsafe { return *p; }
}
export "C" function pt_is_null(raw_ptr<int_32> p) -> int_32 {
    unsafe {
        if (p == null) {
            return 1;
        }
        return 0;
    }
}
