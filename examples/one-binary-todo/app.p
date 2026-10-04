function main() -> void : (SqliteError, NetworkError, EmbedError) {
    sqlite_db db := sqlite_open("todos.db");
    db.exec("CREATE TABLE IF NOT EXISTS todos(id INTEGER PRIMARY KEY, text TEXT)");

    assets := embed_dir("public");
    http_server app := http_server();
    app.get_async("/api/todos", async (http_request req) => {
        try {
            rows := db.query("SELECT id,text FROM todos ORDER BY id");
            return http_json_response(rows);
        } catch (SqliteError e) {
            return http_text("[]");
        }
    });
    app.static("/", assets, "index.html");
    app.listen("127.0.0.1", 18082, 2);
    db.close();
    return;
}
