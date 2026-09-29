#!/usr/bin/env python3
"""Certify HTTP query, form, cookie, redirect, and bounded body helpers."""

from pathlib import Path
import json
import socket
import ssl
import subprocess
import sys
import tempfile

from http_persistence_certification import HttpConnection
from http_response_stream_certification import available_port, compile_program, wait_until_listening


def source(port, tls=False):
    sixty_four = ", ".join(["cookie"] * 64)
    sixty_five = ", ".join(["cookie"] * 65)
    value_4094 = "a" * 4094
    value_4095 = "a" * 4095
    signature = (
        "function main(string command, string[] args) -> int : (NetworkError, TlsError)"
        if tls
        else "function main() -> void : NetworkError"
    )
    prefix = "if (args.length != 2) { return 2; }" if tls else ""
    listen = (
        f'app.listen_tls("127.0.0.1", {port}, args[0], args[1]);'
        if tls
        else f'app.listen("127.0.0.1", {port});'
    )
    suffix = "return 0;" if tls else ""
    return f'''{signature} {{
    {prefix}
    app := http_server();
    app_ref := ref(app);
    app.timeouts(1000, 1000, 1000, 500);
    app.limits(4096, 8192, 64, 8);
    app.get_stream("/inspect", (http_request request, http_response_writer writer) => {{
        tags := request.query_values.values("tag");
        sessions := request.cookies.values("session");
        empties := request.cookies.values("empty");
        first := request.cookies.get("session");
        if (first == null || !request.cookies.has("empty") || request.cookies.has("missing")) {{ writer.write("lookup-error"); return; }}
        writer.write(tags[0]); writer.write("|"); writer.write(tags[1]); writer.write("|");
        writer.write(request.query["tag"]); writer.write("|"); writer.write(request.query["empty"]); writer.write("|");
        writer.write(request.query["plus"]); writer.write("|"); writer.write(request.query["pct"]); writer.write("|");
        writer.write(sessions[0]); writer.write("|"); writer.write(sessions[1]); writer.write("|"); writer.write(empties[0]);
    }});
    app.get("/cookies", (http_request request) => {{
        session := http_cookie("warden_session", "abc123");
        session.path = "/";
        session.max_age = 3600;
        session.secure = true;
        session.http_only = true;
        session.same_site = "Strict";
        expires := http_cookie("theme", "dark");
        expires.path = "/";
        expires.expires = "Wed, 21 Oct 2030 07:28:00 GMT";
        response := http_text("cookies");
        response.cookies = [session, expires];
        return response;
    }});
    app.get_stream("/stream-cookie", (http_request request, http_response_writer writer) => {{
        value := http_cookie("stream", "yes");
        value.http_only = true;
        writer.cookie(value);
        writer.content_length(6);
        writer.write("stream");
    }});
    app.post_stream("/form", (http_request request, http_response_writer writer) => {{
        try {{
            form := request.form();
            items := form.values("item");
            if (!form.has("item") || form.has("missing")) {{ writer.write("lookup-error"); return; }}
            empty := form.values("empty"); bare := form.values("bare"); plus := form.values("plus"); pct := form.values("pct");
            writer.write(items[0]); writer.write("|"); writer.write(items[1]); writer.write("|"); writer.write(empty[0]); writer.write("|"); writer.write(bare[0]); writer.write("|"); writer.write(plus[0]); writer.write("|"); writer.write(pct[0]);
        }} catch (HttpError err) {{ writer.write("form-error"); }}
    }});
    app.post_stream("/form-limit", (http_request request, http_response_writer writer) => {{
        try {{ request.form(3); writer.write("unexpected"); }}
        catch (HttpError err) {{ writer.write("limited"); }}
    }});
    app.post("/json", (http_request request) => {{
        try {{ value := request.json(); return http_json_response(value); }}
        catch (HttpError err) {{ return http_text("json-error"); }}
    }});
    app.post("/text", (http_request request) => {{
        try {{ return http_text(request.text()); }} catch (HttpError err) {{ return http_text("text-error"); }}
    }});
    app.post_request_stream("/stream-conflict", (http_request request, http_request_body body, http_response_writer writer) => {{
        ignored := body.read_bytes(1);
        result := "unexpected";
        try {{ request.text(); }} catch (HttpError err) {{ result = "conflict"; }}
        body.close();
        writer.content_length(result.length());
        writer.write(result);
    }});
    app.get("/redirect", (http_request request) => {{
        try {{ return http_redirect("/target", 303); }}
        catch (HttpError err) {{ return http_text("redirect-error"); }}
    }});
    app.get("/bad-redirect", (http_request request) => {{
        try {{ return http_redirect("/bad path"); }}
        catch (HttpError err) {{ return http_text("redirect-rejected"); }}
    }});
    app.get("/bad-redirect-percent", (http_request request) => {{
        try {{ return http_redirect("/bad%2"); }}
        catch (HttpError err) {{ return http_text("redirect-rejected"); }}
    }});
    app.get("/bad-redirect-character", (http_request request) => {{
        try {{ return http_redirect("/bad<path>"); }}
        catch (HttpError err) {{ return http_text("redirect-rejected"); }}
    }});
    app.get("/bad-cookie", (http_request request) => {{
        cookie := http_cookie("bad name", "value");
        response := http_text("unsafe"); response.cookies = [cookie]; return response;
    }});
    app.get("/generic-cookie", (http_request request) => {{
        response := http_text("unsafe"); response.headers.insert("sEt-CoOkIe", "legacy=value"); return response;
    }});
    app.get("/bad-cookie-attribute", (http_request request) => {{
        cookie := http_cookie("valid", "value"); cookie.path = "relative";
        response := http_text("unsafe"); response.cookies = [cookie]; return response;
    }});
    app.get("/bad-cookie-expires", (http_request request) => {{
        cookie := http_cookie("valid", "value"); cookie.expires = "Wed, 31 Feb 2030 07:28:00 GMT";
        response := http_text("unsafe"); response.cookies = [cookie]; return response;
    }});
    app.get("/cookie-64", (http_request request) => {{
        cookie := http_cookie("valid", "value"); response := http_text("bounded"); response.cookies = [{sixty_four}]; return response;
    }});
    app.get("/cookie-65", (http_request request) => {{
        cookie := http_cookie("valid", "value"); response := http_text("unsafe"); response.cookies = [{sixty_five}]; return response;
    }});
    app.get("/cookie-4096", (http_request request) => {{
        cookie := http_cookie("x", "{value_4094}"); response := http_text("bounded"); response.cookies = [cookie]; return response;
    }});
    app.get("/cookie-4097", (http_request request) => {{
        cookie := http_cookie("x", "{value_4095}"); response := http_text("unsafe"); response.cookies = [cookie]; return response;
    }});
    app.get_stream("/bad-stream-cookie", (http_request request, http_response_writer writer) => {{
        cookie := http_cookie("bad name", "value"); writer.cookie(cookie); writer.write("unsafe");
    }});
    app.post_stream("/text-negative", (http_request request, http_response_writer writer) => {{
        try {{ request.text(-1); writer.write("unexpected"); }} catch (HttpError err) {{ writer.write("limited"); }}
    }});
    app.get("/health", (http_request request) => {{ return http_text("ok"); }});
    app.get("/cookie-count", (http_request request) => {{ return http_text("ok"); }});
    app.get("/stop", (http_request request) => {{ app_ref->stop(); return http_text("stopped"); }});
    {listen}
    {suffix}
}}
'''


