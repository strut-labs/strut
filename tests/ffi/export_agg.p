struct Pair {
    int_32 a;
    int_32 b;
}
struct Mixed {
    int_32 a;
    double_64 b;
    uint_8 c;
}

export "C" function agg_sum(Pair p) -> int_32 {
    return p.a + p.b;
}
export "C" function agg_make(int_32 x, int_32 y) -> Pair {
    return Pair { a: x, b: y };
}
export "C" function agg_mixed_weight(Mixed m) -> double_64 {
    return m.b;
}
export "C" function agg_mixed_make(int_32 a, double_64 b, uint_8 c) -> Mixed {
    return Mixed { a: a, b: b, c: c };
}
export "C" function agg_double_sum(Pair p) -> int_32 {
    return agg_sum(p) + agg_sum(p);
}
