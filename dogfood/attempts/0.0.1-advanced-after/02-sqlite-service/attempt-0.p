function main() -> int : (NetworkError, SqliteError) {
    db := sqlite_open(":memory:");
    db.exec("CREATE TABLE notes(id INTEGER PRIMARY KEY, text TEXT)");
    app := http_server();
    app.get("/notes", (http_request req) => { try { return http_json_response(db.query("SELECT * FROM notes")); } catch (SqliteError e) { return http_text("error"); } });
    app.listen("127.0.0.1", 18081);
    return 0;
}
