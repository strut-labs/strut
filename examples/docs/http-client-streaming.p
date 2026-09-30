function download(string url) -> int_64 : HttpError {
    int_64 initial := 0;
    total := new(initial);
    head := http_request_stream("GET", url,
        {"max_response_body_bytes": 67108864}, null, (bytes chunk) => {
            *total = *total + chunk.length();
            return true;
        });
    if (head.status != 200) { return 0; }
    return *total;
}
function main() -> int {
    return 0;
}
