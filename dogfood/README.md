# Strut dogfood programs

These programs exercise Strut as an application language rather than isolated syntax fixtures:

- `cli/file_stats.p` — filesystem/ifstream/string/nullable environment handling.
- `fs/source_index.p` — recursive source-file counting and byte totals.
- `json/transform.p` — JSON parsing, omission, deep merge and serialization.
- `http/endpoint_check.p` — configurable HTTP client status/body check.
- `service/concurrent_service.p` — native threads, mutexes, typed channels and async futures.
- `web/app.p` — SQLite, async HTTP routing and compile-time embedded static assets in one executable.
- `package-app` — a manifest-backed multi-file project with SQLite and process execution.
- `nift-info/main.p` — parsing and reporting real Nift metadata.

`FIRST_ATTEMPT.md` records compiler/doc feedback cycles from the independent dogfood audit.
`AI_DX_BENCHMARK.md` reports the repeatable twelve-task blind benchmark and its
before/after metrics. Run preserved attempts with `dogfood/benchmark.py`.
`ADVANCED_AI_DX_BENCHMARK.md` extends the same protocol to backend composition,
TLS, packages, project discovery, nested generics, and machine-readable API help.

Compile every program and run the offline-safe subset with:

```sh
python3 dogfood/check.py build/strut
```

Certify that six common mistakes produce actionable Strut-level guidance with:

```sh
python3 dogfood/diagnostics.py build/strut
```

Exercise the built-in language server through its standard stdio protocol:

```sh
python3 dogfood/lsp_check.py build/strut
```

The controlled mistakes cover a missing standard module, an unhandled checked
error, an unsafe pointer operation, an invalid generic key, package dependency
resolution, and a missing native HTTP dependency. Each must be fixable from the
first diagnostic without inspecting compiler source or generated C++.

They are deliberately kept small enough to remain regression-friendly while crossing multiple language/runtime subsystems. API pain discovered here should be fed back into the pre-stability design audit rather than worked around in examples.
