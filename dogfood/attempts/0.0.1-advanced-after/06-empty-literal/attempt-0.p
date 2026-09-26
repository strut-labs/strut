function identity[T](T value) -> T { return value; }
function main() -> int {
    int[] empty := [];
    values := identity(empty);
    println(values.length);
    return 0;
}
