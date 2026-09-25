function main() -> void {
    add := (int a, int b) => a + b;
    int total := 0;
    for (int i := 0; i < 100000; i++) {
        total = add(total, 1);
    }
    print(total);
    return;
}
