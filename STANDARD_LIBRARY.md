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

## Filesystem

Enable with `include <filesystem>;`. Functions include `exists`, `is_file`, `is_dir`, `file_size`, `modified`, `make_dir`, `remove`, `remove_all`, `copy`, `move`, `touch`, `ls`, `walk`, `cwd`, `cd`, `absolute`, `canonical`, `parent`, `filename`, `extension`, `stem`, `join_path`, `read_file`, `read_bytes`, `write_file`, and `append_file`.

`remove` removes one file or an empty directory. `remove_all` recursively removes a tree. Both accept a scalar path or a vector/array of paths. `copy([a,b], dest)` and `move([a,b], dest)` place each source under the existing destination directory using its basename.

`read_file` performs a size-aware single-allocation bulk read. `read_bytes` returns `bytes` (`uint_8` storage). `write_file` and `append_file` accept either text or bytes. Stream APIs remain available for incremental I/O.

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
