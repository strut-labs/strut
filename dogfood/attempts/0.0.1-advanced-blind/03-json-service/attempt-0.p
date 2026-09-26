function main() -> int : NetworkError {
    app := http_server();
    app.post("/echo", (http_request req) => { return http_json_response(req.json()); });
    app.listen("127.0.0.1", 18082);
    return 0;
}
