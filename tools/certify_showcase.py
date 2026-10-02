#!/usr/bin/env python3
"""Compile and run the canonical public showcase examples.

Examples are part of the public contract because both humans and agents copy
them directly, so they must be certified independently of the dogfood suite.
Each example is compiled from its own directory so that relative
`embed_file`/`embed_dir` and `public/` asset paths resolve as documented.
"""
from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "examples" / "showcase.json"


def run(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, text=True, capture_output=True, timeout=180, check=False)


def certify(compiler: Path, case: dict) -> list[str]:
    failures: list[str] = []
    source = ROOT / "examples" / case["source"]
    if not source.exists():
        return [f"showcase {case['name']}: source missing: {source}"]
    cwd = ROOT / "examples" / case.get("cwd", ".")
    artifact = cwd / f".strut-showcase-{case['name']}"
    if sys.platform == "win32":
        artifact = artifact.with_suffix(".exe")
    proc = run([str(compiler), str(source), "-o", str(artifact)], cwd)
    if proc.returncode != 0:
        failures.append(f"showcase {case['name']}: compile failed ({proc.returncode}): {proc.stderr}")
        return failures
    if case.get("mode") == "run":
        executed = run([str(artifact)], cwd)
        if executed.returncode != case.get("exit", 0):
            failures.append(f"showcase {case['name']}: exit {executed.returncode} != {case.get('exit', 0)}: {executed.stderr}")
    artifact.unlink(missing_ok=True)
    return failures


def main() -> int:
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build" / "strut").resolve()
    if not compiler.exists():
        raise SystemExit(f"compiler not found: {compiler}")
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    failures: list[str] = []
    for case in manifest["examples"]:
        failures.extend(certify(compiler, case))
    if failures:
        for failure in failures:
            print(f"FAIL {failure}", file=sys.stderr)
        return 1
    modes = [case["mode"] for case in manifest["examples"]]
    print(f"showcase examples certified: {modes.count('run')} run, {modes.count('compile')} compile-only")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())