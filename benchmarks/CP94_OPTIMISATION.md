# CP94 optimisation pass

CP94 adds a first explicit optimisation layer rather than relying only on whatever the generated C++ compiler happens to do.

- IR lowering folds constant integer arithmetic/comparisons when both operands are literals.
- Release object builds enable function/data sections plus LTO (`-flto` on GCC/Clang, `/GL` + `/LTCG` on MSVC).
- Link-time dead-code elimination remains enabled (`--gc-sections`, `dead_strip`, or `/OPT:REF /OPT:ICF`).
- Host `-O2` remains the release optimisation level after measurement: an `-O3` experiment increased several representative binaries and was not retained.
- Function inlining is currently delegated to the host compiler/LTO. Strut-specific inlining should only be introduced once typed-IR profiling demonstrates a missed opportunity.
- Escape/refcount optimisation opportunities are deliberately handed to CP95.

The CP93 baseline was rerun after the pass and is stored as `results/cp94-optimized.json`. On the development host LTO reduced the collections/JSON example from 55,824 to 51,736 bytes while leaving stripped hello/arithmetic sizes unchanged. Single-run startup/runtime timings are noisy and are retained only as raw baseline data; no runtime speed claim is made from them.
