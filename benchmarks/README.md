# Strut runtime microbenchmarks

`ptr_baseline.cpp` records the initial reference-counting cost baseline used by CP35. It compares the C++17 `std::shared_ptr` mechanism currently used by the bootstrap backend with a raw pointer copy loop. Results are host-specific and are a baseline, not a portability promise. Re-run before/after ownership-runtime optimisations rather than treating `ptr_baseline.latest.txt` as a universal score.
