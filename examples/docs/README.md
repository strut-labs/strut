# Executable documentation examples

`manifest.json` classifies every canonical example as `run`, `compile`, or
`expected-error`. Runnable cases declare deterministic output; compile-only cases
cover offline-safe surfaces whose execution would mutate the filesystem; negative
cases require specific Strut diagnostics and reject native-toolchain failures.

Website source regions use paired comments such as:

```html
<!-- strut-example:hello-world:start -->
...
<!-- strut-example:hello-world:end -->
```

Synchronize those regions after changing a canonical source:

```sh
python3 tools/certify_docs.py --website-root ../strut-labs.github.io --sync-website
```

CI runs the tool without `--sync-website`, so a missing marker or any byte-level
source drift fails certification. Unmarked website blocks are documentation
fragments and are not compiled implicitly.
