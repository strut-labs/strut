# Editor and language-server support

Strut ships a built-in Language Server Protocol endpoint:

```sh
strut lsp
```

It communicates over standard input/output using JSON-RPC framing. The server reuses the compiler lexer, parser, semantic analyser, API registry, project discovery and formatter and provides:

- ranged, coded diagnostics on open/change and diagnostic clearing on close,
- project-aware go-to-definition and document symbols,
- contextual expression, type, import and receiver-member completion,
- automatic standard-module import edits where an API requires one,
- signature help with active-argument selection and checked-error documentation,
- hover information for source declarations and registered APIs,
- useful completion while the current source is incomplete,
- whole-document formatting through the canonical Strut formatter.

The implementation is intentionally dependency-free beyond Strut's approved JSONIC dependency. Editor-specific extensions can launch `strut lsp` through their normal stdio LSP client; they need no custom protocol and should not duplicate language parsing, API signatures, checked errors, module ownership, or type rules.
