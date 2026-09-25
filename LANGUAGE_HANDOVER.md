# Strut Language Design Handover

This document records the current design direction. Items marked **provisional** remain open to change; everything else should be treated as the current language contract until deliberately revised.

## 1. Declarations, assignment, const, aliases

Declaration and assignment follow Nift's distinction:

```strut
x := 2;
name := "Nick";

int count := 10;
count = 11;
```

Rules:

```text
name := value       inferred declaration
Type name := value  explicit declaration
name = value        assignment
```

There is no `auto`, `let`, or `var` requirement for ordinary inference.

Variables are mutable by default. Immutability is explicit:

```strut
const limit := 10;
const int answer := 42;
```

Type aliases are first-class and should be used to implement ergonomic default numeric names rather than giving those names unnecessary compiler magic. Exact alias keyword syntax is **provisional**, but conceptually:

```strut
type int := int_32;
type uint := uint_32;
type double := double_32;

type user_id := uint_64;
```

## 2. Numeric types

Current intended family:

```text
int_8   int_16   int_32   int_64
uint_8  uint_16  uint_32  uint_64
double_32        double_64
```

Aliases:

```text
int    = int_32
uint   = uint_32
double = double_32
```

`double` deliberately means `double_32` in Strut even though that differs from C/C++ convention.

Whether larger widths, platform-sized integers, decimal types, or SIMD-specific numeric families are eventually added should be driven by real use cases.

## 3. Semicolons and blocks

Semicolons are mandatory for statements:

```strut
x := 1;
foo();
return x;
```

Normal control-flow blocks do not take a trailing semicolon:

```strut
if (x > 0) {
    print(x);
}
```

## 4. Control flow

C/C++-style conditions and Nift-style range loops:

```strut
if (count > 5) {
    print("hello");
}

while (running) {
    work();
}

for (int i := 0; i < 10; i++) {
    print(i);
}

for (item : items) {
    print(item);
}
```

Typed range form should be permitted:

```strut
for (User user : users) {
    print(user.name);
}
```

Support `break`, `continue`, and `return`.

`switch` and `match` are both planned. `switch` handles ordinary C-like branching; `match` handles richer pattern/destructuring cases.

## 5. Functions, function types, and templates

Named functions use the full `function` keyword and explicit return types, including `void`:

```strut
function add(int a, int b) -> int {
    return a + b;
}

function print_user(ref<const User> user) -> void {
    print(user.name);
    return;
}
```

`-> void` remains explicit. A bare `return;` is valid only in a `void` function. Returning a value from `void`, or bare-returning from a value-returning function, is a compile error.

Functions are first-class values. Explicit function types use:

```strut
function<(double, double) -> double> multiply := (x, y) => x * y;
function<(int) -> bool> predicate := (x) => x > 0;
function<() -> void> task := () => do_work();
```

Named generic/template declarations use square brackets so generic parameters do not visually collide with callable type syntax:

```strut
function max[T](T a, T b) -> T {
    return a > b ? a : b;
}

function[T]<(T, T) -> T> max_fn := (a, b) => {
    return a > b ? a : b;
};
```

Generic/template parameter identifiers must be uppercase identifiers such as `T`, `X`, or `Y`. This is deliberate: an accidental unknown type such as `ddouble` must be reported as an unknown type rather than silently becoming a template parameter.

Lambdas may infer generic/template parameters directly from uppercase type identifiers in their parameter signatures:

```strut
max := (T x, T y) => {
    return x > y ? x : y;
};

pair := (X x, Y y) => Pair<X, Y> {
    first: x,
    second: y
};
```

Do not add generic-constraint syntax until real use cases demonstrate that it improves expressiveness or diagnostics. Ordinary operator/type validity may be checked when a generic is instantiated.

Closures should support escaping captures safely.

## 6. Lambdas and async lambdas

Ordinary forms:

```strut
square := (x) => x * x;

handler := (req) => {
    return text("hello");
};
```

Async forms:

```strut
handler := async (req) => {
    return await process(req);
};

fetch := async (url) => await http.get(url);
```

Do not add implicit `{...}`-to-lambda conversion initially. APIs that accept callbacks should receive explicit lambdas, e.g.:

```strut
mtx.lock(() => {
    counter++;
});
```

## 7. Arrays, maps, JSON

Dynamic arrays use `T[]`:

```strut
int[] values := [1, 2, 3];
values.push(4);
```

Fixed-size arrays use `T[n]`:

```strut
int[16] buffer;
```

