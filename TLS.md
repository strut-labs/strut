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

An explicit fixture or private CA bundle can be supplied without disabling verification:

```strut
response := http_get_ca("https://localhost:8443/health", "local-ca.pem");
```

## HTTPS servers

Server-side TLS uses the approved OpenSSL dependency and is selected only by `listen_tls`; plaintext HTTP does not link OpenSSL.

```strut
function main(string command, string[] args) -> int : (NetworkError, TlsError) {
    app := http_server();
    app.get("/health", (http_request request) => { return http_text("secure"); });
    app.listen_tls("127.0.0.1", 8443, args[0], args[1]);
    return 0;
}
```

The certificate argument is a PEM certificate chain and the key argument is a PEM private key. Both are loaded and checked for a match before the listener starts. Missing, unreadable, invalid or mismatched material produces `TlsError`; the server never falls back to plaintext. TLS 1.2 is the minimum protocol version. Certificate issuance, rotation and ACME are deliberately outside this API.
