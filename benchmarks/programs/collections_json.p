function main() -> void {
    int[] xs := [1,2,3,4,5,6,7,8];
    ys := xs.map((x) => x * 3).filter((x) => x > 9);
    data := {"name":"strut","values":[1,2,3],"active":true};
    encoded := json.stringify(data);
    parsed := json.parse(encoded);
    print(ys.reduce((a,b) => a + b));
    print(json.stringify(parsed));
    return;
}
