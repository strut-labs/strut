function main() -> int : NetworkError {
    app := http_server();
    app.post("/echo", (http_request req) => {
        try {
            return http_json_response(req.json());
        } catch (HttpError err) {
            return http_text(err.message);
        }
    });
    app.listen("127.0.0.1", 18082);
    return 0;
}
