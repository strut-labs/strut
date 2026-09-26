function main() -> void {
    int* p := ptr(1);
    int total := 0;
    for (int i := 0; i < 100000; i++) {
        int* q := p;
        total = total + *q;
    }
    print(total);
    return;
}
