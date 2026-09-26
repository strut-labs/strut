include <map>;

function main() -> void : (SqliteError, NetworkError, EmbedError) {
    sqlite_db db := sqlite_open("dogfood.db");
    db.exec("CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY, name TEXT)");
    assets := embed_dir("dogfood/web/public");
    http_server app := http_server();
    app.get_async("/api/events", async (http_request req) => {
        return http_json_response(db.query("SELECT id,name FROM events ORDER BY id"));
    });
    app.static("/", assets, "index.html");
    app.listen("127.0.0.1", 18086, 1);
    db.close();
    return;
}
