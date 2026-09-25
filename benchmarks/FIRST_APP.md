# First serious application benchmark

CP86 establishes a reproducible baseline for the `examples/one-binary-todo` application. It is deliberately a baseline, not a claim about final Strut performance: the compiler still lowers through generated C++20 and the HTTP server is intentionally simple.

Run:

```bash
python3 benchmarks/first_app.py --compiler build/strut --output benchmarks/results/first-app.json
```

The script copies the example to a temporary project, expands the HTTP server request limit for the measurement, runs `strut init`, performs clean debug and release builds, then measures:

- cold compile wall time;
- executable size;
- cold server startup to the first completed HTTP request;
- resident memory after startup on Linux when `/proc` is available;
- 100 sequential `/api/todos` requests, reporting throughput plus mean/p50/p95 latency.

## CP86 baseline

Environment: Linux x86_64, glibc 2.41. Values are single-run baselines from the CP86 implementation and should be rerun on stable benchmark hardware before external comparisons.

| Metric | Debug | Release |
| --- | ---: | ---: |
| Cold compile | 3.341 s | 3.860 s |
| Executable size | 2,039,648 B | 97,424 B |
| Cold startup | 11.43 ms | 5.77 ms |
| Idle RSS | 7,232 KiB | 2,840 KiB |
| Sequential throughput | 4,648 req/s | 4,814 req/s |
| Mean latency | 0.215 ms | 0.207 ms |
| p50 latency | 0.192 ms | 0.183 ms |
| p95 latency | 0.329 ms | 0.330 ms |

The raw machine-readable result is stored in `benchmarks/results/first-app.json`.
