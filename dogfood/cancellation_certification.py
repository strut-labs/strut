#!/usr/bin/env python3
"""Certify unified cancellation ownership, observation and thread safety."""

from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    waiter_count = 32
    canceller_count = 8
    waiter_declarations = "\n".join(
        f"    waiter_{index} := thread(() => {{ token_copy.wait(); observed.send(token_copy.cancelled()); }});"
        for index in range(waiter_count)
    )
    canceller_declarations = "\n".join(
        f"    canceller_{index} := thread(() => {{ source_copy.cancel(); }});"
        for index in range(canceller_count)
    )
    receives = "\n".join(
        f"    if (!(observed.receive() ?? false)) {{ return {10 + index}; }}"
        for index in range(waiter_count)
    )
    joins = "\n".join(
        [f"    waiter_{index}.join();" for index in range(waiter_count)]
        + [f"    canceller_{index}.join();" for index in range(canceller_count)]
    )
    source = f"""function detached_token() -> cancellation_token {{
    cancellation_source local;
    return local.token();
}}

function main() -> int : ThreadError {{
    cancellation_token independent;
    if (independent.cancelled()) {{ return 1; }}
    cancellation_token detached := detached_token();
    if (detached.cancelled()) {{ return 2; }}

    cancellation_source source;
    cancellation_source source_copy := source;
    cancellation_token token := source.token();
    cancellation_token token_copy := token;
    channel<bool> observed;
{waiter_declarations}
{canceller_declarations}
{receives}
{joins}
    source.cancel();
    token.wait();
    try {{
        token.throw_if_cancelled();
    }} catch (CancellationError caught) {{
        print("cancellation certification passed");
        return 0;
    }}
    return 100;
}}
"""
    with tempfile.TemporaryDirectory(prefix="strut-cancellation-") as temporary:
        root = Path(temporary)
        program = root / "cancellation.p"
        executable = root / ("cancellation.exe" if sys.platform == "win32" else "cancellation")
        program.write_text(source, encoding="utf-8")
        subprocess.run([compiler, program, "-o", executable], cwd=root, check=True)
        for iteration in range(50):
            result = subprocess.run([executable], cwd=root, text=True, capture_output=True, timeout=15, check=False)
            if result.returncode != 0 or result.stdout != "cancellation certification passed\n" or result.stderr:
                raise RuntimeError(
                    f"iteration {iteration} failed: exit={result.returncode} stdout={result.stdout!r} stderr={result.stderr!r}"
                )
    print("Cancellation certification: 50 cycles, 32 waiters, 8 concurrent cancellers, copies, lifetime and checked errors passed")


if __name__ == "__main__":
    main()
