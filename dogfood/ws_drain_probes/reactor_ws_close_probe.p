function main() -> int : NetworkError {
    app := http_server();
    app.websocket("/explicit-close", (http_request request, websocket socket) => {
        try { socket.accept(); socket.close(1001, "explicit"); } catch (NetworkError e) { } catch (WebSocketError e) { }
        return;
    });
    app.listen("127.0.0.1", 49324, 8);
    return 0;
}
