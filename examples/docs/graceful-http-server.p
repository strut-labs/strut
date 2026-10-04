function main() -> int : (NetworkError, ThreadError) {
    app := http_server();
    app.get("/health", (http_request request) => {
        return http_text("ok");
    });

    listener := thread(() => {
        try {
            app.listen("127.0.0.1", 8080);
        } catch (NetworkError e) {
        }
    });
    wait_for_shutdown_signal();
    app.stop();
    listener.join();
    return 0;
}
