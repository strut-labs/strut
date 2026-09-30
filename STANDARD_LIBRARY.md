# Strut Standard Library Modules

Standard-library facilities are explicitly enabled with Strut module includes such as `include <vector>;`. These names are Strut modules, not direct requests to include C++ headers; the native backend is free to implement them differently.

## Collections

| Strut type | Current native implementation | Notes |
| --- | --- | --- |
| `vector<T>` / `T[]` | `std::vector<T>` | contiguous dynamic array |
| `deque<T>` | `std::deque<T>` | efficient insertion/removal at both ends |
| `list<T>` | `std::list<T>` | doubly-linked list |
| `map<K,V>` | `std::unordered_map<K,V>` | average O(1) lookup, no iteration-order guarantee |
| `set<T>` | `std::unordered_set<T>` | average O(1) lookup, no iteration-order guarantee |
| `ordered_map<K,V>` | `std::map<K,V>` | sorted iteration, O(log n) lookup |
| `ordered_set<T>` | `std::set<T>` | sorted iteration, O(log n) lookup |
| `queue<T>` | `std::queue<T>` | FIFO |
| `stack<T>` | `std::stack<T>` | LIFO |
| `priority_queue<T>` | `std::priority_queue<T>` | max-first by default |
| `priority_queue<T,min>` | priority queue using `greater<T>` | min-first |

Modules are named after the Strut surface type: `vector`, `deque`, `list`, `map`, `set`, `ordered_map`, `ordered_set`, `queue`, `stack`, `priority_queue`, and `tuple`.

## Bytes

`bytes` is the core owned binary value. `bytes()` creates an empty value and `bytes(size)` creates a zero-filled value. A contextually typed literal such as `bytes packet := [0, 127, 255];` accepts `uint_8` elements. Values deep-copy on assignment, compare by contents, and support mutable integer indexing, `length()`, `empty()`, and the half-open copying operation `slice(begin, end)`.

Index and slice bounds are checked at runtime. Lengths and indexes use `int_64`; the addressable limit is the smaller of the platform container limit and `int_64` maximum. `bytes.from_string(text)` and `value.to_string()` are explicit, lossless copies of string code units. They do not validate, decode, or imply UTF-8, and there is no implicit string/bytes conversion.

## Cryptography and binary encoding

Enable cryptographic primitives with `include <crypto>;` and Base64 encoding with `include <encoding>;`. These APIs are binary-first: applications convert text explicitly with `bytes.from_string` when text code units are the intended input.

```strut
include <crypto>;
include <encoding>;

function main() -> void : (CryptoError, EncodingError) {
    bytes nonce := secure_random_bytes(32);
    bytes digest := sha256(nonce);
    bytes authenticator := hmac_sha256(bytes.from_string("key"), digest);
    string token := base64url_encode(authenticator);
    bytes decoded := base64url_decode(token);
    print(constant_time_equal(authenticator, decoded));
    return;
}
```

`secure_random_bytes(count)` uses OpenSSL's cryptographically secure random generator and has no weak fallback. A zero count returns empty bytes; negative or unrepresentable counts raise `CryptoError`. `sha256(data)` and `hmac_sha256(key, data)` return raw 32-byte values and may raise `CryptoError` on native cryptographic failure.

`base64_encode` emits canonical padded RFC 4648 Base64. `base64url_encode` emits the URL-safe alphabet without padding. Their decoders reject whitespace, mixed alphabets, misplaced or noncanonical padding, noncanonical trailing bits, and malformed lengths through `EncodingError`.

`constant_time_equal(left, right)` compares equal-length contents with OpenSSL's constant-time comparison. Length is not secret and unequal lengths return false immediately. SHA-1 is not a public API.

## Binary streams

The existing `istream` and `ostream` types are the generic input and output contracts; there is no second reader/writer type family. `ifstream` and `ofstream` implement those contracts, while retaining their existing text and formatted operations. Process `in`, `out`, and `err` pipes expose the same binary method semantics structurally without becoming nominal file/console stream subtypes.

`read_bytes(max_bytes)` returns at most the requested bytes. A zero-size read returns empty bytes without changing EOF. An empty result denotes EOF only when `eof()` is also true; repeated reads after observed EOF remain empty. `read_all_bytes(limit?)` reads to EOF and treats its optional non-negative limit as a hard maximum. Reads after close raise `StreamError`, or `ExecError` for process pipes.