Nested forms should compose naturally:

```strut
int[][] matrix;
int[4][4] transform;
```

Map literals use square brackets with keyed entries so JSON remains unambiguous:

```strut
scores := [
    "alice": 10,
    "bob": 20
];
```

Explicit map type spelling remains:

```strut
map<string, int> scores := [
    "alice": 10,
    "bob": 20
];
```

JSON is a real built-in type and uses JSON-like braces:

```strut
payload := {
    "name": "Nick",
    "active": true,
    "roles": ["admin", "developer"]
};
```

This means a homogeneous JSON object is still JSON rather than silently inferring to a map.

JSON language/stdlib support should include parsing, stringify/serialization, pretty printing, indexing/navigation, equality, and typed encode/decode where practical.

## 8. Strings and collections

Provide rich collection functions early enough that Strut is pleasant for real scripting/backend work:

```strut
users.map(...);
users.filter(...);
users.reduce(...);
users.any(...);
users.all(...);
users.find(...);
users.count(...);
users.count_by(...);
users.index_by(...);
users.partition(...);
users.sort(...);

items.first();
items.last();
items.contains(...);
items.index_of(...);
items.push(...);
items.pop();
```

Also provide useful map/object helpers such as `pick`, `omit`, and `merge_deep` where semantics are well-defined.

Use `snake_case` consistently.

## 9. Structs, declarations, definitions, inheritance/contracts

Basic struct:

```strut
struct User {
    int id;
    string name;

    function display_name() -> string;
}
```

Out-of-struct definition:

```strut
function User::display_name() -> string {
    return name;
}
```

Inline definitions should also be permitted:

```strut
struct User {
    string name;

    function display_name() -> string {
        return name;
    }
}
```

Current direction is to avoid separate `interface` / `implements` keywords if ordinary struct declarations and inheritance can carry the contract cleanly.

An incomplete struct with function declarations but no implementations can act as an abstract contract and cannot be instantiated until all required functions are implemented by a concrete derived struct.

Multiple contracts should be possible:

```strut
struct User : Serializable, Printable {
    ...
}
```

Inheritance semantics are **provisional**. In particular, unrestricted C++-style multiple inheritance of data-bearing concrete bases should not be accepted blindly. A likely rule to prototype is one concrete/data-bearing base plus multiple abstract contract bases.

Struct literal/construction syntax currently remains:

```strut
user := User {
    id: 1,
    name: "Nick"
};
```

Fields and methods are public by default, matching `struct` expectations and keeping the first visibility model low-ceremony. Field layout follows source declaration order. A later visibility feature must be explicit rather than silently changing existing layout/access semantics.

## 10. Nullability

Use explicit nullable types:

```strut
User? user := find_user(id);
name := user?.name ?? "Anonymous";
```

Rules:

```text
T       non-nullable
T?      nullable
?.      safe access
??      fallback/default
```

Flow narrowing should remove nullable uncertainty when proven:

```strut
if (user != null) {
    print(user.name);
}
```

## 11. Memory model: no GC, reference counting by default

Strut has no tracing garbage collector.

Normal safe owning pointers are reference-counted:

```strut
ptr<User> a := ...;
ptr<User> b := a; // increments ownership count
```

When the last owning `ptr<T>` is released, the object is destroyed deterministically.

Pointer/reference family:

```text
ptr<T>             safe owning reference-counted pointer
ptr<const T>       owning pointer to immutable T
const ptr<T>       immutable pointer binding to mutable T
const ptr<const T> immutable pointer binding to immutable T

ref<T>             safe non-owning non-null reference
ref<const T>       safe non-owning non-null reference to immutable T

weak_ptr<T>        safe non-owning weak pointer for breaking ownership cycles

raw_ptr<T>         unmanaged raw pointer, only usable in unsafe contexts
```

Use `ref<T>` rather than C++ `T&` so constness and pointer/reference intent read clearly left-to-right.

`ref<T>` itself is non-rebindable by definition. `ref<const T>` additionally prevents mutation of the referent through that reference.

No alternate C/C++ declarator spellings such as `T *x`, `T* x`, `T &x`, etc. should exist. Strut should have one canonical type spelling.

Reference counting creates a cycle hazard. `weak_ptr<T>` is the explicit safe mechanism for back-references/non-owning links. Add diagnostics for obvious likely cycles where practical.

`raw_ptr<T>` is for FFI, allocators, systems work, and users who explicitly choose unmanaged memory/performance trade-offs:

```strut
unsafe {
    raw_ptr<int> data := ...;
}
```

