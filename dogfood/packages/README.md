# Package ecosystem dogfood

These local packages exercise the same `strut add` / shared-cache / lockfile / `include <package>` flow expected for the future `strut-packages` organization. HTTP, TLS and SQLite are represented as thin official-style source packages over the current runtime primitives. `system` is an additional integration package built on `exec`.

The wrappers are intentionally small: their purpose is to dogfood package resolution and authoring rather than hide the underlying Strut APIs. As the package ecosystem is extracted from bootstrap built-ins, these directories are migration fixtures rather than a claim that the current layout is the final official-package repository structure.
