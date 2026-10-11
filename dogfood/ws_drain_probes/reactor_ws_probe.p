function main() -> int : NetworkError {
    app := http_server();
    app.get("/plain", (http_request request) => http_text("ok"));
    app.websocket("/ws", (http_request request, websocket socket) => {
        try { socket.accept(); } catch (NetworkError e) { }
        return;
    });
    app.listen("127.0.0.1", 49322, 32);
    return 0;
}
