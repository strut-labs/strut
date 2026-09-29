function install_routes(http_server app) -> void : NetworkError {
    app.get("/health", (http_request req) => { return http_text("ok"); });
}
