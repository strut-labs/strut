function identity[T](T value) -> T {
    return value;
}

function main() -> int {
    int[] initial := [1, 2, 3];
    int[] values := identity(initial);
    print(values.reduce(0, (total, value) => total + value));
    return 0;
}
