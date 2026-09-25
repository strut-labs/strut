# Official HTTP client

The CP76 client is backed by libcurl and keeps safe TLS verification enabled. Response bodies are buffered in this first version; streaming is deliberately deferred until a streaming API can be designed without complicating the basic client.

```strut
function main() -> void : HttpError {
    response := http_get("https://example.com/api/status");
    print(response.status);
    print(response.body);
    return;
}
```

Custom requests use a JSON options object for headers, body, timeouts and redirect policy. `http_get_json(url)` and `response.json()` parse through core JSON/JSONIC support. `http_get_async` and `http_request_async` run on Strut's shared executor.

Redirects are followed by default with a maximum of 10 hops. The default timeout is 30 seconds. Response streaming is explicitly not part of CP76.
