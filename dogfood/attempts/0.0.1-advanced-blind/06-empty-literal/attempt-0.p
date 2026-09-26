function identity[T](T value) -> T { return value; }
function main() -> int {
    values := identity([]);
    println(values.length);
    return 0;
}
