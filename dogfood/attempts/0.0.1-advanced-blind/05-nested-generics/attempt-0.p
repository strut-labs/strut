include <map>;
function identity[T](T value) -> T { return value; }
function main() -> int {
    map<string,int[]> groups;
    groups["odd"] = identity([1, 3, 5]);
    println(groups["odd"].length);
    return 0;
}
