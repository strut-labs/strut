function identity[T](T value) -> T { return value; }
function main() -> int {
    int[] values := identity([1, 2, 3]);
    print(values.reduce(0, (total, value) => total + value));
    return 0;
}