def get_request(path, extra=""):
    return f"GET {path} HTTP/1.1\r\nHost: localhost\r\n{extra}\r\n".encode()


def post_request(path, body, content_type="application/x-www-form-urlencoded", chunked=False):
    if chunked:
        encoded = f"{len(body):x}\r\n".encode() + body + b"\r\n0\r\n\r\n"
        framing = "Transfer-Encoding: chunked\r\n"
    else:
        encoded = body
        framing = f"Content-Length: {len(body)}\r\n"
    return (
        f"POST {path} HTTP/1.1\r\nHost: localhost\r\nContent-Type: {content_type}\r\n{framing}\r\n".encode()
        + encoded
    )


def values(fields, name):
    return [value for field, value in fields if field == name.lower()]


def require_response(response, status, body):
    if response[0] != status or response[2] != body:
        raise RuntimeError(f"unexpected response: {response!r}")


def require_json(response, expected):
    if response[0] != 200 or json.loads(response[2]) != expected:
        raise RuntimeError(f"unexpected JSON response: {response!r}")


def exercise(opener):
    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(get_request(
            "/inspect?tag=one&tag=two&empty=&bare&plus=a+b&pct=a%26b",
            "Cookie: session=abc; session=def; empty=\r\n",
        ))
        require_response(connection.response(), 200, b"one|two|two||a+b|a%26b|abc|def|")

        connection.send(get_request("/cookies"))
        response = connection.response()
        require_response(response, 200, b"cookies")
        cookies = values(response[1], b"set-cookie")
        expected = [
            b"warden_session=abc123; Path=/; Max-Age=3600; Secure; HttpOnly; SameSite=Strict",
            b"theme=dark; Path=/; Expires=Wed, 21 Oct 2030 07:28:00 GMT",
        ]
        if cookies != expected:
            raise RuntimeError(f"unexpected Set-Cookie fields: {cookies!r}")

        form = b"item=one&item=two&empty=&bare&plus=a+b&pct=a%26b"
        connection.send(post_request("/form", form))
        require_response(connection.response(method=b"POST"), 200, b"one|two|||a b|a&b")

        connection.send(post_request("/json", b'{"name":"warden"}', "application/json"))
        require_json(connection.response(method=b"POST"), {"name": "warden"})

        connection.send(post_request("/text", b"plain", "text/plain"))
        require_response(connection.response(method=b"POST"), 200, b"plain")

        connection.send(get_request("/redirect"))
        redirect = connection.response()
        require_response(redirect, 303, b"")
        if values(redirect[1], b"location") != [b"/target"]:
            raise RuntimeError(f"unexpected redirect metadata: {redirect!r}")

        connection.send(get_request("/bad-redirect"))
        require_response(connection.response(), 200, b"redirect-rejected")
        connection.send(get_request("/bad-redirect-percent"))
        require_response(connection.response(), 200, b"redirect-rejected")
        connection.send(get_request("/bad-redirect-character"))
        require_response(connection.response(), 200, b"redirect-rejected")

    with opener() as raw:
        connection = HttpConnection(raw)
        form = b"item=one&item=two&empty=&bare&plus=a+b&pct=a%26b"
        connection.send(post_request("/form", form, chunked=True))
        require_response(connection.response(method=b"POST"), 200, b"one|two|||a b|a&b")
        connection.send(post_request("/form-limit", b"item=large"))
        require_response(connection.response(method=b"POST"), 200, b"limited")
        connection.send(post_request("/stream-conflict", b"abc"))
        require_response(connection.response(method=b"POST"), 200, b"conflict")

    with opener() as raw:
        connection = HttpConnection(raw)
        connection.send(get_request("/stream-cookie"))
        response = connection.response()
        require_response(response, 200, b"stream")
        if values(response[1], b"set-cookie") != [b"stream=yes; HttpOnly"]:
            raise RuntimeError(f"unexpected streaming cookie: {response!r}")


