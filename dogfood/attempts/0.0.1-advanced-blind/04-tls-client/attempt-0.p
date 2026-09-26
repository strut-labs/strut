function main() -> int : TlsError {
    stream := tls_connect("example.com", 443);
    stream.close();
    return 0;
}
