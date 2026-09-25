function package_tls_connect(string host, int port) -> tls_stream : TlsError {
    return tls_connect(host, port);
}
