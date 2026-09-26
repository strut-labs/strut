# Package AI-DX benchmark

The post-CP8 controlled attempt used only `strut help`, command help, the public
package documentation, diagnostics, and `api/project/packages --json`. Compiler
source was not consulted.

The task initialized a project, declared exact local-Git dependencies, installed
and inspected a diamond graph, compiled package APIs, changed the declared
immutable revisions, updated the lock, reproduced it in a second empty cache,
and then installed and built with Git unavailable.

Attempt 0 completed with no correction cycle. The CLI exposed one documentation
drift during preparation: `strut help init` mentioned only build configuration
although the command also creates `strut.json`. That help is now corrected.
Machine-readable API metadata now includes canonical per-command options such as
`install --offline` and `packages --json`; no second command registry was added.

The permanent executable form is `dogfood/package_certification.py`. It retains
the exact success criteria in CI rather than relying on this narrative snapshot.
