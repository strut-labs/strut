# Advanced type/error dogfood

`app.p` composes contextual generic calls, a nested JSON response, SQLite,
HTTP routing, and two nominal checked errors. It is a compile-only backend
shape: certification must not start the listener.

```sh
strut app.p -o app
```
