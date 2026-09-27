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

## API

```cpp
dbg::debug(x)        // pointers print their address (null/cycle safe)
dbg::debug<true>(x)  // pointers are dereferenced (null still prints "null")
```

`Deref` is a compile-time flag propagated down the recursion. The whole
implementation lives in private members of the `std::formatter<dbg::Debug<...>>`
specialization, so no other translation unit can name or reach it.

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