def request_once(port, payload):
    with socket.create_connection(("127.0.0.1", port), timeout=5) as raw:
        connection = HttpConnection(raw)
        connection.send(payload)
        return connection.response()


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    certificate = Path("tests/fixtures/tls/localhost-cert.pem").resolve()
    private_key = Path("tests/fixtures/tls/localhost-key.pem").resolve()
    with tempfile.TemporaryDirectory(prefix="strut-http-helpers-") as temporary:
        root = Path(temporary)
        port = available_port()
        executable = compile_program(str(compiler), root, "helpers", source(port))
        process = subprocess.Popen([executable], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            wait_until_listening(port, process)
            exercise(lambda: socket.create_connection(("127.0.0.1", port), timeout=5))
            for payload in (
                get_request("/inspect?bad=%00"),
                get_request("/inspect?bad=%0A"),
                get_request("/inspect?bad=%7f"),
                get_request("/inspect?bad=%"),
                get_request("/inspect", "Cookie: bad name=value\r\n"),
                get_request("/inspect", "Cookie: session =admin\r\n"),
                get_request("/inspect", "Cookie: session= admin\r\n"),
                get_request("/inspect", "Cookie: session=\"unterminated\r\n"),
                get_request("/inspect", "Cookie: session=abc;\r\n"),
                get_request("/inspect?" + "&".join([f"x{i}=v" for i in range(65)])),
            ):
                require_response(request_once(port, payload), 400, b"Bad Request")
            quoted = request_once(
                port,
                get_request(
                    "/inspect?tag=one&tag=two&empty=&bare&plus=a+b&pct=a%26b",
                    "Cookie: session=\"abc\"; session=def; empty=\r\n",
                ),
            )
            require_response(quoted, 200, b"one|two|two||a+b|a%26b|abc|def|")
            require_response(
                request_once(port, post_request("/form", b"item=one&item=%GG")),
                200,
                b"form-error",
            )
            require_response(
                request_once(port, post_request("/form", b"item=one&item=%0A")),
                200,
                b"form-error",
            )
            require_response(request_once(port, post_request("/json", b"{", "application/json")), 200, b"json-error")
            separator_heavy = b"&".join([b"x="] * 1025)
            require_response(request_once(port, post_request("/form", separator_heavy)), 200, b"form-error")
            require_response(request_once(port, post_request("/form", b"item=one", "text/plain")), 200, b"form-error")
            require_response(request_once(port, post_request("/form", b"item=one", "application/x-www-form-urlencoded; garbage")), 200, b"form-error")
            cookie_64_header = "; ".join([f"c{i}=v" for i in range(64)])
            cookie_65_header = "; ".join([f"c{i}=v" for i in range(65)])
            require_response(request_once(port, get_request("/cookie-count", f"Cookie: {cookie_64_header}\r\n")), 200, b"ok")
            require_response(request_once(port, get_request("/cookie-count", f"Cookie: {cookie_65_header}\r\n")), 400, b"Bad Request")
            require_response(request_once(port, post_request("/text-negative", b"x")), 200, b"limited")
            require_response(request_once(port, get_request("/bad-cookie")), 500, b"Internal Server Error")
            require_response(request_once(port, get_request("/bad-cookie-attribute")), 500, b"Internal Server Error")
            require_response(request_once(port, get_request("/bad-cookie-expires")), 500, b"Internal Server Error")
            require_response(request_once(port, get_request("/generic-cookie")), 500, b"Internal Server Error")
            cookie_64 = request_once(port, get_request("/cookie-64"))
            require_response(cookie_64, 200, b"bounded")
            if len(values(cookie_64[1], b"set-cookie")) != 64:
                raise RuntimeError("64 response cookies were not preserved")
            require_response(request_once(port, get_request("/cookie-65")), 500, b"Internal Server Error")
            cookie_4096 = request_once(port, get_request("/cookie-4096"))
            require_response(cookie_4096, 200, b"bounded")
            if len(values(cookie_4096[1], b"set-cookie")[0]) != 4096:
                raise RuntimeError("4096-byte cookie boundary was not preserved")
            require_response(request_once(port, get_request("/cookie-4097")), 500, b"Internal Server Error")
            require_response(request_once(port, get_request("/bad-stream-cookie")), 500, b"Internal Server Error")
            with socket.create_connection(("127.0.0.1", port), timeout=5) as raw:
                rejected = HttpConnection(raw)
                rejected.send(get_request("/bad-cookie") + get_request("/health"))
                require_response(rejected.response(), 500, b"Internal Server Error")
                rejected.expect_eof()
            require_response(request_once(port, get_request("/health")), 200, b"ok")
            request_once(port, get_request("/stop", "Connection: close\r\n"))
            process.wait(timeout=10)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()

        tls_port = available_port()
        tls_executable = compile_program(str(compiler), root, "helpers-tls", source(tls_port, tls=True))
        tls_process = subprocess.Popen(
            [tls_executable, certificate, private_key],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        context = ssl.create_default_context(cafile=str(certificate))
        try:
            wait_until_listening(tls_port, tls_process)
            exercise(lambda: context.wrap_socket(
                socket.create_connection(("127.0.0.1", tls_port), timeout=5),
                server_hostname="localhost",
            ))
            with context.wrap_socket(
                socket.create_connection(("127.0.0.1", tls_port), timeout=5),
                server_hostname="localhost",
            ) as raw:
                connection = HttpConnection(raw)
                connection.send(get_request("/stop", "Connection: close\r\n"))
                connection.response()
            tls_process.wait(timeout=10)
        finally:
            if tls_process.poll() is None:
                tls_process.kill()
                tls_process.wait()

    print("HTTP application helpers certification: repeated query/form values, cookies, redirects, body limits, persistence, and TLS passed")


if __name__ == "__main__":
    main()
