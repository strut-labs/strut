function echo_f(int_32 x) -> int_32 { return x + 1; }
retained_callback<(int_32)->int_32> g_stored := retained_callback(echo_f);
export "C" function adopt_and_store(native_callback<(int_32)->int_32> cb, int_32 v) -> int_32 {
    g_stored = retained_callback(cb);
    local := g_stored;
    return local(v) + 1;
}
export "C" function run_stored(int_32 v) -> int_32 {
    a := g_stored;
    b := g_stored;
    return a(v) + b(v);
}
export "C" function clear_stored() -> void {
    g_stored = retained_callback(echo_f);
    return;
}
