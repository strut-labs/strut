# Cross compilation

Strut uses a fixed target-name vocabulary so build metadata, package artifacts and CI use the same names:

- `native`
- `linux-x64`
- `linux-arm64`
- `macos-arm64`
- `macos-x64`
- `windows-x64`

Direct compilation accepts `--target <name>`. Project builds use the `target` field in `.strut/config.json`; object and metadata caches are already partitioned by target.

The current C++ bootstrap backend intentionally delegates cross compilation to an external C++ cross toolchain. Default driver names are:

| Strut target | Default cross C++ driver |
| --- | --- |
| linux-x64 | `x86_64-linux-gnu-g++` |
| linux-arm64 | `aarch64-linux-gnu-g++` |
| windows-x64 | `x86_64-w64-mingw32-g++` |
| macos-arm64 | `oa64-clang++` (osxcross convention) |
| macos-x64 | `o64-clang++` (osxcross convention) |

Every target can override the driver without changing Strut source. For example:

```sh
STRUT_CXX_LINUX_ARM64=/opt/toolchains/bin/aarch64-linux-gnu-g++ \
    strut app.p --target linux-arm64
```

Native builds continue to honour `CXX`.

Cross compilation of programs using native packages requires target-compatible headers/libraries for those packages. Strut never silently substitutes host libraries. libcurl, OpenSSL and SQLite therefore need target builds when the program uses them. Fully static macOS binaries are not supported by the normal Apple toolchain.

The bootstrap has been exercised locally for `linux-x64`; the other targets are certified only when their corresponding toolchains/runners are available. Native macOS and Windows certification is intentionally handled by GitHub Actions rather than being inferred from Linux cross compilation.
