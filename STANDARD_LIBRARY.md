# Strut Standard Library Modules

Standard-library facilities are explicitly enabled with Strut module includes such as `include <vector>;`. These names are Strut modules, not direct requests to include C++ headers; the native backend is free to implement them differently.

## Collections

| Strut type | Current native implementation | Notes |
| --- | --- | --- |
| `vector<T>` / `T[]` | `std::vector<T>` | contiguous dynamic array |
| `deque<T>` | `std::deque<T>` | efficient insertion/removal at both ends |
| `list<T>` | `std::list<T>` | doubly-linked list |
| `map<K,V>` | `std::unordered_map<K,V>` | average O(1) lookup, no iteration-order guarantee |
| `set<T>` | `std::unordered_set<T>` | average O(1) lookup, no iteration-order guarantee |
| `ordered_map<K,V>` | `std::map<K,V>` | sorted iteration, O(log n) lookup |
| `ordered_set<T>` | `std::set<T>` | sorted iteration, O(log n) lookup |
| `queue<T>` | `std::queue<T>` | FIFO |
| `stack<T>` | `std::stack<T>` | LIFO |
| `priority_queue<T>` | `std::priority_queue<T>` | max-first by default |
| `priority_queue<T,min>` | priority queue using `greater<T>` | min-first |

Modules are named after the Strut surface type: `vector`, `deque`, `list`, `map`, `set`, `ordered_map`, `ordered_set`, `queue`, `stack`, `priority_queue`, and `tuple`.

## Bytes

`bytes` is the core owned binary value. `bytes()` creates an empty value and `bytes(size)` creates a zero-filled value. A contextually typed literal such as `bytes packet := [0, 127, 255];` accepts `uint_8` elements. Values deep-copy on assignment, compare by contents, and support mutable integer indexing, `length()`, `empty()`, and the half-open copying operation `slice(begin, end)`.

Index and slice bounds are checked at runtime. Lengths and indexes use `int_64`; the addressable limit is the smaller of the platform container limit and `int_64` maximum. `bytes.from_string(text)` and `value.to_string()` are explicit, lossless copies of string code units. They do not validate, decode, or imply UTF-8, and there is no implicit string/bytes conversion.

## Binary streams

The existing `istream` and `ostream` types are the generic input and output contracts; there is no second reader/writer type family. `ifstream` and `ofstream` implement those contracts, while retaining their existing text and formatted operations. Process `in`, `out`, and `err` pipes expose the same binary method semantics structurally without becoming nominal file/console stream subtypes.

`read_bytes(max_bytes)` returns at most the requested bytes. A zero-size read returns empty bytes without changing EOF. An empty result denotes EOF only when `eof()` is also true; repeated reads after observed EOF remain empty. `read_all_bytes(limit?)` reads to EOF and treats its optional non-negative limit as a hard maximum. Reads after close raise `StreamError`, or `ExecError` for process pipes.

`write_bytes(value)` has complete-write-or-error semantics, including internal retries for partial native writes. It does not flush implicitly. `flush()` reports native flush failures, while flushing after close is a defined no-op. `close()` is idempotent; later writes fail. File streams must be opened with `binary=true` when byte-exact behavior is required on platforms with text-mode translation.

## Cancellation

`cancellation_source` owns the authority to request cancellation. `source.token()` returns a cheap copyable `cancellation_token` that observes the same shared state. Sources are also copyable; all copies retain cancellation authority over that state. Destroying a source does not cancel, tokens remain valid after every source is destroyed, and `cancel()` is one-way, thread-safe, and idempotent.

`token.cancelled()` performs non-blocking observation. `token.wait()` sleeps without busy-spinning until cancellation is requested. `token.throw_if_cancelled()` raises the checked `CancellationError`. Tokens cannot reset or request cancellation. CP6 represents only explicit cancellation; the internal terminal-state representation can gain timeout, disconnect, or shutdown reasons compatibly when those producers are introduced.

Native runtime facilities may subscribe to a token to wake blocking operations. Subscription is intentionally not public: registration cannot miss concurrent cancellation, removal waits for an in-flight callback, and callbacks run without the cancellation-state lock held.

## Filesystem

Enable with `include <filesystem>;`. Functions include `exists`, `is_file`, `is_dir`, `file_size`, `modified`, `make_dir`, `remove`, `remove_all`, `copy`, `move`, `touch`, `ls`, `walk`, `cwd`, `cd`, `absolute`, `canonical`, `parent`, `filename`, `extension`, `stem`, `join_path`, `read_file`, `read_bytes`, `write_file`, and `append_file`.

`remove` removes one file or an empty directory. `remove_all` recursively removes a tree. Both accept a scalar path or a vector/array of paths. `copy([a,b], dest)` and `move([a,b], dest)` place each source under the existing destination directory using its basename.

`read_file` performs a size-aware single-allocation bulk read. `read_bytes` returns `bytes`. `write_file` and `append_file` accept either text or bytes. Stream APIs remain available for incremental I/O.

Filesystem failures use the checked `FilesystemError` type.

## Tuples

Enable tuples with `include <tuple>;`. Tuple types are heterogeneous and fixed-size:

```strut
include <tuple>;
tuple<double,int> t := (2.0, 1);
tuple<int> one := (7,);
print(t[0]);
```

`(x)` remains ordinary grouping; a one-element tuple therefore uses the conventional trailing comma `(x,)`. Tuple indexing is compile-time and requires an integer literal, which keeps each access statically typed.

## Include spelling

Angle brackets are the canonical spelling for Strut standard-library modules and package dependencies: `include <vector>;`, `include <filesystem>;`, `include <sqlite>;`. This is familiar C/C++-style syntax but it does **not** mean "inject a C++ header". Quoted includes such as `include "mylib.h";` are local Strut/native source dependencies resolved relative to the including source file.

## Filesystem wildcards

The source operands of `copy`, `move`, `remove`, and `remove_all` accept `*`, `?`, and recursive `**` wildcards. Destinations are always literal paths. A wildcard `copy`/`move` requires an existing destination directory; no-match copy/move is an error, while no-match removal is a no-op. Vector forms may mix literal paths and patterns.

## Complexity summary

- `vector`: amortized O(1) push-back, O(1) indexing.
- `deque`: O(1) push/pop at either end, O(1) indexing.
- `list`: O(1) end insertion/removal, linear traversal.
- `map` / `set`: hash based; average O(1) lookup/insert, no iteration-order guarantee.
- `ordered_map` / `ordered_set`: ordered tree containers; O(log n) lookup/insert and sorted iteration.
- `queue` / `stack`: O(1) adaptor operations.
- `priority_queue`: O(log n) push/pop and O(1) top; max-first by default, `priority_queue<T,min>` for min-first.
- `tuple`: fixed-size heterogeneous product type; literal-index access is compile-time.
