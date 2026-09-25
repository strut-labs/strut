function main() -> void : StreamError {
    path := env("STRUT_INPUT") ?? "dogfood-input.txt";
    ifstream input(path);
    text := input.read_all();
    lines := text.split("\n");
    print(text.size());
    print(lines.size());
    input.close();
    return;
}
