include <filesystem>;
include <vector>;

function main() -> void : (StreamError, FilesystemError) {
    root := env("STRUT_SOURCE_ROOT") ?? ".";
    files := walk(root);
    int source_files := 0;
    int_64 source_bytes := 0;
    for (entry : files) {
        path := join_path(root, entry);
        if (is_file(path) && extension(path) == ".p") {
            source_files = source_files + 1;
            source_bytes = source_bytes + file_size(path);
        }
    }
    print(source_files);
    print(source_bytes);
    return;
}
