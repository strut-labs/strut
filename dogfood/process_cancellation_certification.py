#!/usr/bin/env python3
"""Certify cancellation of blocking process-pipe reads and writes."""

from pathlib import Path
import subprocess
import sys
import tempfile


def escaped(value):
    return str(value).replace("\\", "\\\\").replace('"', '\\"')


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    with tempfile.TemporaryDirectory(prefix="strut-process-cancellation-") as temporary:
        root = Path(temporary)
        helper = root / "process-helper.py"
        helper.write_text(
            "import sys, time\n"
            "mode = sys.argv[1]\n"
            "if mode == 'sleep': time.sleep(30)\n"
            "elif mode == 'delayed-output':\n"
            "    time.sleep(0.15)\n"
            "    sys.stdout.write('independent')\n"
            "    sys.stdout.flush()\n"
            "elif mode == 'output':\n"
            "    sys.stdout.write('complete')\n"
            "    sys.stdout.flush()\n",
            encoding="utf-8",
        )
        source = f'''function main() -> int : (ExecError, ThreadError, TimeError) {{
    cancellation_source read_source;
    child_read := new(process("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"], read_source.token()));
    channel<bool> read_result;
    read_worker := thread(() => {{
        try {{ child_read->out.read_bytes(1); read_result.send(false); }}
        catch (ExecError caught) {{ read_result.send(caught.code == 125 && caught.message == "process I/O cancelled"); }}
    }});
    sleep_ms(50);
    read_source.cancel();
    if (!(read_result.receive() ?? false)) {{ return 1; }}
    read_worker.join();
    child_read->terminate();
    child_read->wait();

    cancellation_source write_source;
    child_write := new(process("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"], write_source.token()));
    bytes payload := bytes(16777216);
    channel<bool> write_result;
    write_worker := thread(() => {{
        try {{ child_write->in.write_bytes(payload); write_result.send(false); }}
        catch (ExecError caught) {{ write_result.send(caught.code == 125 && caught.message == "process I/O cancelled"); }}
    }});
    sleep_ms(50);
    write_source.cancel();
    if (!(write_result.receive() ?? false)) {{ return 2; }}
    write_worker.join();
    child_write->terminate();
    child_write->wait();

    cancellation_source first_source;
    cancellation_source second_source;
    first := new(process("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"], first_source.token()));
    second := new(process("{escaped(sys.executable)}", ["{escaped(helper)}", "delayed-output"], second_source.token()));
    channel<bool> first_result;
    channel<bool> second_result;
    first_worker := thread(() => {{
        try {{ first->out.read_bytes(1); first_result.send(false); }}
        catch (ExecError caught) {{ first_result.send(caught.code == 125); }}
    }});
    second_worker := thread(() => {{ second_result.send(second->out.read_all() == "independent"); }});
    sleep_ms(50);
    first_source.cancel();
    if (!(first_result.receive() ?? false) || !(second_result.receive() ?? false)) {{ return 3; }}
    first_worker.join();
    second_worker.join();
    first->terminate();
    first->wait();
    if (second->wait() != 0 || second_source.token().cancelled()) {{ return 4; }}

    cancellation_source close_source;
    close_child := new(process("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"], close_source.token()));
    channel<bool> close_result;
    close_worker := thread(() => {{
        try {{ close_child->out.read_bytes(1); close_result.send(false); }}
        catch (ExecError caught) {{ close_result.send(caught.message == "process pipe is closed"); }}
    }});
    sleep_ms(50);
    close_child->out.close();
    if (!(close_result.receive() ?? false)) {{ return 5; }}
    close_worker.join();
    close_child->terminate();
    close_child->wait();

    cancellation_source completed_source;
    completed := process("{escaped(sys.executable)}", ["{escaped(helper)}", "output"], completed_source.token());
    if (completed.out.read_all() != "complete" || !completed.out.eof() || completed.wait() != 0) {{ return 6; }}
    completed_source.cancel();

    cancellation_source early_source;
    early_source.cancel();
    early := process("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"], early_source.token());
    try {{ early.out.read_bytes(1); return 7; }}
    catch (ExecError caught) {{ if (caught.code != 125) {{ return 8; }} }}
    early.terminate();
    early.wait();

    for (cycle := 0; cycle < 10; cycle++) {{
        cancellation_source cycle_source;
        cycle_child := new(process("{escaped(sys.executable)}", ["{escaped(helper)}", "sleep"], cycle_source.token()));
        channel<bool> cycle_result;
        cycle_worker := thread(() => {{
            try {{ cycle_child->out.read_bytes(1); cycle_result.send(false); }}
            catch (ExecError caught) {{ cycle_result.send(caught.code == 125); }}
        }});
        sleep_ms(10);
        cycle_source.cancel();
        if (!(cycle_result.receive() ?? false)) {{ return 9; }}
        cycle_worker.join();
        cycle_child->terminate();
        cycle_child->wait();
    }}

    print("process cancellation certification passed");
    return 0;
}}
'''
        program = root / "process-cancellation.p"
        executable = root / ("process-cancellation.exe" if sys.platform == "win32" else "process-cancellation")
        program.write_text(source, encoding="utf-8")
        subprocess.run([compiler, program, "-o", executable], cwd=root, check=True)
        for iteration in range(10):
            result = subprocess.run([executable], cwd=root, text=True, capture_output=True, timeout=15, check=False)
            if result.returncode != 0 or result.stdout != "process cancellation certification passed\n" or result.stderr:
                raise RuntimeError(
                    f"iteration {iteration} failed: exit={result.returncode} stdout={result.stdout!r} stderr={result.stderr!r}"
                )
    print("Process cancellation certification: 10 runs of 10 in-process cancellation cycles plus blocked writes, independent tokens, close, EOF, completion, and pre-cancellation passed")


if __name__ == "__main__":
    main()
