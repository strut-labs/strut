# Strut Website / Documentation Handover

The sibling repository `strut-labs.github.io/` is the public Strut website and documentation site. It is a Nift project and already contains Nift's project handover.

The website should be established near the start of Strut development and maintained alongside the compiler rather than built as a launch-day afterthought.

## Visual direction

- dark mode by default;
- minimalist and polished;
- similar general quality/discipline to `nift.dev`, but not a clone;
- avoid blue-dominated palettes;
- responsive desktop/mobile layout;
- full-screen mobile docs/navigation menu toggled by a hamburger;
- clean code blocks and syntax presentation;
- dedicated favicon/brand assets;
- good typography and spacing;
- fast, dependency-light frontend;
- accessible navigation and sensible semantic HTML.

## Site structure

Use a multi-page documentation site, not one giant landing page. Initial pages should evolve toward:

```text
/
/about/
/install/
/getting-started/
/language/
  syntax/
  types/
  functions/
  collections/
  json/
  structs/
  memory/
  errors/
  concurrency/
  async/
  unsafe/
/packages/
/generics/
/cli/
/examples/
/changelog/
```

Do not publish speculative syntax as shipped functionality. Clearly separate implemented/stable features from roadmap/design notes if both are shown publicly.

## Navigation

Desktop should have clear documentation navigation. Mobile should use a hamburger that shows/hides a full-screen menu suitable for navigating the full docs tree.

The menu must close correctly after navigation/selection and should not trap scrolling or leave stale overlay state.

## 404 page

Provide a custom dark-mode 404 page similar in spirit to `nift.dev`'s 404 treatment:

- centered/minimal;
- no normal site header;
- no normal site footer;
- clear route back to useful content;
- same brand/theme as the rest of the site.

## Nift discipline

- Keep the existing `.nift/` project structure valid.
- Use `@path(...)` for internal project-aware links/assets where appropriate.
- Run `nift build` frequently during website work and after Nift config/tracking changes.
- Do not delete/recreate `.nift/` casually.
- Keep the site's own Nift `HANDOVER.md` current.

## Documentation maintenance rule

For every implemented checkpoint that changes user-visible Strut behaviour:

1. update the relevant docs page;
2. update examples/snippets;
3. rebuild with `nift build`;
4. inspect the generated output and responsive navigation where relevant;
5. commit the website changes in the website repository;
6. do not let the public docs claim syntax that no longer matches the compiler.

Major language-design changes should also trigger a docs audit for stale syntax.

## New language areas that must receive docs as implemented

Keep dedicated/reference coverage for:

- `function` declarations, callable types, lambdas, async lambdas, and `[T]` generic/template declaration syntax;
- operator overloading, including lambda operators, fixed precedence, dereference, `=` and `:=`;
- `istream`/`ostream`/`sstream`/`ifstream`/`ofstream` plus `in`/`out`/`err`;
- `exec` and child-process/pipe APIs;
- static, dynamic, and mixed linking plus executable-size/deployment guidance;
- `T*`, `T&`, `weak_ptr<T>`, and unsafe `ptr<T>`.

Do not document planned syntax as shipped without clear planned/provisional labelling.
