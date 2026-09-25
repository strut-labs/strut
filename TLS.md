# TLS package direction

Strut's official TLS client uses the pre-approved libcurl dependency. Certificate verification is on by default (`CURLOPT_SSL_VERIFYPEER=1`, `CURLOPT_SSL_VERIFYHOST=2`) and there is no ordinary "insecure" convenience API.

```strut
function main() -> void : TlsError {
    stream := tls_connect("example.com", 443);
    stream.write("...");
    bytes := stream.read(4096);
    stream.close();
    return;
}
```

Server-side TLS is not implemented in CP75. libcurl is a client library, and Strut will not silently embed OpenSSL or another server TLS implementation without maintainer approval. The HTTP server checkpoint therefore begins with plain HTTP; HTTPS server support remains a separate backend decision.
