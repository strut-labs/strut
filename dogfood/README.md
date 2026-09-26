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

Compile every program and run the offline-safe subset with:

```sh
python3 dogfood/check.py build/strut
```

They are deliberately kept small enough to remain regression-friendly while crossing multiple language/runtime subsystems. API pain discovered here should be fed back into the pre-stability design audit rather than worked around in examples.
