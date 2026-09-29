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

`stop()` is safe before start and idempotent. Each generation moves through stopped, starting, running and stopping phases under one lifecycle lock. The generation is published before address resolution and bind, so a stop overlapping startup is recorded and prevents that generation from becoming an accepting server. Stop closes the listener, permits admitted work to drain against one shared graceful-shutdown deadline, then closes queued and running sockets. Synchronous and asynchronous handlers may initiate stop without waiting on themselves. A handler that does not return by the deadline is isolated in its retired listener generation; `running()` remains true and restart or reconfiguration remains unavailable until that handler finishes. A drained server may then be started again.

```strut
app := http_server();
app.timeouts(30000, 30000, 5000, 5000);
app.limits(1048576, 65536, 100, 1024);
app.get("/health", (http_request request) => { return http_text("ok"); });
app.listen("127.0.0.1", 8080);
```

The timeout arguments are read, write, idle and graceful-shutdown milliseconds. The TLS handshake uses one absolute deadline equal to the shortest read, write or idle timeout. The limit arguments are maximum body bytes, header bytes, header count and total admitted connections. Defaults match the values above. Malformed requests and operational limits produce controlled 400, 405, 413, 414, 417, 431, 500, 501, 503 or 505 responses; handler failures do not expose native C++ details.

The server accepts strict HTTP/1.0 and HTTP/1.1 request heads. Request lines require exact space separators and origin-form targets. HTTP/1.1 requires exactly one valid `Host`; HTTP/1.0 permits zero or one. Header names use case-insensitive protocol matching, field syntax and control bytes are validated, and obsolete folded fields are rejected. Parsed occurrences are retained internally; because the current public request header type is single-valued, every repeated field name is conservatively rejected and accepted map keys are normalized to lowercase.

Read-only `request.query_values` uses strict percent decoding with `+` interpreted as space and preserves repeated values in wire order through `get(name)`, `values(name)` and `has(name)`. Existing `request.query` remains the raw, last-value compatibility view. Encoded NUL and controls, malformed escapes and invalid targets receive 400. Empty keys and values are retained; a bare name has an empty value. Query pair count is bounded by the configured header-count limit and the encoded target remains bounded by the request-head limit.

A single accepted `Cookie` header is parsed into read-only `request.cookies`, which uses the same `http_values` lookup API and preserves duplicate names. Cookie names must be HTTP tokens, values use strict cookie octets with an optional quoted wrapper, and pair count follows the configured header-count limit; malformed pairs fail the request. Response cookies are structured `http_cookie` values created with `http_cookie(name, value)`, with optional `path`, `domain`, `max_age`, `expires`, `secure`, `http_only` and `same_site` fields. Path must be absolute, Domain uses DNS label syntax, Expires uses IMF-fixdate, SameSite is Strict, Lax or None, and None requires Secure; non-positive Max-Age requests immediate expiry. Assign cookies to `http_server_response.cookies` or call `http_response_writer.cookie(cookie)` before commitment. Each becomes a separate validated `Set-Cookie` field. Generic `Set-Cookie` headers remain reserved, so ordinary duplicate-header rejection is unchanged. At most 64 cookies of at most 4096 serialized bytes each may be emitted.

Buffered handlers retain `request.body`, but both buffered and streaming handlers consume one body reader built from the CP8 framing result. One valid `Content-Length` is accepted case-insensitively; duplicate, signed, comma-separated, malformed, overflowing or conflicting lengths are rejected. Length is bounded before allocation or body reads. A sole HTTP/1.1 `Transfer-Encoding: chunked` is decoded strictly; TE/CL remains 400, malformed or non-final chunked lists remain 400, and supported syntax containing an earlier unsupported coding receives 501. HTTP/1.0 transfer coding is rejected. Chunk sizes, extensions, payload terminators and the terminal zero chunk require exact syntax. Decoded bytes and cumulative chunk-framing bytes are bounded separately by the configured body and header limits. Non-empty trailers are rejected. `Expect` is not implemented and receives 417 instead of triggering implicit buffering or a provisional response.

