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

## Server lifecycle

`http_server` accepts connections concurrently through a lazily grown reusable worker pool. Queued plus running connections never exceed the configured connection limit, so native worker and socket tracking remain bounded. Saturated plaintext connections receive 503; saturated TLS connections close before handshake. `listen` blocks until `stop()` is requested or the optional finite admitted-connection count is reached. Malformed requests and failed TLS handshakes count once admitted; saturation rejections do not.

`stop()` is safe before start and idempotent. It closes the listener, permits admitted work to drain against one shared graceful-shutdown deadline, then closes queued and running sockets. Synchronous and asynchronous handlers may initiate stop without waiting on themselves. A handler that does not return by the deadline is isolated in its retired listener generation; `running()` remains true and restart or reconfiguration remains unavailable until that handler finishes. A drained server may then be started again.

```strut
app := http_server();
app.timeouts(30000, 30000, 5000, 5000);
app.limits(1048576, 65536, 100, 1024);
app.get("/health", (http_request request) => { return http_text("ok"); });
app.listen("127.0.0.1", 8080);
```

The timeout arguments are read, write, idle and graceful-shutdown milliseconds. The TLS handshake uses one absolute deadline equal to the shortest read, write or idle timeout. The limit arguments are maximum body bytes, header bytes, header count and total admitted connections. Defaults match the values above. Malformed requests and operational limits produce controlled 400, 405, 413, 414, 431, 500, 501, 503 or 505 responses; handler failures do not expose native C++ details.

The server accepts strict HTTP/1.0 and HTTP/1.1 request heads. Request lines require exact space separators and origin-form targets. HTTP/1.1 requires exactly one valid `Host`; HTTP/1.0 permits zero or one. Header names use case-insensitive protocol matching, field syntax and control bytes are validated, and obsolete folded fields are rejected. Parsed occurrences are retained internally; because the current public request header type is single-valued, every repeated field name is conservatively rejected and accepted map keys are normalized to lowercase.

Request bodies remain buffered. One valid `Content-Length` is accepted case-insensitively; duplicate, signed, comma-separated, malformed, overflowing or conflicting lengths are rejected. Length is bounded before allocation or body reads. `Transfer-Encoding` is recognized but unsupported: malformed or non-chunked-final coding lists and every TE/CL combination receive 400, while a syntactically valid list ending in `chunked` receives 501 because decoding is not implemented. The server reads exactly the validated fixed body length and closes after one response, so trailing bytes cannot become another request.

Plaintext premature EOF receives 400. A TLS connection truncated below the HTTP layer always fails closed without handler dispatch; when the TLS record channel is already broken, the server may close without attempting an HTTP error response.

For signal-driven services, start `listen` on a Strut thread, call `wait_for_shutdown_signal()`, then `stop()` and join the listener thread. The runtime signal handler only records the event; ordinary Strut code runs outside raw OS signal context.