Safe Strut must prevent use-after-free, dangling safe references, double free, invalid safe dereference, and other lifetime bugs promised by the safe type system.

## 12. Errors

Current proposed checked-error signature syntax:

```strut
function load(string path) -> Config : IOError {
    ...
}

function load(string path) -> Config : (IOError, ParseError) {
    ...
}
```

Throw:

```strut
throw IOError("unable to read file");
```

Catch:

```strut
try {
    config := load("config.json");
}
catch (IOError err) {
    ...
}
catch (ParseError err) {
    ...
}
catch {
    ...
}
```

The compiler should know which checked error types a function may throw from its signature. Exact propagation shorthand is still **provisional** and should be prototyped against filesystem, JSON, networking, and database code before freezing.

## 13. Enums

Allow implicit and explicit integer values:

```strut
enum Status {
    pending = 0,
    running,
    complete,
    failed
}
```

Explicit enum representation rules and richer algebraic/payload enum syntax are **provisional**.

## 14. Switch and match

Support familiar `switch` for straightforward branching:

```strut
switch (status) {
    case pending:
        ...
        break;

    case running:
        ...
        break;

    default:
        ...
}
```

Support `match` where destructuring/pattern matching materially helps:

```strut
match (result) {
    ok(value) => print(value),
    err(error) => print(error)
}
```

Do not force `match` where `switch` is clearer.

## 15. Generics/templates

Use square brackets on generic/template declarations and angle brackets for instantiated types:

```strut
function first[T](T[] items) -> T? {
    ...
}

struct Box[T] {
    T value;
}

Box<int> box;
```

Template identifiers are uppercase (`T`, `X`, `Y`, etc.). Generic lambdas may infer otherwise-undeclared uppercase template identifiers from parameter types. Unknown lowercase/mixed-case type names remain compiler errors.

Prefer ordinary monomorphisation or another predictable implementation strategy. Do not recreate C++ template metaprogramming as an accidental second language.

If compile-time code generation/metaprogramming is added, design it deliberately and transparently rather than through type-system abuse.

## 16. Modules/includes and source organisation

Current direction is C++-familiar syntax with modern semantic inclusion rather than textual preprocessing:

```strut
include <http>
include <sqlite>
include "auth.h"
```

`<...>` denotes packages/system modules; quoted includes denote local/project files.

Includes should behave as language/module dependencies, not C preprocessor text pasting.

Source extensions are **provisional**, but the leading candidate is:

```text
.h   optional header/declaration file
.p   ordinary Strut source/implementation/program file
```

Headers are optional. Small programs may live entirely in one `.p` file; header-only libraries should be possible. Do not require C++-style split compilation ceremony where it buys nothing.

## 17. Streams and console IO

Keep familiar C++-style stream concepts and insertion/extraction operators:

```strut
ofstream ofs(path);
ofs << "hello" << endl;

string name;
in >> name;
out << "hello, " << name << endl;
err << "warning" << endl;
```

Planned stream types include:

```text
istream
ostream
sstream
ifstream
ofstream
```

The standard console streams are `in`, `out`, and `err`. Convenience functions remain available as well:

```strut
print("hello");
name := input();
input(name);
```

`<<` and `>>` are ordinary overloadable operators, allowing user-defined types to participate naturally in streams.

## 18. Operator overloading

Strut should support overloading a fixed language-defined set of operators. Do **not** permit arbitrary new punctuation/text operators or user-defined precedence. Operator precedence and parsing remain fixed by the language for readability, tooling, formatting, and agent friendliness.

Use the full `operator` keyword:

```strut
operator +(Vector a, Vector b) -> Vector {
    ...
}

operator[T] <<(ref<ostream> os, ref<const T> value) -> ref<ostream> {
    ...
}
```

Lambda operator definitions mirror lambda function definitions:

```strut
operator<(vector<int>, vector<int>) -> vector<int>> + :=
    (a, b) => {
        ...
    };

operator[T]<(vector<T>, vector<T>) -> vector<T>> + :=
    (a, b) => {
        ...
    };
```

The operator token occupies the name position. Visually repetitive cases such as overloading `:=` are acceptable if they follow the same grammar consistently.

At minimum, overload useful language-defined arithmetic, comparison, stream/shift, indexing/call, dereference, assignment and initialization operators where doing so does not undermine static semantics. Prefix `*` remains the dereference operator for `ptr<T>` and is overloadable.

