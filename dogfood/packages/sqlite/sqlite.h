function package_sqlite_open(string path) -> sqlite_db : SqliteError {
    return sqlite_open(path);
}
