# Strut Regression Suite Handover

The sibling repository `strut-regression-suite/` is the independent compatibility and regression suite for Strut.

It should be created and used very early, not after the compiler is mature.

## Purpose

The regression suite should answer a simple question after every meaningful compiler change:

> Did Strut preserve all behaviour and diagnostics that have already been deliberately accepted?

It is separate from the compiler's own unit/integration tests so that compiler-internal refactors cannot accidentally redefine the expected language behaviour.

## Rules

- Add regression fixtures as soon as a user-visible language behaviour is implemented.
- Include both success and expected-failure cases.
- Record expected diagnostics for invalid programs where message quality/location is part of the contract.
- Keep fixtures small and focused; add real-project fixtures separately.
- Never silently update expected output merely to make a regression pass. Determine whether the compiler or the expectation is wrong.
- A checkpoint that changes syntax or semantics is not complete until corresponding regression cases pass.
- Keep the suite runnable against a locally built Strut binary and, later, released binaries.
- Add CI once the compiler can build in CI reliably.

## Suggested layout

The exact harness is not frozen, but a useful shape is:

```text
strut-regression-suite/
  README.md
  run.*
  cases/
    syntax/
    types/
    diagnostics/
    collections/
    json/
    functions/
    operators/
    streams/
    processes/
    linking/
    memory/
    errors/
    modules/
    concurrency/
    async/
    ffi/
    packages/
    generics/
    integration/
```

Each feature area should contain tiny fixtures plus expected stdout/stderr/exit-status metadata in a format that is easy for both humans and agents to inspect.

## Early regression sequence

1. Harness can locate/accept a Strut compiler binary.
2. Harness can run one successful fixture.
3. Harness can assert stdout and exit code.
4. Harness can assert compile failure.
5. Harness can assert diagnostic substring/location.
6. Every lexer/parser feature gains fixtures.
7. Every type/inference rule gains positive and negative fixtures.
8. Memory-safety rules gain compile-fail and runtime-lifetime fixtures.
9. Ref-counting/weak-pointer behaviour gains destruction/cycle tests.
10. Generic/template syntax gains fixtures for `[T]`, callable types, inferred uppercase lambda templates, and misspelled-type rejection.
11. Operator overloading gains normal/lambda operator fixtures, dereference, stream operators, `=`/`:=`, ambiguity and reserved-operator failures.
12. Streams gain `in`/`out`/`err`, file/string streams, insertion/extraction and custom-type fixtures.
13. `exec`/child processes gain argv/cwd/env/capture/exit-status/pipe fixtures.
14. Static/dynamic/mixed linking gains tiny native-library integration fixtures on each supported platform.
15. Concurrency gains deterministic contract tests plus stress tests where practical.
17. Released versions become selectable baselines for compatibility testing.

## Maintenance

When `LANGUAGE_HANDOVER.md` changes deliberately, update the regression-suite plan/fixtures in the same development batch. The regression repo should become increasingly difficult to "fake green" as Strut grows.
