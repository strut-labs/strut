#!/usr/bin/env python3
"""Controlled common-mistake checks for actionable, Strut-led diagnostics."""
from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


CASES = [
    ("missing-module", "function main() -> int { map<string,int> values; return 0; }", ["requires standard module <map>", "help: add `include <map>;`"]),
    ("missing-checked-error", "function load() -> int : IOError { throw IOError(\"x\"); } function main() -> int { return load(); }", ["may throw checked error IOError", "try`/`catch", "function signature"]),
    ("wrong-pointer-operation", "function main() -> int { int* value := new(1); raw_ptr<int> raw := ptr(value); return 0; }", ["ptr(...) requires unsafe block"]),
    ("invalid-generic-type", "include <map>; function main() -> int { map<json,int> values; return 0; }", ["map key type 'json' is not hashable"]),
]


def invoke(compiler: Path, source: Path, env: dict[str, str] | None = None, check: bool = True) -> subprocess.CompletedProcess[str]:
    args = [str(compiler)]
    if check:
        args.append("--check")
    args.append(str(source))
    return subprocess.run(args, cwd=source.parent, env=env, text=True, capture_output=True, timeout=90, check=False)


def main() -> int:
    compiler = Path(sys.argv[1]).resolve()
    failures: list[str] = []
    with tempfile.TemporaryDirectory(prefix="strut-diagnostics-") as raw:
        root = Path(raw)
        for name, text, needles in CASES:
            source = root / f"{name}.p"
            source.write_text(text + "\n", encoding="utf-8")
            result = invoke(compiler, source)
            if result.returncode == 0 or any(needle not in result.stderr for needle in needles):
                failures.append(f"{name}: {result.stderr!r}")

        package = root / "package"
        package.mkdir()
        (package / "main.p").write_text("include <missing>;\nfunction main() -> int { return 0; }\n", encoding="utf-8")
        (package / "strut.json").write_text(json.dumps({"name":"diagnostic","version":"0.0.1","entry":"main.p","dependencies":{}}), encoding="utf-8")
        result = invoke(compiler, package / "main.p")
        if result.returncode == 0 or "package graph is not locked" not in result.stderr or "run `strut install`" not in result.stderr:
            failures.append(f"package-resolution: {result.stderr!r}")

        native = root / "native.p"
        native.write_text("function main() -> int : HttpError { response := http_get(\"http://127.0.0.1/\"); return 0; }\n", encoding="utf-8")
        env = os.environ.copy()
        env["CXX"] = "strut-deliberately-missing-cxx"
        result = invoke(compiler, native, env=env, check=False)
        if result.returncode == 0 or "required native library curl" not in result.stderr or "native toolchain output above is secondary detail" not in result.stderr:
            failures.append(f"native-dependency: {result.stderr!r}")

    if failures:
        for failure in failures:
            print("FAIL " + failure, file=sys.stderr)
        return 1
    print("diagnostic dogfood: 6 common mistakes actionable in one compiler cycle")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
