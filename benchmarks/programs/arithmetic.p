function main() -> void {
    int x := 0;
    for (int i := 0; i < 200000; i++) {
        x = x + i % 17;
    }
    print(x);
    return;
}
