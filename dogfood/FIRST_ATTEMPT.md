# Dogfood first-attempt log

Recorded during the independent ecosystem audit. A cycle means one edit followed by
another compiler invocation.

| Program | Purpose | First attempt | Feedback cycles | Observation |
| --- | --- | --- | ---: | --- |
| `cli/file_stats.p` | CLI/stream input | compiled | 0 | Environment fallback and stream APIs were predictable. |
| `fs/source_index.p` | Recursive filesystem statistics | failed | 2 | The first signature omitted `FilesystemError`; then runtime use showed that `walk` entries are root-relative. Diagnostics were strong, but the traversal path contract needed clearer docs. |
| `json/transform.p` | JSON transformation | failed | 1 | An array argument now requires `include <vector>;`; the checked-in dogfood had drifted. The diagnostic was actionable. |
| `http/endpoint_check.p` | HTTP endpoint check | compiled | 0 | The documented `HttpError` contract and response fields were sufficient. Runtime deliberately requires network access. |
| `service/concurrent_service.p` | Threads/channels/async | compiled and ran | 0 | Ownership across the worker boundary was straightforward. |
| `web/app.p` | SQLite/embedded HTTP application | failed | 1 | `embed_dir` returns a map, so explicit modules require `include <map>;`. The diagnostic identified the missing module. |
| `package-app` | Manifest-backed multi-file application | failed | 3 | The old example named undeclared packages and wrappers; successive diagnostics identified the vector module and the removed `run_capture` API, which is now `exec(...).stdout`. |
| `nift-info/main.p` | Real JSON metadata reader | compiled and ran | 0 | Nullable environment fallback plus JSON indexing was concise. |

The two failures were ecosystem drift, not guessed-syntax failures in newly written
programs. Both are retained here because unchecked examples that stop compiling are
an important AI-DX signal.
