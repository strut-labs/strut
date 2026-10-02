# Package manifest and cache model

Strut source distinguishes local semantic includes from package includes:

```strut
include "user.h";
include <http>;
```

Local includes resolve relative to the including file and are loaded once semantically; they are not textual preprocessing. Angle-bracket includes name a package in the project/package namespace.

## Manifest

Projects/packages use `strut.json`. JSONIC is the canonical parser used by the bootstrap compiler for this metadata.

```json
{
  "name": "todo-service",
  "version": "0.1.0",
  "entry": "src/main.p",
  "dependencies": {
    "http": "^0.2.0",
    "sqlite": "~0.3.1"
  }
}
```

`name` and `version` are required. `entry` is optional for libraries. Manifest versions use `MAJOR.MINOR.PATCH`.

Dependency requirements intentionally start small and deterministic:

- `1.2.3` — exact version.
- `^1.2.3` — compatible release within the same non-zero major version.
- `^0.2.3` — compatible release within the same minor version (zero-major caret tightening).
- `^0.0.3` — exact patch compatibility within `0.0.x` (zero-major, zero-minor caret tightening).
- `~1.2.3` — patch-compatible release within the same minor version.
- `~0.2.3` — patch-compatible release within the same minor version.
- `*` — any published version; allowed but discouraged for reproducible applications because the lockfile still resolves it to one exact version.

No `latest`, git-branch-as-version, or arbitrary executable version scripts are part of the package contract.

An immutable Git dependency uses an object instead of the local/cache string form:

```json
"dependencies": {
  "example": {
    "version": "1.2.3",
    "git": "https://example.invalid/example.git",
    "rev": "0123456789abcdef0123456789abcdef01234567"
  }
}
```

`rev` must be an exact hexadecimal commit identity, not a branch or floating tag. Acquisition uses system Git with repository hooks disabled, checks out only that requested revision, verifies `HEAD`, removes Git metadata, validates the package manifest and paths, computes the content digest, and then uses the same staged atomic cache promotion as a local package. Package install scripts are neither recognized nor executed. Local Git repositories are supported as deterministic development and test sources; live hosting is not required by the resolver.

## Lockfile and reproducibility

Resolution produces `strut.lock.json` schema version 2. The lockfile is committed for applications and records every direct/transitive package as an exact immutable resolution including:

- package name;
- exact semantic version;
- canonical source/repository identity;
- immutable revision/tag identity;
- content checksum;
- exact transitive dependency edges.

Entries are serialized in lexicographic package-name order (then version where needed) so two equivalent resolutions produce byte-stable lockfiles. Normal builds consume the lockfile without silently upgrading versions. Explicit package update operations are responsible for changing it.

The schema uses a top-level `schema_version` and `packages` array. Each package records `name`, the original `requested` requirement, exact `version`, `source_kind`, logical `source`, immutable `revision`, SHA-256 `checksum`, whether it is `direct`, and an object of exact dependency edges. It contains no cache paths, checkout paths, timestamps, or other machine-specific state. Legacy version-1 files are rejected with instructions to regenerate rather than silently trusting their absolute cache paths.

## Shared package cache

Downloaded package contents are shared across projects and addressed by immutable package/version/content identity. `STRUT_HOME` overrides the base location. Otherwise the platform defaults are:

- Linux: `$XDG_CACHE_HOME/strut/packages`, or `~/.cache/strut/packages`.
- macOS: `~/Library/Caches/strut/packages`.
- Windows: `%LOCALAPPDATA%\\Strut\\Cache\\packages`.

A cached package never changes in place. Entries use `name/version/<sha256>` so different content at the same semantic version receives a different immutable cache entry. Local acquisition first validates source paths and content, copies into a cache-local staging directory, verifies the staged digest, and atomically renames it into its final identity. A concurrent installer either wins that atomic promotion or verifies and reuses the winner. Failed staging is removed, and abandoned staging directories older than 24 hours are cleaned on later acquisition. Cache reuse recomputes the content digest; local acquisition can safely repair a corrupt entry from its explicit source checkout. Project builds reference cached immutable sources rather than creating a `node_modules`-style dependency tree inside every project.

## Deterministic resolution

The resolver checkpoint must:

1. read all requirements without depending on filesystem/hash-map iteration order;
2. prefer an existing locked exact version when still compatible;
3. otherwise choose the highest compatible published semantic version;
4. resolve package names in stable lexicographic order;
5. reject conflicting requirements with a dependency-path diagnostic rather than choosing nondeterministically;
6. verify cached/downloaded content against the lockfile checksum before use.

CP71 defined the base contract; CP72–CP73 added the `strut-packages` repository convention and local package workflow. The reproducibility phase upgraded the lockfile to schema 2 with content digests, immutable Git revisions, direct/transitive edges, deterministic graph resolution, conflict and cycle diagnostics, atomic cache repair, and offline installation.

## Package CLI (CP73)

`strut install <name>[@<requirement>]` is the official-package shorthand. A bare name has exactly one meaning: `https://github.com/strut-packages/<name>`. The resolver reads immutable semantic-version Git tags, chooses the highest compatible version, resolves that tag to an exact commit, and records the canonical repository, commit and content checksum in the lockfile. It never searches GitHub, guesses owners, or falls back to a similarly named third-party repository.

For example, `strut install sqlite` installs the highest tagged official SQLite package, while `strut install sqlite@^1.2.0` constrains selection. The resulting manifest remains concise (`"sqlite": "^1.2.0"`); `strut packages --json` exposes `official`, `source_owner`, `source_repo`, and `canonical_source` for tooling.

`strut add <path>` adds a local package checkout to the current project's manifest, copies the immutable version into the shared cache, and rewrites the lockfile deterministically. Third-party remote dependencies remain explicit objects with a semantic `version`, Git URL, and exact hexadecimal `rev`. `strut remove <name>`, `strut list`, `strut install`, `strut update`, and `strut packages [--json]` manage and inspect the graph.

`include <name>` loads the package entry declared by the exact locked and checksum-verified package. `include <name/path.h>` loads an explicit file inside it. Includes may name direct or transitive packages present in the validated lock graph.

## Install, update, and offline behavior

`strut install` consumes an existing valid lockfile exactly. It verifies every content-addressed cache entry and restores missing or corrupt Git or official packages from the locked URL and exact commit, rejecting any checksum mismatch without rewriting the lock. When no lockfile exists, a string dependency resolves from the official namespace if it is not already available as an explicitly added local package. `strut update` re-reads official tags and explicit manifest sources, then rewrites the lockfile. `strut install --offline` never performs acquisition: it requires a valid lockfile and every exact locked digest to already be cached, otherwise it fails with the missing identity and remediation.

`strut packages` reports every locked package, its direct/transitive status, request, resolved version, source, revision, checksum, and verified-cache state. `strut packages --json` and `strut project --json` expose stable schema-versioned state for CI, agents, and editors. The LSP reads the same validated lock/cache state for completion, hover, signatures, definitions, and missing-package diagnostics; it never acquires packages or contacts the network.

The permanent `dogfood/package_certification.py` matrix fixture certifies a Git-backed diamond graph twice from empty caches, byte-stable locks, compilation through a shared transitive package, network-disabled offline install, missing/corrupt cache handling, interrupted staging isolation, concurrent promotion, and controlled updates on Linux, macOS, and Windows.
