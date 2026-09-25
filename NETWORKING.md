# Networking substrate

The core runtime owns only the transport primitives higher-level official packages need: TCP connect/listen/accept, byte-string reads/writes, explicit close, executor-backed async connect/accept, and `NetworkError`. HTTP/TLS policy stays in official packages rather than expanding the language core.

```strut
function main() -> void : NetworkError {
    listener := tcp_listen("127.0.0.1", 8080);
    socket := listener.accept();
    request := socket.read(4096);
    socket.write("ok");
    socket.close();
    listener.close();
    return;
}
```

`tcp_connect_async(host, port)` and `listener.accept_async()` return futures and use Strut's shared multithreaded executor. Socket handles are shared runtime handles; `close()` closes the underlying endpoint for all copies.