`write_bytes(value)` has complete-write-or-error semantics, including internal retries for partial native writes. It does not flush implicitly. `flush()` reports native flush failures, while flushing after close is a defined no-op. `close()` is idempotent; later writes fail. File streams must be opened with `binary=true` when byte-exact behavior is required on platforms with text-mode translation.

## Cancellation

`cancellation_source` owns the authority to request cancellation. `source.token()` returns a cheap copyable `cancellation_token` that observes the same shared state. Sources are also copyable; all copies retain cancellation authority over that state. Destroying a source does not cancel, tokens remain valid after every source is destroyed, and `cancel()` is one-way, thread-safe, and idempotent.

`token.cancelled()` performs non-blocking observation. `token.wait()` sleeps without busy-spinning until cancellation is requested. `token.throw_if_cancelled()` raises the checked `CancellationError`. Tokens cannot reset or request cancellation. The same terminal state supports explicit sources and runtime-owned lifetime producers without exposing subsystem-specific token types.

Native runtime facilities may subscribe to a token to wake blocking operations. Subscription is intentionally not public: registration cannot miss concurrent cancellation, removal waits for an in-flight callback, and callbacks run without the cancellation-state lock held.

`process(program, args, token)` binds a copied token to the new process's `in`, `out`, and `err` handles. The two-argument form remains available and uses an independent token that is never cancelled. Existing stream methods do not take token parameters. Cancelling the bound token wakes blocked process-pipe reads and writes and raises `ExecError("process I/O cancelled", 125)`; it does not terminate or wait for the child.

## Pseudo-terminals

`pty_spawn(program, args, options?, token?) -> pty : PtyError` starts an arbitrary executable directly, without an implicit shell. The four overloads match `process`: `(program, args)`, `(program, args, options)`, `(program, args, token)`, and `(program, args, options, token)`. Options are a JSON object with `cwd`, an `env` object whose values are strings, and positive integer `rows` and `columns`; the initial size defaults to 24 rows by 80 columns. Environment entries overlay the inherited environment, `PATH` lookup is completed in the parent before launch, and supported platforms apply `cwd` atomically through their native `posix_spawn` chdir file action.

The binary-only handle exposes `read_bytes(int_64) -> bytes : PtyError`, `write_bytes(bytes) -> void : PtyError`, `eof() -> bool`, `wait() -> int : PtyError`, `running() -> bool`, `exit_code() -> int`, and idempotent `close() -> void`. Standard input, output, and error share the terminal stream. Strut does not normalize text, parse escapes, or expose the native master descriptor. EOF includes the POSIX `EIO`/hangup forms commonly reported after slave closure. Writes complete fully or raise an error.

`pty` is a copyable shared-state handle. Copies refer to one master descriptor, session leader, EOF state, and bound cancellation token; `close()` through any copy closes the shared handle and performs bounded cleanup. Closing the master lets the kernel deliver terminal hangup to its current foreground process group without userspace retaining or signalling a numeric foreground PGID. TERM/KILL escalation is limited to the session leader's original process group and occurs only while WNOWAIT ordering pins the child identity. POSIX has no operation that signals every process in a session by SID: background process groups outside the leader group, and deliberately detached process groups, are not contained. One reader and one writer may be active concurrently; a second operation in the same direction raises `PtyError`. Cancellation wakes a blocked read or write and raises `PtyError("PTY I/O cancelled", 125)` without adding a PTY-specific token type.

P7 supports Linux with glibc 2.34 or newer, where the runtime has `POSIX_SPAWN_SETSID`, `posix_spawn_file_actions_addclosefrom_np`, and `posix_spawn_file_actions_addchdir_np`, plus macOS with its native equivalents. Launch creates the session, opens the slave as an ordered spawn file action, closes unrelated descriptors with the native close-from action, applies optional `cwd` with the native chdir action, merges child stdio, and keeps master I/O nonblocking with operation-local poll wakeups. Older glibc versions and other POSIX libcs fail deterministically, including calls without `cwd`; PTY launch does not use descriptor enumeration or post-fork child code. One lifecycle owner performs WNOWAIT identity probes and native reaping; blocking `wait()` does not hold the lifecycle mutex, concurrent `close()` wakes that owner, and later waits use the cached status or cached deterministic failure. Applications must not reap PTY children through an external SIGCHLD handler; if ownership is already lost, the handle closes safely, reports wait failure where applicable, and never signals the potentially reusable PID. The `pty` runtime slice depends on process ownership, cancellation, and bytes, but has no HTTP, WebSocket, curl, OpenSSL, SQLite, or third-party terminal dependency. Windows compiles a warning-clean API stub that raises `PtyError("PTY unsupported on Windows in P7")`; ConPTY is deferred to P9. Resize and public signal delivery are deferred to P8.

