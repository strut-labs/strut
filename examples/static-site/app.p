function main() -> void : (NetworkError, EmbedError) {
    assets := embed_dir("public");
    http_server app := http_server();
    app.static("/", assets, "index.html");
    app.listen("127.0.0.1", 18083, 1);
    return;
}
