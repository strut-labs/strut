function main() -> void : SqliteError {
    sqlite_db db := sqlite_open("example.db");
    db.exec("CREATE TABLE IF NOT EXISTS users(id INTEGER, name TEXT)");
    db.exec("DELETE FROM users");
    db.exec("INSERT INTO users VALUES (?, ?)", json.parse("[1,\"Nick\"]"));
    rows := db.query("SELECT id,name FROM users WHERE id = ?", json.parse("[1]"));
    print(json.stringify(rows));
    db.close();
    return;
}
