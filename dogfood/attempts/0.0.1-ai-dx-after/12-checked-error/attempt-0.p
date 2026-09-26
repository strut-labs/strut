include <filesystem>;
function main() -> int : FilesystemError {
    text := read_file("value.txt");
    print(text.size());
    return 0;
}
