function main(string command, string[] args) -> int : (NetworkError, TlsError) {
    if (args.length != 2) {
        println("usage: " + command + " <certificate.pem> <private-key.pem>");
        return 2;
    }

    app := http_server();
    app.get("/health", (http_request request) => {
        return http_text("secure");
    });
    app.listen_tls("127.0.0.1", 18443, args[0], args[1]);
    return 0;
}
