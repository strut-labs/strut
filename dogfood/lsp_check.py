#!/usr/bin/env python3
"""Exercise the standard LSP transport without an editor-specific client."""
import json
import pathlib
import subprocess
import sys


def frame(message):
    body = json.dumps(message, separators=(",", ":")).encode()
    return b"Content-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body


def main():
    compiler = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    source = """include <filesystem>;
function identity[T](T value) -> T { return value; }
function main() -> int : HttpError, SqliteError {
    string[] args := ["ok"];
    ptr<int> owner := new(1);
    payload := json {"ok": true};
    response := http_get("https://example.test");
    database := sqlite_open("app.db");
    files := ls(".");
    identity(args);
    return 0;
}
"""
    uri = "file:///tmp/strut-lsp-dogfood.p"
    messages = [
        {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}},
        {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {"textDocument": {"uri": uri, "text": source}}},
        {"jsonrpc": "2.0", "id": 2, "method": "textDocument/completion", "params": {"textDocument": {"uri": uri}, "position": {"line": 9, "character": 4}}},
        {"jsonrpc": "2.0", "id": 3, "method": "textDocument/signatureHelp", "params": {"textDocument": {"uri": uri}, "position": {"line": 6, "character": 24}}},
        {"jsonrpc": "2.0", "id": 4, "method": "textDocument/hover", "params": {"textDocument": {"uri": uri}, "position": {"line": 6, "character": 17}}},
        {"jsonrpc": "2.0", "id": 5, "method": "textDocument/documentSymbol", "params": {"textDocument": {"uri": uri}}},
        {"jsonrpc": "2.0", "id": 6, "method": "shutdown", "params": {}},
        {"jsonrpc": "2.0", "method": "exit", "params": {}},
    ]
    run = subprocess.run([str(compiler), "lsp"], input=b"".join(map(frame, messages)), stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
    output = run.stdout.decode(errors="replace")
    required = ["signatureHelpProvider", "identity", "http_get", "sqlite_open", "json", "new", "ls", "Checked errors: HttpError"]
    missing = [value for value in required if value not in output]
    if run.returncode or run.stderr or missing:
        print(run.stderr.decode(errors="replace"), file=sys.stderr)
        print("missing: " + ", ".join(missing), file=sys.stderr)
        return 1
    print("LSP dogfood: CLI, JSON, HTTP, SQLite, generics, pointers, and filesystem metadata available")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
