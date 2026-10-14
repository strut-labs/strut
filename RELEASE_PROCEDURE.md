# Strut release procedure

Status: prepared for v0.0.5, **not executed**. Tagging and publication require explicit user
approval (see Approval checkpoints).

## Scope

Strut releases are driven entirely by the GitHub Actions release workflow
(`.github/workflows/release.yml`), triggered by pushing a tag matching `v*`. The release
produces a GitHub Release with relocatable archives for four platforms plus a `SHA256SUMS`
file. There is no separate package-registry publication step in this repository; language
packages are distributed through the `strut-packages` organization independently.

## Preconditions (verify before tagging)

- The implementation is frozen at the approved candidate. For v0.0.5 the certified candidate is
  `2c9c7e8`; any subsequent commit must be documentation-only and is recorded separately.
- Release metadata is consistent: `python3 tools/check_release.py v0.0.5` prints
  `release metadata is consistent for 0.0.5`. This checks:
  - `CMakeLists.txt` `project(strut VERSION 0.0.5 ...)`;
  - `include/strut/version.h` `version = "0.0.5"`;
  - `RELEASE_NOTES.md` contains an exact `# Strut 0.0.5` heading.
- `RELEASE_NOTES.md` fully describes 0.0.5 (no `# Unreleased` placeholder).
- The independent regression-suite commit pinned in `release.yml`
  (`strut-labs/strut-regression-suite`, currently `f62701cd737a01412986fc1f14002fdecb4bd3d0`) is
  the intended baseline for the release.
- The working tree is clean and `main` is synchronized with `origin/main`.
- The five-platform certification for the candidate is green (for v0.0.5: workflow run
  `38529518056` on `2c9c7e8`, five-platform PASS first attempt).

## Automated release workflow

Trigger: `git push origin v0.0.5` (tag matching `v*`).

`package` job (matrix: `linux-x64`, `linux-arm64`, `macos-arm64`, `windows-x64`):

1. Check out the repository and the pinned regression suite.
2. Validate the tag and metadata: `tools/check_release.py "${GITHUB_REF_NAME}"`.
3. Install generated-program dependencies (curl/OpenSSL/sqlite3; vcpkg on Windows).
4. Configure and build `Release` with `-DSTRUT_WARNINGS_AS_ERRORS=ON`.
5. `ctest` for the platform.
6. Run the independent regression suite against the built compiler.
7. Run the release dogfood runtime certification suite (HTTP/WebSocket, worker/lifecycle,
   bytes/crypto/streams/cancellation/process, PTY and PTY lifecycle, WebSocket-PTY, and the
   native FFI link tests).
8. `cmake --install` into a staged relocatable prefix `stage/strut-<version>-<platform>`.
9. Verify the installed compiler: `strut --version` and compile+run a smoke program.
10. Archive (`tar.gz` for POSIX, `zip` for Windows) and upload the artifact.

`publish` job (needs `package`):

11. Download all platform artifacts into `dist/`.
12. Generate `dist/SHA256SUMS` (`sha256sum strut-*`).
13. Create the GitHub Release for the tag as a draft (`gh release create --verify-tag --draft
    --notes-file RELEASE_NOTES.md`), upload the archives + `SHA256SUMS`, then flip the release
    to published (`gh release edit ... --draft=false`).

## Credentials and permissions

- The workflow uses the built-in Actions token with `permissions: contents: write`
  (`GH_TOKEN: ${{ github.token }}`). No external secret, registry credential, or signing key is
  configured or required by the current procedure.
- Publishing to the website/documentation repository and to `strut-packages` is separate and
  requires its own approval; it is not performed by this workflow.

## Approval checkpoints

1. **Tag creation** — `git tag -a v0.0.5 -m "Strut 0.0.5"` and `git push origin v0.0.5` require
   explicit user approval. Do not create the tag before approval.
2. **Release publication** — the `publish` job creates the GitHub Release; this is the
   publication action. It runs automatically once the tag is pushed, so the tag push is the
   publication decision.

## Post-release verification (after the workflow completes)

- The Release workflow's `package` and `publish` jobs are green.
- The GitHub Release exists for `v0.0.5`, non-draft, with four archives and `SHA256SUMS`.
- Download each archive, verify it against `SHA256SUMS`, extract, run `bin/strut --version`
  (expects `0.0.5`), and compile+run the smoke program.

## Documentation changes proposed for the separate website repository

The packaged README/CHANGELOG and the public documentation live in the separate
`strut-labs/strut-labs.github.io` repository. That repository is **not modified here**. Proposed
updates for review:

- Bump the displayed version and the download/release links to 0.0.5.
- Add a "Two-way FFI and embedding" guide summarizing FFI-1…FFI-10 and the FFI-8 embedding API
  (`stru_embed_context`, `load_source`/`load_file`, invocation, value/retained/callback kinds,
  release rules, BUSY destroy), with the five-platform consumer matrix.
- Add an explicit weak-reference ownership example showing a handler capturing `weak(server)`,
  promoting with `.lock()`, and breaking the server/handler reference cycle (the compiler fix in
  1b0560b). Note that by-value capture is unchanged and strong self-capture still cycles.
- Document the v0.0.5 known limitations (FFI `bool`/hex-literal/`\uXXXX` restrictions,
  interpreter-hosted consumers not ASan-certified, Nift interop Linux-local, Windows PTY
  interrupt/ETX timing residual).
- Ensure the compatibility page states that the reactor backend remains opt-in
  (`STRUT_HTTP_REACTOR=1`) and that the embedding ABI is new in 0.0.5.
