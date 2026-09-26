function load() -> int : IOError {
    throw IOError("unavailable");
}

function main() -> int {
    return load();
}
