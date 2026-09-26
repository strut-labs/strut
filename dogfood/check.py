#!/usr/bin/env python3
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [
    "dogfood/cli/file_stats.p",
    "dogfood/fs/source_index.p",
    "dogfood/json/transform.p",
    "dogfood/http/endpoint_check.p",
    "dogfood/service/concurrent_service.p",
    "dogfood/nift-info/main.p",
    "dogfood/web/app.p",
    "dogfood/package-app/main.p",
]


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, timeout=90, **kwargs)
    if result.returncode:
        raise RuntimeError(f"{' '.join(command)}\n{result.stdout}{result.stderr}")
    return result


def main() -> int:
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build" / "strut").resolve()
    if not compiler.exists():
        raise SystemExit(f"compiler not found: {compiler}")
    with tempfile.TemporaryDirectory(prefix="strut-dogfood-") as temp_string:
        temp = Path(temp_string)
        binaries: dict[str, Path] = {}
        for index, relative in enumerate(SOURCES):
            source = ROOT / relative
            output = temp / f"program-{index}"
            run([str(compiler), str(source), "-o", str(output)], cwd=ROOT)
            binaries[relative] = output

        source_env = os.environ.copy()
        source_env["STRUT_SOURCE_ROOT"] = str(ROOT / "dogfood")
        run([str(binaries["dogfood/fs/source_index.p"])], cwd=temp, env=source_env)
        run([str(binaries["dogfood/json/transform.p"])], cwd=temp)
        run([str(binaries["dogfood/service/concurrent_service.p"])], cwd=temp)
        package = run([str(binaries["dogfood/package-app/main.p"])], cwd=temp)
        if package.stdout != "package-ok\n":
            raise RuntimeError(f"unexpected package-app output: {package.stdout!r}")
    print(f"dogfood: {len(SOURCES)} programs compiled, 4 offline programs ran")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
