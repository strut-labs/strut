function main() -> int : SqliteError {
    db := sqlite_open(":memory:");
    db.exec("CREATE TABLE users(id INTEGER, name TEXT)");
    db.exec("INSERT INTO users VALUES (?, ?)", json.parse("[1,\"Ada\"]"));
    rows := db.query("SELECT id, name FROM users");
    print(json.stringify(rows));
    return 0;
}
