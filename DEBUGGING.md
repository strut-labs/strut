# Debug information and runtime stack traces

Debug builds preserve native symbols and source mapping. The bootstrap backend emits `#line` directives that map generated C++ statements back to the originating `.p` source line, compiles GCC/Clang debug objects with `-O0 -g`, and uses MSVC `/Od /Zi` plus linker `/DEBUG`. Linux debug links export symbols so runtime backtraces retain useful function names. Release builds continue to use optimisation/LTO and strip/dead-code-eliminate symbols where the platform supports it.

Runtime safety failures that represent a Strut panic print the Strut source location followed by a native stack trace before terminating. For example, a null `ptr<T>` dereference reports the `.p` path and line that performed the dereference. POSIX builds use the platform `execinfo` backtrace API; Windows uses `CaptureStackBackTrace`. These are OS facilities, not additional embedded dependencies.

The current bootstrap stack trace is intentionally low level. Function names are normally available in debug builds, while exact symbolic source-line expansion beyond the directly reported Strut panic location may still require the platform debugger/symbolizer. A future native backend can provide richer Strut-frame metadata without changing the language surface.
