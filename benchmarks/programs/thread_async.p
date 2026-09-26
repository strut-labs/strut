async function add(int a, int b) -> int { return a + b; }
function main() -> void : ThreadError {
    p := new(0);
    t := thread(() => { *p = 7; });
    t.join();
    f := add(*p, 35);
    print(await f);
    return;
}
