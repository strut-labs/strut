# Agent instructions (Strut)

Strut is a memory-safe compiled language that emits native C++20. This file is the always-read
entry point for agent sessions in this repository. Read it before starting, and keep it short.

## Repositories

- `strut` (this repository) — compiler, runtime, standard library, dogfood/CI, docs.
- `strut-regression-suite` — independent black-box regression suite (sibling repository).
- `strut-labs.github.io` — public website + documentation (sibling Nift project).
- `strut-benchmarks`, `strut-packages` — benchmarks and package ecosystem.

Boundaries: the **Nift core is strictly read-only**; **Strut v0.0.4 is immutable**; do not
change unrelated repositories. **Do not tag, publish, package, or deploy** without explicit
user approval.

## Cross-repository completion gate (mandatory)

Strut's development process treats the independent regression suite and the website as part of
the change, not as release cleanup (see `REGRESSION_HANDOVER.md`, `WEBSITE_HANDOVER.md`,
`COMPATIBILITY.md`). Drift happens when an implementation change is declared complete without
the sibling-repository work.

Before declaring any meaningful change complete, assess and **record** the impact on:

1. **Compiler/runtime tests** — `ctest` (default and reactor), dogfood certification.
2. **Independent regression suite** — does the change alter user-visible language/runtime
   behaviour? Add or update a fixture in `strut-regression-suite`.
3. **Website documentation and examples** — does it change a public API, syntax, capability, or
   version boundary? Update `strut-labs.github.io` source (examples must compile/run).
4. **Release notes / checklist** — note user-visible changes.

Rules:

- When an update is needed, it is part of completing the feature — not deferred.
- When no update is needed, record a **one-line justification** (e.g. "internal refactor, no
  public contract change").
- The completion report must state **which related repositories were checked**, not only which
  one was modified.
- Do not silently update regression expectations or website claims to make something pass;
  determine whether the implementation or the expectation is wrong.

This gate is satisfied by a recorded assessment, not necessarily by commits in every
repository. A justified "no change required" is acceptable; an omitted assessment is not.

## Non-negotiables

- Do not weaken or delete certification/regression assertions to make a check pass.
- Do not change the public FFI ABI or general ownership/capture semantics without explicit
  approval.
- Preserve and extend certification evidence in `certification/` and `RELEASE_CHECKLIST.md`.
- Run the relevant checks (CTest, regression suite, website `nift build` + `check_site.py`)
  before reporting success — "tests pass" alone is not completion.
- Website deployment is a separate branch/publication action; do not push the generated site.

## Where to look

- `HANDOVER.md`, `IMPLEMENTATION_HANDOVER.md`, `LANGUAGE_HANDOVER.md`, `BACKEND_RUNTIME_HANDOVER.md`.
- `REGRESSION_HANDOVER.md`, `WEBSITE_HANDOVER.md`, `FFI_HANDOVER.md`, `FFI_ABI_AUDIT.md`.
- `RELEASE_CHECKLIST.md`, `RELEASE_PROCEDURE.md`, `certification/v0.0.5/status.md`.
