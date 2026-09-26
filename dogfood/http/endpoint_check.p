function main() -> void : HttpError {
    url := env("STRUT_ENDPOINT") ?? "https://example.com/";
    response := http_get(url);
    print(response.status);
    print(response.body.size());
    return;
}
