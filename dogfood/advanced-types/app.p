error ValidationError {
    string message;
    int code;
}

error NotFoundError {
    string message;
    int code;
}

function first[T](T[] values) -> T : NotFoundError {
    if (values.length == 0) {
        throw NotFoundError { message: "no values", code: 404 };
    }
    return values[0];
}

function validate_name(string value) -> string : ValidationError {
    if (value.length == 0) {
        throw ValidationError { message: "name is required", code: 400 };
    }
    return value;
}

function main() -> int : (NetworkError, SqliteError) {
    db := sqlite_open(":memory:");
    db.exec("CREATE TABLE notes(id INTEGER PRIMARY KEY, text TEXT)");

    app := http_server();
    app.get("/notes", (http_request request) => {
        try {
            string[] names := [validate_name(request.path)];
            selected := first(names);
            return http_json_response({"selected": selected, "rows": db.query("SELECT * FROM notes")});
        } catch (ValidationError err) {
            return http_json_response({"error": err.message, "code": err.code});
        } catch (NotFoundError err) {
            return http_json_response({"error": err.message, "code": err.code});
        }
    });
    app.listen("127.0.0.1", 18089);
    return 0;
}
