include <filesystem>;
function main() -> int : FilesystemError {
    count := 0;
    for (entry : walk("fixture")) {
        if (is_file(join_path("fixture", entry))) { count++; }
    }
    print(count);
    return 0;
}
