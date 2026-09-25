# Strut package ecosystem contract

Official packages live under the `strut-packages` GitHub organisation, one repository per package: `strut-packages/http`, `strut-packages/sqlite`, and so on. Package names use lowercase ASCII letters, digits, `-`, and `_`; the repository name and manifest `name` must agree.

## Quality contract

An official package must have a versioned `strut.json`, tests that run without network access where practical, public API documentation, supported-platform notes, and reproducible native-link metadata. Releases use immutable semantic-version tags. A published version is never rewritten.

## Source/build metadata

`strut.json` may declare `entry`, `sources`, and `include_dirs`. All package paths are relative to the package root and may not escape it. Native-library requirements must be explicit rather than hidden in build scripts. The compiler will never execute arbitrary package install scripts as part of dependency resolution.

## Local development

A package can be developed from a local checkout and installed into the shared cache by the package CLI. Local development is explicit: the lockfile records the source/revision or content checksum used, so switching from a local checkout to a published package cannot happen silently.

## Security and reproducibility

Dependency resolution is deterministic. Lock entries identify exact versions and content checksums. Package cache entries are immutable by `(name, version, checksum)`. Path traversal in package metadata is rejected. Fetching, unpacking, and native build steps must not silently execute package-provided shell commands.