`get_request_stream` and `post_request_stream` receive `(http_request, http_request_body, http_response_writer)`. The binary-first body handle provides `read_bytes(max_bytes)`, `read_all_bytes(limit?)`, `eof()` and `close()`. Reads expose exactly the validated fixed-length or decoded chunked payload and never expose chunk boundaries. Zero-size reads do not change EOF, reads after close fail, and concurrent reads are rejected. The request body must be consumed or closed before the response writer commits; response commitment during an active read fails, and later body reads fail. This serialization prevents concurrent reads and writes on one TLS transport. Transport failures, malformed/truncated framing and server interruption raise `NetworkError`; TLS errors are normalized at this public boundary. Reader copies share one consumption state and become inactive when the handler returns.

Existing buffered routes consume that same reader into `request.body`, so no second parser or decoder exists and `request.json()` remains compatible. Response-stream-only routes also preserve their CP10 buffered request behavior. Request-stream handlers have no simultaneous buffered copy. If a handler returns without consuming its body, the response is completed and the connection closes without hidden draining. A connection is reused only after the body reaches validated EOF.

Buffered requests provide `text(limit?)`, `json(limit?)` and `form(limit?)`. Limits are non-negative byte limits applied to the body already acquired through the CP11 reader. `form` requires the exact `application/x-www-form-urlencoded` media type, uses the query decoder, returns `http_values`, and accepts at most 1024 pairs; media-type parameters and multipart are not supported. Helpers may be called repeatedly on a buffered request without consuming another transport body. They raise `HttpError` in request-stream handlers, where `http_request_body` remains the only body consumer. `http_redirect(location, status?)` creates an empty redirect, validates safe ASCII Location metadata and percent escapes, and accepts only 301, 302, 303, 307 or 308; it is not a URL authorization or canonicalization API.

Plaintext premature EOF receives 400. A TLS connection truncated below the HTTP layer always fails closed without handler dispatch; when the TLS record channel is already broken, the server may close without attempting an HTTP error response.

Buffered, static, error and streaming responses pass through one response-head validator and state machine. Handler header names must be non-empty HTTP tokens; values reject CR, LF, NUL, DEL and controls other than horizontal tab. Content types additionally require media-type `type/subtype` syntax with valid token or quoted parameters. Header names are compared case-insensitively, duplicates are rejected, and handlers cannot supply `Content-Type`, `Content-Length`, `Transfer-Encoding`, `Connection` or `Set-Cookie` through the generic map. Those fields belong to the transport or their dedicated safe representation. Handler statuses must be final response codes from 200 through 599; 204, 205 and 304 reject non-empty bodies, and 204/304 omit Content-Length as required. Invalid metadata produces a safe 500 without reflecting the invalid value.

## Streaming responses

`get_stream` and `post_stream` retain the existing buffered request while supplying an `http_response_writer` for incremental output:

```strut
app.get_stream("/events", (http_request request, http_response_writer response) => {
    response.status(200);
    response.content_type("application/octet-stream");
    response.header("X-Source", "live");
    response.write_bytes(first);
    response.flush();
    response.write_bytes(second);
    response.finish();
});
```

The writer moves from uncommitted to committed to finished, with an internal aborted state for transport or handler failure. `status`, `header`, `content_type` and `content_length` are available only before commitment. The first write or flush commits metadata. `finish` is idempotent, flush after finish is a no-op, and writes or metadata mutation after finish/commit raise `NetworkError`. Writer copies share one request-scoped state and become unusable when the handler returns.

`content_length` declares an exact transport-owned length and finish fails if the number of bytes differs. Without a declared length, HTTP/1.1 uses transport-generated chunked coding and HTTP/1.0 uses connection-close delimitation. Chunk sizes and the single terminal chunk are never application-controlled. Writes are synchronous and apply socket backpressure without an unbounded response buffer. A handler failure before commitment produces the fixed safe 500; after commitment the connection closes without attempting a second response. Plaintext and TLS writers use the same framing state machine, while TLS retains bounded native write sizing and socket-operation pinning.