Native completion, including EOF, wins when it is observed in the same wake cycle as cancellation. A token already cancelled when an operation starts fails before I/O. If close and cancellation are both pending when a blocked operation resumes, cancellation wins; a blocked operation interrupted only by close raises `ExecError`. The existing text `read()` compatibility behavior still returns an empty string when invoked after close, while `read_bytes()` raises `ExecError`. Process pipes do not currently expose a timeout, and cancellation is not reported as timeout, EOF, peer close, or another I/O failure.

Each dispatched `http_request` has a read-only `cancellation_token cancellation`. It is fresh for that request, including consecutive requests on one persistent connection, and is cancelled when the request lifetime ends normally or through handler, framing, transport, response, timeout, abort or shutdown failure. Peer disconnect notification is cooperative and may be delayed for CPU-only handlers that perform no transport operation. Binding this token to `process` cancels process-pipe I/O but does not terminate the child.

Outbound `http_request_stream` accepts the same token explicitly. Pre-cancellation and cancellation observed during progress raise `HttpError` code `-103`; no second HTTP-specific token type exists. Its upload and download callbacks exchange owned binary `bytes` chunks synchronously, so callback return is the backpressure boundary. The async form preserves the same callbacks and token while returning `future<http_response_head>`.

`http_values` is the small repeated-value representation used by decoded query parameters, request cookies and URL-encoded forms. `get(name)` returns the first value or null, `values(name)` returns every value in wire order, and `has(name)` tests presence. `request.query` remains the raw last-value map for compatibility. Buffered requests add bounded `text`, `json` and `form` helpers; forms accept at most 1024 pairs, while streaming handlers must continue through `http_request_body` and cannot create a second buffered body.

`http_cookie(name, value)` creates a structured response cookie with writable `path`, `domain`, `max_age`, `expires`, `secure`, `http_only` and `same_site` attributes. Path is absolute, Domain follows DNS label syntax, Expires is IMF-fixdate, SameSite accepts Strict, Lax or None, and None requires Secure. Non-positive Max-Age requests immediate expiry. Buffered responses carry an ordered `cookies` array and streaming writers append cookies with `cookie(value)`. Generic response headers cannot emit `Set-Cookie`; this dedicated path validates and serializes each cookie as its own field.

## HTTP request body

`http_request_body` is the request-scoped binary reader supplied by `http_server.get_request_stream` and `post_request_stream`. It mirrors the input-stream contract with `read_bytes(max_bytes)`, `read_all_bytes(limit?)`, `eof()` and idempotent `close()`, using `NetworkError` for malformed framing, transport failure and invalid operations.

Fixed-length reads expose exactly the declared payload. HTTP/1.1 chunked reads expose decoded bytes without chunk boundaries, enforce body and framing-overhead limits, and reject malformed sizes, terminators, unsupported trailers and truncation. Reader copies share one consumption position; concurrent reads are rejected, and escaped active reads are interrupted before the request transport is released. A request-stream handler must consume or close the body before committing its response; commitment during an active read and reads after commitment fail with `NetworkError`. Buffered `request.body` is an adapter over the same reader rather than a second body path.

## HTTP response writer

`http_response_writer` is the request-scoped binary-capable output handle used by `http_server.get_stream` and `post_stream`. It follows the stream vocabulary with `write`, `write_bytes`, `flush` and `finish`, while adding precommit `status`, `header`, `cookie`, `content_type` and transport-owned `content_length` configuration. Operations that can fail raise `NetworkError`.

The first write or flush commits metadata. Metadata cannot change afterward, writes after finish fail, and finish is idempotent. Known-length writes must exactly match the declaration. Unknown-length HTTP/1.1 output is chunked internally; HTTP/1.0 output is close-delimited. The handle becomes inactive when its handler returns and never exposes raw HTTP chunk framing. A persistent connection is reusable only after the request body reaches validated EOF and the writer finishes a self-delimited response.

`http_serve_file(request, writer, path, content_type?) -> void : (FilesystemError, NetworkError)` incrementally serves one explicit binary file through the same writer. It supports full responses, HEAD metadata, and one bounded byte range with 206/416 metadata. It is not a directory mount or path-authorization API; URL-to-path policy remains application-owned.

