function main() -> int : NetworkError {
    app := http_server();
    app.timeouts(5000, 5000, 5000, 2000);
    app.limits(1024, 8192, 32, 16);
    app.get("/health", (http_request request) => {
        return http_text("ok");
    });

    // A finite request count is useful for tests. Production services normally
    // omit the third argument and call stop() from their shutdown coordinator.
    app.listen("127.0.0.1", 18087, 1);
    if (app.running()) {
        return 1;
    }
    return 0;
}
