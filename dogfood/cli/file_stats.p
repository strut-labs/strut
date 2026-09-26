function main(string[] args) -> int : StreamError {
    path := env("STRUT_INPUT") ?? "dogfood-input.txt";
    if (args.length() > 0) { path = args[0]; }
    ifstream input(path);
    text := input.read_all();
    lines := text.split("\n");
    print(text.size());
    print(lines.size());
    input.close();
    return 0;
}
