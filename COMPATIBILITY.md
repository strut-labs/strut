# Strut compatibility and versioning policy

## Compiler and language versions

Strut uses semantic versioning for compiler releases. Before 1.0, minor releases may contain deliberate language changes; every such change must be documented and covered by regression fixtures. After 1.0:

- patch releases must not intentionally break valid source or package contracts;
- minor releases may add backwards-compatible language/library features;
- major releases are required for deliberate backwards-incompatible language changes.

The language does not introduce editions yet. Editions add permanent complexity and are not justified until Strut has real long-lived codebases that need them.

## Deprecation

After 1.0, avoid immediate removal of public language/CLI/package behaviour. Where practical, deprecate in a minor release with a clear diagnostic and remove only in a later major release. Security defects may require faster changes and must be documented explicitly.

## Packages

Packages use semantic versions in `strut.json`. Lockfiles record exact versions/revisions/checksums so a successful locked build does not silently change underneath the user. Package authors should treat public source/API changes under the same SemVer expectations as the compiler after their own 1.0 release.

## Regression baselines

Every release candidate must record:

- compiler commit/version;
- independent regression-suite commit;
- official-package revisions;
- supported target/platform matrix;
- benchmark/regression baseline where relevant.

A released behaviour that is intentionally supported should gain a regression fixture before a later refactor can accidentally remove it.

## Generated/build metadata

`.strut` object metadata includes compiler/build fingerprints and target/mode information. It is a build cache contract, not a stable interchange format; incompatible metadata may force a clean rebuild rather than being migrated indefinitely.

## C ABI and FFI

`extern "C"` follows the target platform C ABI. Strut guarantees its syntax and safety boundary, not binary compatibility between arbitrary C++ ABIs or incompatible third-party library versions.

## Standard-library naming migration

- `map<K,V>` is hash-based and does not promise iteration order. Use `ordered_map<K,V>` when sorted iteration / tree semantics are required.
- `set<T>` is hash-based; use `ordered_set<T>` for sorted iteration.
- The provisional `prique<T>` spelling has been removed. Use `priority_queue<T>` (max-first) or `priority_queue<T,min>` (min-first).
- Standard-library and package modules use angle-bracket includes, e.g. `include <vector>;` and `include <sqlite>;`. Local source dependencies remain quoted, e.g. `include "mylib.h";`.
