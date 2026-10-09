error RetainedErr { string message; }
function risky(int_32 x) -> int_32 : RetainedErr {
    if (x < 0) { throw RetainedErr { message: "strut" }; }
    return x * 2;
}
retained_callback<(int_32)->int_32 : RetainedErr> g_stored2 := retained_callback(risky);
export "C" function store_nfactory(native_callback<(int_32)->int_32 : RetainedErr> cb, int_32 v) -> int_32 {
    g_stored2 = retained_callback(cb);
    local := g_stored2;
    try { return local(v); }
    catch (RetainedErr _) { return -1; }
}
export "C" function run_stored_nf(int_32 v) -> int_32 {
    a := g_stored2;
    b := g_stored2;
    try { return a(v) + b(v); }
    catch (RetainedErr _) { return -1; }
}
export "C" function clear_stored_nf() -> void {
    g_stored2 = retained_callback(risky);
    return;
}