`http_serve_file(request, writer, path, content_type?)` streams an explicit filesystem path through that writer in bounded 64 KiB reads. It infers a conservative media type when none is supplied, emits `Accept-Ranges: bytes`, and uses an exact content length. GET supports one `bytes=first-last`, `bytes=first-`, or `bytes=-suffix` range: a satisfiable range returns 206 with `Content-Range`, a syntactically valid but unsatisfiable range returns an empty 416 with `Content-Range: bytes */size`, and malformed, unknown-unit, or multipart ranges are ignored. End offsets and oversized suffixes clamp to the opened file size. HEAD ignores Range and emits the full representation metadata without reading the file body. Open, size, seek, and read failures raise `FilesystemError`; cancellation and writer failures raise `NetworkError`.

The helper accepts a valid UTF-8 path without NUL to an application-owned regular file, not a URL root or mount. It performs no URL decoding, traversal cleanup, root containment, symlink policy, authorization, cache validation, compression, or platform path normalization. Applications must derive and authorize explicit paths before calling it. The regular-file check precedes open, so concurrently replacing an authorized path is outside this helper's contract; use immutable application-owned directories rather than treating it as a hardened filesystem sandbox. The file is opened once and its initial size defines the response; a later short read closes a committed response rather than emitting a second response.

`http_write_ndjson(request, writer, value)` uses JSONIC's canonical validation to require finite numbers, UTF-8 strings/keys, exact number spellings, and at most 512 nested levels. It serializes one value with JSONIC's compact serializer, appends exactly one LF, writes it through the same response writer, and flushes before returning. Invalid JSON values raise `HttpError` before commitment. The first call selects `application/x-ndjson`; later calls require that committed media type. Literal newlines and controls inside JSON strings remain JSON escapes and cannot create extra records. One record is materialized at a time, so memory is bounded by the largest individual record rather than the complete stream. Empty streams must set `writer.content_type("application/x-ndjson")` explicitly because no record helper runs.

NDJSON has no implicit aggregate Content-Length. HTTP/1.1 therefore uses the writer's internal chunked framing, while HTTP/1.0 remains close-delimited; HTTP chunks are transport details and do not define record boundaries. The helper checks `request.cancellation` before and after serialization, and synchronous write/flush observes transport backpressure. Cancellation and transport failure raise `NetworkError`. A failure before the first record can still produce the safe fixed 500; a failure after commitment closes the connection, so consumers must treat a final unterminated or truncated record as incomplete and never as a second response.

## Connection persistence and cancellation

HTTP/1.1 connections persist by default unless either side selects `Connection: close`. HTTP/1.0 closes by default and persists only when the request contains `Connection: keep-alive` and the response is self-delimited. Connection tokens are parsed case-insensitively as comma-separated tokens. Requests are processed sequentially in wire order, including pipelined requests; handlers do not execute concurrently on one connection. Bytes read beyond a validated fixed-length or chunked body are retained for the next request rather than reparsed as body data. Reuse requires validated body EOF and a successfully finished self-delimited response. Unread bodies, malformed framing, transport failures, close-delimited output, explicit close, timeout and shutdown terminate reuse. Idle persistent reads use `idle_timeout_ms`.

HEAD uses the registered GET route and emits the corresponding response metadata while suppressing body bytes. Upgrade requests are detected at the connection-ownership boundary but currently receive 501; no public upgraded-stream ownership is exposed yet.

Every dispatched `http_request` contains a read-only `cancellation_token cancellation`. The server creates a distinct state for every request on a persistent connection and cancels it when that request lifetime ends, including normal handler completion, handler or framing failure, transport failure observed by request I/O, response failure, timeout, connection abort and server shutdown. Handler-created Strut threads inherit the request execution context, so joined child work can observe the same shutdown rules. Cancellation is cooperative: a CPU-only handler that performs no transport operation is not promised immediate notification of a peer disconnect. Passing `request.cancellation` to `process` cancels blocked process-pipe I/O under the existing process contract; it does not terminate the child.

For signal-driven services, start `listen` on a Strut thread, call `wait_for_shutdown_signal()`, then `stop()` and join the listener thread. The runtime signal handler only records the event; ordinary Strut code runs outside raw OS signal context.
