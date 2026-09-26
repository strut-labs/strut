# Dogfood first-attempt log

Recorded during the independent ecosystem audit. A cycle means one edit followed by
another compiler invocation. The checked-in program is the final resolution; where
the initial attempt predated this log, the table says so rather than reconstructing
code from memory.

| Program | First attempt | Cycles | Error class | Docs used | Diagnostics sufficient | Internals inspected | Final resolution |
| --- | --- | ---: | --- | --- | --- | --- | --- |
| `cli/file_stats.p` | compiled | 0 | — | yes | n/a | no | Environment fallback and stream APIs worked as documented. |
| `fs/source_index.p` | failed | 2 | checked error, then path contract | yes | yes for the error; no for path semantics | no | Declared `FilesystemError`, joined each root-relative `walk` entry to its root, and clarified docs. |
| `json/transform.p` | failed | 1 | missing standard module | yes | yes | no | Added `include <vector>;`. |
| `http/endpoint_check.p` | compiled | 0 | — | yes | n/a | no | Runtime remains opt-in because it requires network access. |
| `service/concurrent_service.p` | compiled and ran | 0 | — | yes | n/a | no | Ownership, channels, mutexes, and async worked as documented. |
| `web/app.p` | failed | 1 | missing standard module | yes | yes | no | Added `include <map>;` for `embed_dir`'s result. |
| `package-app` | failed | 3 | undeclared packages, missing module, obsolete API | yes | yes | no | Converted it to a local multi-file project and replaced `run_capture` with `exec(...).stdout`. |
| `nift-info/main.p` | compiled and ran | 0 | — | yes | n/a | no | Nullable environment fallback plus JSON indexing worked directly. |

Four failures were ecosystem drift rather than fresh syntax guesses. They remain
important AI-DX evidence because unchecked examples are part of the effective docs.

## Protocol for future runs

For each new task, save the initial source before the first compilation and record:

1. compiler version and documentation revision;
2. exact task statement and pages consulted before coding;
3. initial source (or a path to a committed snapshot);
4. compiler/runtime result and error class for every cycle;
5. whether diagnostics alone were sufficient;
6. the first point, if any, where compiler source or generated C++ was inspected;
7. final source and total cycles.

Store controlled runs under `dogfood/attempts/<compiler-version>/<task>/` with an
immutable `attempt-0.p`, each subsequent `attempt-N.p`, and a `result.json`. The
result must record compiler commit, documentation commit, task text, consulted
pages, commands, exit statuses, normalized diagnostics, reason for each edit, and
the first point where implementation source was inspected. Do not overwrite an
attempt: comparisons depend on retaining the exact unsuccessful source as well as
the final program.

During the first-attempt phase the agent may use only `strut --help`, published
documentation, and compiler diagnostics. Compiler source, generated C++, existing
regression fixtures, and prior solutions are prohibited until the run records a
blocked attempt and its reason. Environment failures are classified separately
from language, documentation, and diagnostic failures.

The original audit predates this protocol, so it is a useful retrospective baseline
but not a controlled, independently repeatable benchmark. New rows should follow the
protocol above.

## Diagnostic-actionability control

`dogfood/diagnostics.py` now runs six deliberately broken programs as a stable
zero-retry control. Missing-module, checked-error, pointer, invalid-generic,
package-resolution, and native-dependency cases must all identify the category and
the next corrective action on the first compiler invocation. The suite reports one
compiler cycle per case; needing compiler source or generated C++ is a failure.
