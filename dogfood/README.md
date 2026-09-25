# Strut dogfood programs

These programs exercise Strut as an application language rather than isolated syntax fixtures:

- `cli/file_stats.p` — filesystem/ifstream/string/nullable environment handling.
- `json/transform.p` — JSON parsing, omission, deep merge and serialization.
- `service/concurrent_service.p` — native threads, mutexes, typed channels and async futures.
- `web/app.p` — SQLite, async HTTP routing and compile-time embedded static assets in one executable.

They are deliberately kept small enough to remain regression-friendly while crossing multiple language/runtime subsystems. API pain discovered here should be fed back into the pre-stability design audit rather than worked around in examples.
