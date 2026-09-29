# debug_fmt

A single-header, generic `Debug` printer for C++26. Everything is treated as
data: a small set of structural rules — dispatched by `if constexpr` and driven
by static reflection — cover arbitrary types with no per-type special cases.

```cpp
#include <debug_fmt.hpp>
#include <print>

enum class Mode { Idle, Running, Done };
struct AppState { int frame; Mode mode; };

std::println("{}", dbg::debug(AppState{3, Mode::Done}));
// AppState { frame = 3, mode = Done }
```

## Rules

| # | kind    | behaviour                                                        |
|---|---------|-----------------------------------------------------------------|
| 0 | char[N] | length-bounded string (no C-string over-read)                   |
| 1 | leaf    | anything `std::formattable` — printed directly                  |
| 2 | enum    | reflected to a name, else the underlying value                  |
| 3 | range   | `[e, ...]`, each element recursed                               |
| 4 | pointer | address by default; `debug<true>` follows it (`null` when null) |
| 5 | class   | direct bases (incl. private) then all data members (incl. private) |
| 6 | union   | the bytes replayed as each member type; non-trivial ones skipped |
| 7 | tag-union | via `dbg::TagUnionTrait<T>`: shared fields + the live arm only |

## Tagged unions

A type becomes a *tagged union* by specializing `dbg::TagUnionTrait<T>`: say how
to read the discriminator (`tag`) and how each enumerator maps to a payload
member (`union_val<E>`). The library assumes nothing about field names or
layout, and only ever reads the **active** member — never an inactive one.

```cpp
enum class ValueKind { Num, Text };
struct Value {
    ValueKind kind;
    union { int num; const char* text; };
};

template <> struct dbg::TagUnionTrait<Value> {
    static ValueKind tag(const Value& v) { return v.kind; }
    template <ValueKind K> static decltype(auto) union_val(const Value& v) {
        if constexpr (K == ValueKind::Num) return (v.num);
        else return (v.text);
    }
};

Value v{}; v.kind = ValueKind::Num; v.num = 42;
std::println("{}", dbg::debug(v));          // Value { kind = Num, 42 }
```

Without a specialization a type simply falls through to the generic `union` /
`class` rules.

## API

Development printing — best-effort inspection, output not stable across versions:

```cpp
dbg::debug(x)        // pointers print their address (null / cycle safe)
dbg::debug<true>(x)  // pointers are dereferenced (null still prints "null")
```

`Deref` is a compile-time flag propagated down the recursion.

Standalone free functions — each defined by its own interface, independent of the
printer:

```cpp
dbg::to_string(e)             // enum -> reflected name as const char* (stable, no allocation)
dbg::string_tag_union_val(x)  // tagged union -> the live arm's value, formatted, as std::string
```

`to_string` is aimed at release paths that need stable string data; `debug` is
for development-time inspection. `string_tag_union_val` accepts an optional
`Deref` argument: `dbg::string_tag_union_val<T, true>(x)`.

The printer's recursion lives in private members of the
`std::formatter<dbg::Debug<...>>` specialization, so no other translation unit
can name or reach it; the free functions above are the only other entry points.

## Requirements

- **GCC ≥ 16** with `-freflection` (P2996 static reflection + P1306 expansion
  statements). Clang/MSVC do not implement this yet.
- C++26 (`-std=c++26`).

## Use with CMake

```cmake
add_subdirectory(debug_fmt)          # e.g. a submodule at vendor/debug_fmt
target_link_libraries(app PRIVATE debug_fmt::debug_fmt)
```

The include path and (on GCC) `-freflection` propagate through the interface
target, so consumers don't need to set them by hand.
