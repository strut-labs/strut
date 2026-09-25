# CP93 baseline benchmark suite

Run the reproducible baseline with:

```bash
python3 benchmarks/baseline.py --compiler build/strut --output benchmarks/results/cp93-baseline.json
```

The suite records release compile time, stripped executable size, and median process runtime/startup for representative arithmetic/loop, strings/collections/JSON, reference-counted pointer, lambda/call, threading/async, and SQLite programs. It separately compares dynamic and fully-static hello-world builds where the host toolchain supports both. The existing one-binary HTTP application baseline is folded into the JSON result so HTTP throughput/latency/RSS remains part of the benchmark record.

Results are host-specific baselines, not cross-language performance claims. Benchmark changes must preserve the raw JSON and describe the host/toolchain before public comparisons are made.

The CP93 development-host run showed a 14.5 KiB stripped dynamically linked hello-world executable versus 1.39 MiB fully static on this Linux host. Representative release executable sizes ranged from 14.4 KiB for arithmetic/lambda programs to 64.2 KiB for the SQLite case. These figures are intended to expose regressions and optimisation opportunities, not to imply portability to other platforms.
