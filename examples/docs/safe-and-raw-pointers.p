function main() -> int {
    int* value := new(7);
    unsafe {
        raw_ptr<int> address := ptr(value);
        print(*address);
    }
    return 0;
}
