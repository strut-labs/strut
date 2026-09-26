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
- `^1.2.3` — compatible release within the same non-zero major version (with normal semver zero-major tightening when the resolver lands).
- `~1.2.3` — patch-compatible release within the same minor version.
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

CP71 defined the base contract; CP72–CP73 added the `strut-packages` repository convention, local package development/cache flow, deterministic lockfile mutation, `strut add/remove/list/install`, and package include resolution. The reproducibility phase upgraded the lockfile to schema 2 with content digests, logical source identities, direct/transitive edges, round-trip validation, stale-entry checks, duplicate rejection, and cycle rejection. Remote fetching remains intentionally separate from the local/cache workflow.

## Package CLI (CP73)

`strut add <path>` adds a local package checkout to the current project's manifest, copies the immutable version into the shared cache, and rewrites the lockfile deterministically. `strut remove <name>`, `strut list`, and `strut install` manage the local dependency set. Remote fetching is intentionally not guessed at before the HTTP client exists; CP73 operates against explicit local checkouts and cached versions.

`include <name>` loads the package entry declared by the resolved cached package. `include <name/path.h>` loads an explicit file inside that package. Package includes must also appear in the project manifest.

## Install, update, and offline behavior

`strut install` consumes an existing valid lockfile exactly. It verifies every content-addressed cache entry and restores missing or corrupt Git packages from the locked URL and exact commit, rejecting any checksum mismatch without rewriting the lock. When no lockfile exists, it resolves the manifest and writes one. `strut update` is the explicit operation that re-resolves current manifest sources and rewrites the lockfile. `strut install --offline` never performs acquisition: it requires a valid lockfile and every exact locked digest to already be cached, otherwise it fails with the missing identity and remediation.
