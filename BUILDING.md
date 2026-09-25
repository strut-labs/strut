# Building Strut

## Bootstrap requirements

- CMake 3.20 or newer
- a C++20 compiler:
  - GCC 11+ or Clang 14+ on Linux
  - Apple Clang with C++20 support on macOS
  - Visual Studio 2022 / MSVC on Windows
- Python 3 for the independent regression suite (not required by the compiler binary itself)

Ninja is recommended but not required.

## Linux / macOS

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Without Ninja, omit `-G Ninja`.

## Windows (PowerShell, Visual Studio generator)

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Build-tree policy

Generated files and binaries belong under `build/` (or another caller-selected CMake build directory), never alongside source files.

## Native backend policy

The initial backend emits portable C++20 then invokes a host C++ toolchain. The compiler frontend must not depend on this representation. Direct AOT backends can be added later behind the backend interface.
