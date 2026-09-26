async function answer() -> int {
    return 42;
}

function main() -> int {
    pending := answer();
    value := await pending;
    print(value);
    return 0;
}
