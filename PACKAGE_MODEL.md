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

CP71 defined the base contract; CP72–CP73 added the `strut-packages` repository convention and local package workflow. The reproducibility phase upgraded the lockfile to schema 2 with content digests, immutable Git revisions, direct/transitive edges, deterministic graph resolution, conflict and cycle diagnostics, atomic cache repair, and offline installation.

## Package CLI (CP73)

`strut install <name>[@<requirement>]` is the official-package shorthand. A bare name has exactly one meaning: `https://github.com/strut-packages/<name>`. The resolver reads immutable semantic-version Git tags, chooses the highest compatible version, resolves that tag to an exact commit, and records the canonical repository, commit and content checksum in the lockfile. It never searches GitHub, guesses owners, or falls back to a similarly named third-party repository.

For example, `strut install sqlite` installs the highest tagged official SQLite package, while `strut install sqlite@^1.2.0` constrains selection. The resulting manifest remains concise (`"sqlite": "^1.2.0"`); `strut packages --json` exposes `official`, `source_owner`, `source_repo`, and `canonical_source` for tooling.

`strut add <path>` adds a local package checkout to the current project's manifest, copies the immutable version into the shared cache, and rewrites the lockfile deterministically. Third-party remote dependencies remain explicit objects with a semantic `version`, Git URL, and exact hexadecimal `rev`. `strut remove <name>`, `strut list`, `strut install`, `strut update`, and `strut packages [--json]` manage and inspect the graph.

`include <name>` loads the package entry declared by the exact locked and checksum-verified package. An application may include its direct manifest dependencies; a package may include its own declared dependencies. A dependency import exposes only that dependency's public surface to the importing owner and does not re-export it.

## Package exports and ownership

Package entries may opt into an explicit public surface with contextual top-level directives:

```strut
include "implementation.p";

client := Client { endpoint: "https://example.test" };
function request(string path) -> string { return request_impl(client, path); }

export client;
export request;
```

The first `export name;` or `export;` directive in the manifest entry selects explicit mode. `export name;` exports every function overload owned by that package under `name`, or the single owned value/type with that name. `export;` selects explicit mode without adding a name, so it can define an intentionally empty public surface. Directives are declarations of the package interface, not namespaces, and are only valid at the top level of the manifest entry.

Packages with no export directives retain the legacy export-all behavior. This keeps existing packages source-compatible. Explicit packages reject external `include <name/path.p>` access; implementation files remain reachable through quoted includes from the package and retain the same package owner. Every package-owned path is checked both lexically and after canonical filesystem resolution, so quoted includes and legacy subpath includes cannot traverse or follow a symlink outside the verified package root.

Every package-owned top-level value, function, and type receives a deterministic compiler identity using an injective encoding of its value/function/type namespace, source spelling, locked package name/version, and verified content checksum. A direct importer binds exported source names to those identities; private and transitive names never enter its scope. Lexical binding resolution preserves parameters, locals, lambdas, loop variables, catch bindings, fields, and generic type parameters that shadow a package top-level spelling. Semantic ownership also rejects external references to generated identities, so spelling or discovering an internal name does not grant access. Distinct package names such as `a-b` and `a_b` cannot collapse to one identity, while every edge of a diamond uses the same locked identity for its shared node.

A package may export a facade value whose recursively inferred carrier type remains private. Inference follows grouping, package value forwarding, struct construction, scalar and aggregate literals, and unambiguous package function returns; an exported value whose public type cannot be determined is rejected. The carrier's fields, bases, and inline or out-of-struct method signatures are part of that facade API and may use only standard types or types exported by the same package; another private type cannot leak through a nested generic, array, pointer/reference, or nullable type. The same rule applies to exported structs and their methods. Dependency types cannot be exposed through a package signature because dependency imports are not re-exports. Missing, duplicate, ambiguous, and dependency-owned exports are compile errors; exporting a function name exports its complete overload set.

Package-owned `main` is always internal and cannot be exported, including under legacy export-all behavior. Operators are package-private in S0: they may support operations implemented by that package but cannot be named by an export directive or imported as a consumer API.

Standard-module includes and package imports belong to the source owner that declared them. A package's standard-module enablement does not enable that module in its consumer, and an angle-bracket dependency import does not make that dependency visible to the consumer's consumer. Package identity is the locked package name/version/content node, so a shared dependency in a diamond is loaded once. LSP package indexing walks only the manifest entry's quoted-include closure, regardless of whether those files use `.p` or `.h`, follows the same explicit surface, includes public struct and facade members, and omits unreachable, private, and transitive APIs.

## Install, update, and offline behavior

`strut install` consumes an existing valid lockfile exactly. It verifies every content-addressed cache entry and restores missing or corrupt Git or official packages from the locked URL and exact commit, rejecting any checksum mismatch without rewriting the lock. When no lockfile exists, a string dependency resolves from the official namespace if it is not already available as an explicitly added local package. `strut update` re-reads official tags and explicit manifest sources, then rewrites the lockfile. `strut install --offline` never performs acquisition: it requires a valid lockfile and every exact locked digest to already be cached, otherwise it fails with the missing identity and remediation.

`strut packages` reports every locked package, its direct/transitive status, request, resolved version, source, revision, checksum, and verified-cache state. `strut packages --json` and `strut project --json` expose stable schema-versioned state for CI, agents, and editors. The LSP reads the same validated lock/cache state for completion, hover, signatures, definitions, and missing-package diagnostics; it never acquires packages or contacts the network.

The permanent `dogfood/package_certification.py` matrix fixture certifies a Git-backed diamond graph twice from empty caches, byte-stable locks, compilation through a shared transitive package, network-disabled offline install, missing/corrupt cache handling, interrupted staging isolation, concurrent promotion, and controlled updates on Linux, macOS, and Windows.