`=` may be overloaded for assignment to an existing value. `:=` may be overloaded for typed construction/initialization into new destination storage. The destination of `:=` is not an existing `ref<T>`; the compiler provides construction storage for the declared destination. Inferred `x := value` must not allow an overload to unpredictably choose an unrelated destination type.

Structural language syntax such as member access should remain reserved unless a compelling, well-specified use case justifies overloading it.

## 19. External processes (`exec`)

External program execution belongs in the standard library. Provide a safe argv-based API rather than requiring shell-string construction:

```strut
result := exec("git", ["status", "--short"]);
print(result.stdout);
print(result.stderr);
print(result.exit_code);
```

Support cwd/environment control, inherited or captured stdio, exit status, and errors. The lower-level `process(program, args)` API exposes blocking `in`, `out`, and `err` pipe streams plus `wait()`, `terminate()`, `running()`, and `exit_code()`. `pipe_exec(...)` composes two argv-based processes with an OS pipe and never requires shell interpolation. These APIs are deliberately blocking until the async I/O layer is introduced.

## 20. Static and dynamic linking

Static and dynamic linking are both first-class Strut deployment models. Do not equate the single-binary story with a requirement to statically link everything.

Support:

- fully static builds where the platform/dependencies permit;
- ordinary dynamically linked binaries;
- mixed builds with selected dependencies linked statically or dynamically;
- native static libraries and dynamic libraries (`.a`, `.so`, `.dylib`, `.dll`/import libraries as appropriate);
- C ABI declarations use `extern "C" function name(args) -> type;`; calls cross an explicit `unsafe` boundary;
- package/FFI metadata that can express link mode cleanly.

Executable size is a first-class benchmark dimension alongside speed, memory and startup time. Dead-code elimination and stripped release output should be measured explicitly.

## 21. Concurrency

Strut should provide real OS/native threading, not a JavaScript-style single-threaded event loop pretending to be parallelism. Async functions and async lambdas are scheduled on a shared multithreaded executor with a blocking work queue and worker wakeups. Long-running blocking operations should use explicit threads or purpose-built blocking APIs rather than monopolising executor workers.

Explicit threads:

```strut
worker := thread(work, arg1, arg2);
worker.join();

// join() waits for the native thread and rethrows any worker error on the joining thread.

worker := thread(() => {
    do_work();
});
```

Mutexes are a type with explicit low-level operations:

```strut
mutex mtx;
mtx.lock();
counter++;
mtx.unlock();
```

Also provide safe scoped locking:

```strut
mtx.lock(() => {
    counter++;
});
```

Channels/queues and other synchronization primitives should be added where they materially improve safe concurrent programming.

## 22. Async/await

Async/await should be ergonomic while the runtime is capable of using a real multithreaded executor:

```strut
async function fetch_user(int id) -> User {
    response := await http.get(...);
    return await response.json<User>();
}
```

Async lambdas:

```strut
handler := async (req) => {
    return await process(req);
};
```

Do not make async the only concurrency model. Explicit threads, mutexes, and low-level primitives remain available.

## 23. Stdlib versus packages

Keep the stdlib intentionally small but complete for universal language functionality.

Expected stdlib/core territory includes:

- strings and fundamental collections;
- JSON type plus parse/stringify/typed conversion support;
- fundamental filesystem/IO primitives;
- basic time/error/runtime facilities;
- external process execution (`exec`) and fundamental child-process primitives;
- threading/synchronization primitives that require runtime integration;
- memory/runtime facilities intrinsic to the language.

Broader facilities should live in packages, ideally curated under a dedicated `strut-packages` GitHub organisation. Examples:

- HTTP;
- TLS;
- SQLite;
- PostgreSQL;
- WebSocket;
- JSON Schema;
- higher-level frameworks and integrations.

SQLite should be an official/first-class package used very early for dogfooding, not necessarily compiled into the minimal stdlib.

## 25. CLI direction

The CLI should remain small and direct.

Single-file compile direction:

```sh
strut hello.p
```

This compiles `hello.p` to a native executable (exact output naming/options to be defined).

Project compilation direction:

```sh
strut make
```

Potential explicit/secondary commands may include:

```sh
strut compile hello.p
strut test
strut fmt
strut add <package>
strut --help
strut --version
```

`strut run file` is not a preferred baseline. Compile-and-execute, if added, should have an intentional name/semantics rather than being assumed.

Release and target flags should eventually support optimized builds and cross compilation.

## 26. Deployment target

