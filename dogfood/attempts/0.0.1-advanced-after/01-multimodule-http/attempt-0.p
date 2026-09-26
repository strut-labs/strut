include "routes.h";
function main() -> int : NetworkError {
    app := http_server();
    install_routes(app);
    app.listen("127.0.0.1", 18080);
    return 0;
}
