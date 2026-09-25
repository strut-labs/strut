function main() -> void : StreamError {
    path := env("NIFT_INFO") ?? "index.info.json";
    ifstream input(path);
    info := json.parse(input.read_all());
    input.close();

    print(info["name"]);
    print(info["content"]);
    print(info["output"]);
    print(json.stringify(info["dependencies"]));
    return;
}
