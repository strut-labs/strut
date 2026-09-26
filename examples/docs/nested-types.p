include <map>;

function main() -> int {
    map<string, int[]> groups;
    vector<function<(int)->int>> operations := [];
    function<(map<string, int[]>, int)->int> count :=
        (map<string, int[]> values, int fallback) => fallback;
    return 0;
}
