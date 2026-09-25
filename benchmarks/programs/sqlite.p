function main() -> void : SqliteError {
    sqlite_db db := sqlite_open(":memory:");
    db.exec("CREATE TABLE t(id INTEGER, value TEXT)");
    db.exec("INSERT INTO t VALUES (?, ?)", json.parse("[1,\"hello\"]"));
    rows := db.query("SELECT value FROM t WHERE id = ?", json.parse("[1]"));
    print(json.stringify(rows));
    return;
}
