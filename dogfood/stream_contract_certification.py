#!/usr/bin/env python3
"""Certify binary stream contracts, file adapters and process-pipe adapters."""

from pathlib import Path
import subprocess
import sys
import tempfile


def compile_program(compiler, root, name, source):
    path = root / f"{name}.p"
    executable = root / (f"{name}.exe" if sys.platform == "win32" else name)
    path.write_text(source, encoding="utf-8")
    subprocess.run([compiler, path, "-o", executable], cwd=root, check=True)
    return executable


def escaped(value):
    return str(value).replace("\\", "\\\\").replace('"', '\\"')


def expect_failure(compiler, root, name, source, message):
    executable = compile_program(compiler, root, name, source)
    result = subprocess.run([executable], cwd=root, text=True, capture_output=True, check=False)
    if result.returncode == 0 or message not in result.stderr:
        raise RuntimeError(f"{name} did not fail as required: exit={result.returncode} stderr={result.stderr!r}")


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    with tempfile.TemporaryDirectory(prefix="strut-stream-contract-") as temporary:
        root = Path(temporary)
        helper = root / "binary-helper.py"
        helper.write_text(
            "import sys, time\n"
            "data = sys.stdin.buffer.read()\n"
            "for offset in range(0, len(data), 4096):\n"
            "    sys.stdout.buffer.write(data[offset:offset + 4096])\n"
            "    sys.stdout.buffer.flush()\n"
            "    time.sleep(0.001)\n",
            encoding="utf-8",
        )
        source = f"""function main() -> int : (StreamError, ExecError) {{
    ofstream empty_out := ofstream("empty.bin", true);
    empty_out.close();
    ifstream empty_in := ifstream("empty.bin", true);
    if (empty_in.eof()) {{ return 1; }}
    bytes no_data := empty_in.read_bytes(8);
    if (!no_data.empty() || !empty_in.eof()) {{ return 2; }}
    empty_in.close();

    bytes large := bytes(1048576);
    large[0] = 255;
    large[1048575] = 127;
    ofstream output := ofstream("large.bin", true);
    output.write_bytes(large);
    output.flush();
    output.close();
    output.close();
    output.flush();
    ifstream input := ifstream("large.bin", true);
    bytes prefix := input.read_bytes(4096);
    bytes suffix := input.read_all_bytes(1044480);
    if (prefix.length() != 4096 || suffix.length() != 1044480 || prefix[0] != 255 || suffix[1044479] != 127 || !input.eof()) {{ return 3; }}
    if (!input.read_bytes(1).empty()) {{ return 4; }}
    input.close();
    input.close();

    ofstream text_out := ofstream("text.txt");
    text_out.write("text compatibility");
    text_out.close();
    ifstream text_in := ifstream("text.txt");
    if (text_in.read_all() != "text compatibility" || !text_in.eof()) {{ return 5; }}
    text_in.close();

    bytes payload := bytes(1048576);
    payload[0] = 255;
    payload[1048575] = 127;
    child := process("{escaped(sys.executable)}", ["{escaped(helper)}"]);
    child.in.write_bytes(payload);
    child.in.flush();
    child.in.close();
    bytes first := child.out.read_bytes(65536);
    bytes rest := child.out.read_all_bytes(1048576);
    int code := child.wait();
    if (code != 0 || first.empty() || first.length() > 65536 || first.length() + rest.length() != 1048576 || first[0] != 255 || rest[rest.length() - 1] != 127) {{ return 6; }}
    child.out.close();
    print("stream certification passed");
    return 0;
}}
"""
        executable = compile_program(compiler, root, "streams", source)
        result = subprocess.run([executable], cwd=root, text=True, capture_output=True, check=False)
        if result.returncode != 0 or result.stdout != "stream certification passed\n":
            raise RuntimeError(f"stream behavior failed: exit={result.returncode} stdout={result.stdout!r} stderr={result.stderr!r}")

        expect_failure(
            compiler,
            root,
            "read-closed",
            'function main() -> void : StreamError { ifstream input := ifstream("empty.bin", true); input.close(); input.read_bytes(1); }',
            "input stream is not open",
        )
        expect_failure(
            compiler,
            root,
            "write-closed",
            'function main() -> void : StreamError { ofstream output := ofstream("closed.bin", true); output.close(); output.write_bytes([0]); }',
            "output stream is not open",
        )
        expect_failure(
            compiler,
            root,
            "read-limit",
            'function main() -> void : StreamError { ofstream output := ofstream("limit.bin", true); output.write_bytes([0, 1]); output.close(); ifstream input := ifstream("limit.bin", true); input.read_all_bytes(1); }',
            "byte limit exceeded",
        )
        expect_failure(
            compiler,
            root,
            "process-peer-closed",
            f'function main() -> void : ExecError {{ child := process("{escaped(sys.executable)}", ["-c", "pass"]); child.wait(); child.in.write_bytes([0]); }}',
            "process stdin write failed",
        )

    print("Stream certification: binary files, partial process reads, EOF, limits, close, flush and text compatibility passed")


if __name__ == "__main__":
    main()