`http_write_ndjson(request, writer, value) -> void : (HttpError, NetworkError)` validates finite UTF-8 JSON through JSONIC's 512-level validation boundary, writes its compact representation followed by one LF, selects `application/x-ndjson`, checks request cancellation around serialization, and flushes the shared response writer before returning. Each call buffers only its record. Empty streams set the content type directly on the writer.

## WebSockets

`http_server.websocket(route, handler)` supplies a request-scoped `websocket`. Call `accept(subprotocol?)` before frame operations. `read() -> websocket_message?` returns a tagged message whose read-only `kind`, `text` and `data` fields distinguish UTF-8 text from opaque bytes. `read_text()` and `read_bytes()` retain one mismatched message and raise `WebSocketError` rather than losing it. Peer close produces null.

`write_text(string)`, `write_bytes(bytes)`, `ping(bytes?)` and `close(code?, reason?)` are synchronous and raise `WebSocketError` for protocol, state, limit or UTF-8 violations and `NetworkError` for transport failure. Server close rejects code 1010. `http_server.websocket_limits(frame_bytes, message_bytes)` changes the stopped-server defaults of 1 MiB per data frame and 4 MiB per message; the data-frame limit is at least 125 bytes and RFC control frames retain their independent 125-byte maximum. One reader is allowed; writes are serialized without an unbounded queue. TCP/TLS transport operations are conservative and serial: a write or close behind a blocked read is released by input, configured read timeout, or interruption. Handler teardown waits at most one second for a peer Close when it can own the parser, answering Ping meanwhile; an escaped reader is interrupted and drained instead.

## Filesystem

Enable with `include <filesystem>;`. Functions include `exists`, `is_file`, `is_dir`, `file_size`, `modified`, `make_dir`, `remove`, `remove_all`, `copy`, `move`, `touch`, `ls`, `walk`, `cwd`, `cd`, `absolute`, `canonical`, `parent`, `filename`, `extension`, `stem`, `join_path`, `read_file`, `read_bytes`, `write_file`, and `append_file`.

`remove` removes one file or an empty directory. `remove_all` recursively removes a tree. Both accept a scalar path or a vector/array of paths. `copy([a,b], dest)` and `move([a,b], dest)` place each source under the existing destination directory using its basename.

`read_file` performs a size-aware single-allocation bulk read. `read_bytes` returns `bytes`. `write_file` and `append_file` accept either text or bytes. Stream APIs remain available for incremental I/O.

Filesystem failures use the checked `FilesystemError` type.

## Tuples

Enable tuples with `include <tuple>;`. Tuple types are heterogeneous and fixed-size:

```strut
include <tuple>;
tuple<double,int> t := (2.0, 1);
tuple<int> one := (7,);
print(t[0]);
```

`(x)` remains ordinary grouping; a one-element tuple therefore uses the conventional trailing comma `(x,)`. Tuple indexing is compile-time and requires an integer literal, which keeps each access statically typed.

## Include spelling

Angle brackets are the canonical spelling for Strut standard-library modules and package dependencies: `include <vector>;`, `include <filesystem>;`, `include <sqlite>;`. This is familiar C/C++-style syntax but it does **not** mean "inject a C++ header". Quoted includes such as `include "mylib.h";` are local Strut/native source dependencies resolved relative to the including source file.

## Filesystem wildcards

The source operands of `copy`, `move`, `remove`, and `remove_all` accept `*`, `?`, and recursive `**` wildcards. Destinations are always literal paths. A wildcard `copy`/`move` requires an existing destination directory; no-match copy/move is an error, while no-match removal is a no-op. Vector forms may mix literal paths and patterns.

## Complexity summary

- `vector`: amortized O(1) push-back, O(1) indexing.
- `deque`: O(1) push/pop at either end, O(1) indexing.
- `list`: O(1) end insertion/removal, linear traversal.
- `map` / `set`: hash based; average O(1) lookup/insert, no iteration-order guarantee.
- `ordered_map` / `ordered_set`: ordered tree containers; O(log n) lookup/insert and sorted iteration.
- `queue` / `stack`: O(1) adaptor operations.
- `priority_queue`: O(log n) push/pop and O(1) top; max-first by default, `priority_queue<T,min>` for min-first.
- `tuple`: fixed-size heterogeneous product type; literal-index access is compile-time.
