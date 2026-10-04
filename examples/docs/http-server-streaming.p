function configure() -> void : NetworkError {
    app := http_server();
    app.get_stream("/events", (http_request request, http_response_writer response) => {
        try {
            response.status(200);
            response.content_type("application/octet-stream");
            response.header("X-Source", "live");
            response.write_bytes(bytes.from_string("first"));
            response.flush();
            response.write_bytes(bytes.from_string("second"));
            response.finish();
        } catch (NetworkError e) {
        }
    });
}
function main() -> int {
    return 0;
}
