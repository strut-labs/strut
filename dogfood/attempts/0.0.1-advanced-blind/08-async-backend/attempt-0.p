include <filesystem>;
async function inspect() -> string : (FilesystemError, ExecError, HttpError) {
    write_file("advanced.tmp", "ok");
    result := exec("printf", [read_file("advanced.tmp")]);
    response := await http_get_async("https://example.com");
    return result.stdout + response.status;
}
function main() -> int : (FilesystemError, ExecError, HttpError) { println(await inspect()); return 0; }
