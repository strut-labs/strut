# Editor and language-server support

Strut ships a built-in Language Server Protocol endpoint:

```sh
strut lsp
```

It communicates over standard input/output using JSON-RPC framing. The current server reuses the compiler lexer, parser, semantic analyser and formatter and provides:

- parse/type diagnostics on open/change,
- go-to-definition for declarations visible in the current document,
- completion for Strut keywords and document declarations,
- hover information for declarations,
- whole-document formatting through the canonical Strut formatter.

The implementation is intentionally dependency-free beyond Strut's approved JSONIC dependency. Editor-specific extensions can launch `strut lsp`; they should not duplicate language parsing or type rules.
