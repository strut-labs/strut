async function answer() -> int { return 42; }
function main() -> int {
    print(await answer());
    return 0;
}
