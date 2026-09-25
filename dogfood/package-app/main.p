include <http>;
include <sqlite>;
include <tls>;
include <system>;
function main() -> void : (ExecError, SqliteError) {
    db := package_sqlite_open("package-dogfood.db");
    db.exec("CREATE TABLE IF NOT EXISTS values_table(value INTEGER)");
    db.close();
    text := run_capture("printf", ["package-ok"]);
    print(text);
    return;
}
