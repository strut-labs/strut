function main() -> int {
    int* value := new(7);
    raw_ptr<int> address := ptr(value);
    return 0;
}