A major Strut goal is simple native deployment. Strut should be able to produce a single native executable containing application code and embedded frontend/static assets where requested, but must also support small dynamically linked binaries and mixed static/dynamic linking. A production-ish backend should not inherently require Node, a separate static-file server, or a Strut language runtime installed on the target machine.

## 27. Open design questions

Do not silently freeze these without explicit review:

- final type-alias keyword/spelling;
- exact allocation/construction syntax for `ptr<T>` objects;
- exact safe-reference lifetime validation rules;
- exact reference-cycle diagnostics and whether any compile-time cycle prevention is practical;
- concrete-base versus abstract-contract multiple-inheritance rules;
- destructor/resource-cleanup syntax;
- checked-error propagation shorthand;
- richer enum/payload enum semantics;
- package manifest/lockfile format;
- final `.h` / `.p` extension decision;
- exact CLI project/build/package commands;
- compile-time metaprogramming/code-generation model;
- exact async scheduler/runtime model;
- FFI syntax and ABI guarantees;
- exact set of overloadable built-in operators and any deliberately reserved operators;
- package/project syntax for selecting static versus dynamic link mode;
- exact child-process streaming API beyond `exec`.

## Deferred: server-side templating

Nift-style/server-side templating is intentionally not part of the active Strut language plan. It may be reconsidered later if the core language and web/backend ecosystem demonstrate a clear need and the design is fleshed out independently.

## Lambda capture baseline

Lambdas capture referenced outer values by value by default. This makes escaping closures lifetime-safe and keeps captures effectively const. Mutation of external state should be explicit through safe reference/pointer facilities rather than implicit mutable capture. Uppercase undeclared lambda parameter types such as `T` are inferred generic parameters.

## Reference-counted ownership baseline

`ptr<T>` is the ordinary safe owning pointer and is reference counted. Copying a `ptr<T>` shares ownership, release decrements the count, and the object is destroyed deterministically when the last owner disappears. `ptr<T>` may be `null`; `ref<T>` is the non-null borrowing facility. The bootstrap C++20 backend currently maps this contract to `std::shared_ptr` while runtime optimisation remains open. There is no tracing garbage collector.

## Borrow baseline

`ref<T>` is a safe non-owning non-null borrow. `ref<const T>` prevents mutation through the borrow, reference bindings cannot be reseated, and `ref(...)` requires an lvalue. The first lifetime validator is deliberately conservative: ref fields and ref returns are rejected until the compiler can prove those escapes safe. Passing `ref<T>` does not change a `ptr<T>` reference count.

## Weak ownership baseline

`weak_ptr<T>` is the non-owning counterpart to reference-counted `ptr<T>`. Construct it with `weak(ptr_value)`, use `.lock()` to obtain a safe `ptr<T>` when the object is still alive, and `.expired()` to query liveness. The compiler emits a non-fatal warning for obvious two-struct strong reference cycles and recommends a weak back-reference.

## Unsafe/raw pointer baseline

`raw_ptr<T>` is an unmanaged raw pointer and raw operations are restricted to `unsafe { ... }`. `raw(ptr_value)` exposes a non-owning raw address from a safe `ptr<T>` without changing ownership. Raw dereference and pointer arithmetic are only legal inside unsafe blocks. There is no automatic promotion from `raw_ptr<T>` back to owning `ptr<T>`; callers must not manufacture ownership from an unmanaged address. `weak_ptr<T>` must be upgraded with `.lock()` before converting to raw.

### Native library selection

The bootstrap CLI accepts native library search paths and per-library link intent with `--lib`, `--static-lib`, `--dynamic-lib`, and `--lib-path`. Exact library paths are also accepted. Static/dynamic availability remains platform/toolchain dependent; the compiler must diagnose unsupported requests rather than silently changing modes.

### Final executable link modes

`--static` requests a fully static final executable where the platform/toolchain permits it; `--dynamic` requests the ordinary dynamically linked platform model. Per-library `--static-lib`/`--dynamic-lib` selections allow mixed builds. `--release` enables optimisation plus platform-appropriate dead-code elimination and symbol stripping. macOS does not generally support a fully static system executable with the default toolchain, so Strut diagnoses that request rather than pretending it succeeded.

### Package metadata

Projects/packages use JSONIC-parsed `strut.json` metadata. Initial dependency requirements are exact semver, caret, tilde, or `*`; resolution is locked into deterministic `strut.lock.json` entries with immutable revision/content checksums. Package contents are shared in a platform cache (or under `STRUT_HOME`) rather than copied into a per-project `node_modules` equivalent.
