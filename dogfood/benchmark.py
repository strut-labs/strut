#!/usr/bin/env python3
"""Run preserved AI-DX benchmark attempts without consulting compiler internals."""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
from pathlib import Path


def run(command: list[str], cwd: Path) -> dict[str, object]:
    completed = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    return {
        "command": command,
        "exit_status": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("compiler", type=Path)
    parser.add_argument("run", type=Path)
    parser.add_argument("--attempt", default="attempt-0.p")
    parser.add_argument("--write-results", action="store_true")
    parser.add_argument("--materialize-preserved-results", action="store_true")
    args = parser.parse_args()

    compiler = args.compiler.resolve()
    run_root = args.run.resolve()
    manifest = json.loads((run_root / "manifest.json").read_text())
    failures = 0
    summary: list[dict[str, object]] = []

    for task in manifest["tasks"]:
        task_root = run_root / task["id"]
        if args.materialize_preserved_results:
            observation = json.loads((task_root / "attempt-0-observed.json").read_text())
            preserved_result = dict(task["result"])
            preserved_result["attempt_0_observation"] = observation
            (task_root / "result.json").write_text(json.dumps(preserved_result, indent=2) + "\n")
            summary.append(observation)
            print(f"PRESERVED {task['id']}")
            continue
        source = task_root / args.attempt
        if not source.exists():
            source = task_root / task.get("source", "attempt-0.p")
        with tempfile.TemporaryDirectory(prefix=f"strut-ai-dx-{task['id']}-") as raw:
            temp = Path(raw)
            output = temp / ("program.exe" if task.get("windows") else "program")
            compile_result = run([str(compiler), str(source), "-o", str(output)], task_root)
            record: dict[str, object] = {
                "task": task["id"],
                "source": str(source.relative_to(run_root)),
                "compile": compile_result,
                "compiled": compile_result["exit_status"] == 0,
                "ran_correctly": False,
            }
            if record["compiled"] and task.get("execute", True):
                execution = run([str(output), *task.get("args", [])], task_root)
                expected = task.get("expected", {})
                record["execute"] = execution
                record["ran_correctly"] = (
                    execution["exit_status"] == expected.get("exit_status", 0)
                    and execution["stdout"] == expected.get("stdout", "")
                    and execution["stderr"] == expected.get("stderr", "")
                )
            elif record["compiled"]:
                record["ran_correctly"] = True
            if not record["ran_correctly"]:
                failures += 1
            summary.append(record)
            status = "PASS" if record["ran_correctly"] else "FAIL"
            print(f"{status} {task['id']}")
            if not record["compiled"]:
                print(str(compile_result["stderr"]).rstrip())
            if args.write_results:
                (task_root / f"{source.stem}-observed.json").write_text(
                    json.dumps(record, indent=2) + "\n"
                )
                if "result" in task:
                    preserved_result = dict(task["result"])
                    preserved_result["attempt_0_observation"] = record
                    (task_root / "result.json").write_text(
                        json.dumps(preserved_result, indent=2) + "\n"
                    )

    compiled = sum(bool(item["compiled"]) for item in summary)
    correct = sum(bool(item["ran_correctly"]) for item in summary)
    print(f"{compiled}/{len(summary)} compiled; {correct}/{len(summary)} correct")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
