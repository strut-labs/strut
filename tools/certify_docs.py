#!/usr/bin/env python3
"""Compile canonical documentation examples and detect website snippet drift."""
from __future__ import annotations

import argparse
import html
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "examples" / "docs"
START = "<!-- strut-example:{name}:start -->"
END = "<!-- strut-example:{name}:end -->"


def run(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, text=True, capture_output=True, timeout=90, check=False)


def rendered_block(source: Path) -> str:
    text = source.read_text(encoding="utf-8").rstrip("\n")
    return '<pre><code class="language-strut">' + html.escape(text, quote=False) + "</code></pre>"


def sync_or_check(site: Path, case: dict, source: Path, sync: bool) -> list[str]:
    failures: list[str] = []
    block = rendered_block(source)
    for relative, marker in case.get("docs", []):
        page = site / relative
        if not page.exists():
            failures.append(f"{case['name']}: docs page missing: {page}")
            continue
        text = page.read_text(encoding="utf-8")
        start, end = START.format(name=marker), END.format(name=marker)
        left, separator, rest = text.partition(start)
        body, separator2, right = rest.partition(end)
        if not separator or not separator2:
            failures.append(f"{case['name']}: marker {marker!r} missing in {relative}")
            continue
        expected = "\n" + block + "\n"
        if body != expected:
            if sync:
                page.write_text(left + start + expected + end + right, encoding="utf-8")
            else:
                failures.append(f"{case['name']}: {relative} differs from {source.relative_to(ROOT)}; run tools/certify_docs.py --sync-website")
    return failures


def certify(compiler: Path, case: dict, source: Path) -> list[str]:
    failures: list[str] = []
    with tempfile.TemporaryDirectory(prefix="strut-docs-") as temp:
        artifact = Path(temp) / ("example.exe" if sys.platform == "win32" else "example")
        proc = run([str(compiler), str(source), "-o", str(artifact)], EXAMPLES)
        expected_error = case["mode"] == "expected-error"
        if expected_error:
            if proc.returncode == 0:
                return [f"{case['name']}: compilation unexpectedly succeeded"]
            for needle in case.get("stderr_contains", []):
                if needle not in proc.stderr:
                    failures.append(f"{case['name']}: diagnostic missing {needle!r}; stderr={proc.stderr!r}")
            if "native C++" in proc.stderr or "native linker" in proc.stderr:
                failures.append(f"{case['name']}: expected Strut diagnostic but reached native toolchain")
            return failures
        if proc.returncode != 0:
            return [f"{case['name']}: compile failed ({proc.returncode}): {proc.stderr}"]
        if case["mode"] == "run":
            executed = run([str(artifact), *case.get("args", [])], EXAMPLES)
            if executed.returncode != case.get("exit", 0):
                failures.append(f"{case['name']}: exit {executed.returncode} != {case.get('exit', 0)}")
            if executed.stdout != case.get("stdout", ""):
                failures.append(f"{case['name']}: stdout {executed.stdout!r} != {case.get('stdout', '')!r}")
            if executed.stderr != case.get("stderr", ""):
                failures.append(f"{case['name']}: stderr {executed.stderr!r} != {case.get('stderr', '')!r}")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("compiler", type=Path, nargs="?")
    parser.add_argument("--website-root", type=Path)
    parser.add_argument("--sync-website", action="store_true")
    args = parser.parse_args()
    if not args.compiler and not args.website_root:
        parser.error("provide a compiler, --website-root, or both")
    compiler = args.compiler.resolve() if args.compiler else None
    site = args.website_root.resolve() if args.website_root else None
    manifest = json.loads((EXAMPLES / "manifest.json").read_text(encoding="utf-8"))
    failures: list[str] = []
    counts = {"run": 0, "compile": 0, "expected-error": 0}
    for case in manifest["examples"]:
        source = EXAMPLES / case["source"]
        if not source.exists():
            failures.append(f"{case['name']}: source missing: {source}")
            continue
        if compiler:
            failures.extend(certify(compiler, case, source))
            counts[case["mode"]] += 1
        if site:
            failures.extend(sync_or_check(site, case, source, args.sync_website))
    if failures:
        for failure in failures:
            print(f"FAIL {failure}", file=sys.stderr)
        return 1
    print(f"docs examples certified: {counts['run']} executed, {counts['compile']} compile-only, {counts['expected-error']} expected-error")
    if site:
        print("website example regions synchronized" if args.sync_website else "website example regions match canonical sources")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
