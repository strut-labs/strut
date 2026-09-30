function configure() -> void : (NetworkError, WebSocketError) {
    app := http_server();
    app.timeouts(30000, 30000, 5000, 5000);
    app.websocket_limits(1048576, 4194304);
    app.websocket("/echo", (http_request request, websocket socket) => {
        socket.accept();
        text := socket.read_text();
        socket.write_text(text ?? "");
        socket.close(1000, "done");
        return;
    });
}
function main() -> int {
    return 0;
}
