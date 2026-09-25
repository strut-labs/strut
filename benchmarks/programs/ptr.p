function main() -> void {
    ptr<int> p := ptr(1);
    int total := 0;
    for (int i := 0; i < 100000; i++) {
        ptr<int> q := p;
        total = total + *q;
    }
    print(total);
    return;
}
