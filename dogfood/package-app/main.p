include "config.h";
include <vector>;

function main() -> void : (ExecError, SqliteError) {
    db := sqlite_open(database_path());
    db.exec("CREATE TABLE IF NOT EXISTS values_table(value INTEGER)");
    db.close();
    result := exec("printf", ["package-ok"]);
    print(result.stdout);
    return;
}
