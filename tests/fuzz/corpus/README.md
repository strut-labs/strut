# Frontend fuzz corpus

The deterministic smoke fuzzer seeds the lexer/parser/typechecker with representative valid constructs and then mutates arbitrary source bytes. Add minimized crash/hang inputs here whenever one is found so it becomes a permanent regression.

For deeper campaigns, build `strut_frontend_fuzz_tests` with sanitizers enabled and run it under the same compiler sanitizers used by CP103.
