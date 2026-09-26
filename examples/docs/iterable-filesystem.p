include <filesystem>;
include <list>;

function main() -> int : FilesystemError {
    list<string> sources;
    sources.push("draft.md");
    sources.push("notes.md");
    string[] destinations := ["archive/draft.md", "archive/notes.md"];
    copy(sources, destinations);
    return 0;
}
