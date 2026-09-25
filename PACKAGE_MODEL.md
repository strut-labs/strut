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

## Lockfile and reproducibility

Resolution will produce `strut.lock.json`. The lockfile is committed for applications and records every direct/transitive package as an exact immutable resolution including:

- package name;
- exact semantic version;
- canonical source/repository identity;
- immutable revision/tag identity;
- content checksum;
- exact transitive dependency edges.

Entries are serialized in lexicographic package-name order (then version where needed) so two equivalent resolutions produce byte-stable lockfiles. Normal builds consume the lockfile without silently upgrading versions. Explicit package update operations are responsible for changing it.

## Shared package cache

Downloaded package contents are shared across projects and addressed by immutable package/version/content identity. `STRUT_HOME` overrides the base location. Otherwise the platform defaults are:

- Linux: `$XDG_CACHE_HOME/strut/packages`, or `~/.cache/strut/packages`.
- macOS: `~/Library/Caches/strut/packages`.
- Windows: `%LOCALAPPDATA%\\Strut\\Cache\\packages`.

A cached package never changes in place. A different checksum/revision receives a different cache entry. Project builds reference cached immutable sources rather than creating a `node_modules`-style dependency tree inside every project.

## Deterministic resolution

The resolver checkpoint must:

1. read all requirements without depending on filesystem/hash-map iteration order;
2. prefer an existing locked exact version when still compatible;
3. otherwise choose the highest compatible published semantic version;
4. resolve package names in stable lexicographic order;
5. reject conflicting requirements with a dependency-path diagnostic rather than choosing nondeterministically;
6. verify cached/downloaded content against the lockfile checksum before use.

CP71 defines and validates this contract. Fetching, registry/GitHub organisation conventions, lockfile mutation, and `strut add` arrive in subsequent package checkpoints.
