function main() -> int : NetworkError {
    app := http_server();
    app.websocket("/ws", (http_request request, websocket socket) => {
        try { socket.accept(); } catch (NetworkError e) { println(e.message); }
        return;
    });
    app.listen("127.0.0.1", 49321, 1);
    return 0;
}
