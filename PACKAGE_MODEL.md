# Package include model

Strut source distinguishes local semantic includes from package includes:

```strut
include "user.h";
include <http>;
```

Local includes resolve relative to the including file and are loaded once semantically; they are not textual preprocessing.

Angle-bracket includes name a package in the project/package namespace. Package discovery, manifests and fetching arrive in later checkpoints; CP42 deliberately records package includes in the AST without attempting network access or silently vendoring dependencies. Official packages will use the same concise source syntax as third-party packages. Local filesystem source continues to use quoted includes, so package and local resolution cannot be confused.
