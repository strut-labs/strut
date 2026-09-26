function main() -> int {
    int[] values := [1, 2, 3];
    print(values.reduce((total, value) => total + value));
    return 0;
}
