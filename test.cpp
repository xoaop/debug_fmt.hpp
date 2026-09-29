// test.cpp — smoke test / usage gallery for debug_fmt.
// Exercises every structural rule and its edge cases; eyeball the output.
// Pointer addresses are non-reproducible, everything else is stable.
//
// Built automatically when debug_fmt is the top-level CMake project:
//   cmake -B build && cmake --build build && ./build/debug_fmt_test

#include <debug_fmt.hpp>

#include <print>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <variant>
#include <utility>

namespace smoke {

enum Color { Red, Green = 10, Blue };          // unscoped + explicit value (Blue == 11)
enum class Dir { North, East, South, West };
enum class Alias { A = 1, B = 1 };             // duplicate value: first same-value name wins

struct Empty {};                               // -> "Empty { }"

struct Base { int b = 100; };
struct Mid : Base { char tag = 'M'; };
struct Derived : Mid { double d = 2.5; };      // multi-level inheritance: bases recursed

struct Secret { int pub = 1; private: int hidden = 42; };  // private members printed too

struct WithPtr { int* p; const char* name; };  // pointer members: Deref propagation

union Bytes { int i; float f; char c[4]; };    // trivially copyable: one byte pattern, many views

union Mixed {                                  // non-trivial member -> skipped
    int i; std::string s;
    Mixed() : i(0) {}
    ~Mixed() {}
};

// tagged union: shared discriminator + payload in an anonymous union. The
// library knows nothing about layout/naming — tagged-ness comes from the
// dbg::TagUnionTrait specialization below, not from any field convention.
enum class ValueKind { Num, Text, Ptr };
struct Value {
    ValueKind kind;
    union {
        int num;
        const char* text;
        int* ptr;
    };
};

} // namespace smoke

// Opt a type into tagged-union printing: say how to read the discriminator, and
// how each enumerator maps to a payload member. Only the matching arm is ever
// read (no UB) — the inactive members are never touched.
template <> struct dbg::TagUnionTrait<smoke::Value> {
    static smoke::ValueKind tag(const smoke::Value& v) { return v.kind; }
    template <smoke::ValueKind K> static decltype(auto) union_val(const smoke::Value& v) {
        if constexpr (K == smoke::ValueKind::Num) return (v.num);
        else if constexpr (K == smoke::ValueKind::Text) return (v.text);
        else return (v.ptr);
    }
};

int main() {
    using namespace smoke;

    std::println("---- 1. leaf (arithmetic / bool / char / string) ----");
    std::println("{}", dbg::debug(-7));
    std::println("{}", dbg::debug(3.14159));
    std::println("{}", dbg::debug(true));
    std::println("{}", dbg::debug('A'));
    std::println("{}", dbg::debug(std::string("hi")));
    std::println("{}", dbg::debug(std::string_view("sv")));
    const char* cs = "c-string";
    std::println("{}", dbg::debug(cs));            // const char* -> leaf (string)

    std::println("---- 2. enum (name / unmatched / alias) ----");
    std::println("{}", dbg::debug(Green));         // explicit value -> name
    std::println("{}", dbg::debug(Blue));          // == 11 -> name
    std::println("{}", dbg::debug(Dir::West));
    std::println("{}", dbg::debug(static_cast<Dir>(99))); // unmatched -> underlying 99
    std::println("{}", dbg::debug(Alias::B));      // alias -> first same-value name

    std::println("---- 3. range (only non-formattable ranges land here) ----");
    int carr[3] = {1, 2, 3};
    std::println("{}", dbg::debug(carr));          // C array
    char chars[3] = {'a', 'b', 'c'};
    std::println("{}", dbg::debug(chars));         // char array -> formattable -> leaf (string)
    std::println("{}", dbg::debug(std::vector<int>{4, 5, 6}));
    std::println("{}", dbg::debug(std::vector<int>{}));   // empty -> []
    std::println("{}", dbg::debug(std::vector<Dir>{Dir::North, Dir::East})); // elems -> enum names
    std::println("{}", dbg::debug(std::vector<std::vector<int>>{{1}, {2, 3}})); // nested
    std::println("---- 4. pointer (address / deref / null) ----");
    int x = 7;
    int* px = &x;
    std::println("{}", dbg::debug(px));            // address (non-reproducible)
    std::println("{}", dbg::debug<true>(px));      // deref -> &7
    std::println("{}", dbg::debug<true>(static_cast<int*>(nullptr))); // -> null
    void* pv = &x;
    std::println("{}", dbg::debug(pv));            // void* -> leaf (address)
    WithPtr wp{&x, "obj"};
    std::println("{}", dbg::debug(wp));            // member pointer prints address
    std::println("{}", dbg::debug<true>(wp));      // Deref propagates: p -> &7, name stays a string

    std::println("---- 5. class (empty / inheritance / private) ----");
    std::println("{}", dbg::debug(Empty{}));
    std::println("{}", dbg::debug(Base{}));
    std::println("{}", dbg::debug(Derived{}));     // multi-level inheritance
    std::println("{}", dbg::debug(Secret{}));      // private member

    std::println("---- 6. union (one byte pattern, many views) ----");
    Bytes by; by.i = 0x41424344;
    std::println("{}", dbg::debug(by));            // int / float / char[4] each interpreted
    Mixed mx;                                      // i = 0
    std::println("{}", dbg::debug(mx));            // std::string member -> skipped

    std::println("---- 7. std composites (hard cases: private bases / inner unions) ----");
    std::println("{}", dbg::debug(std::pair<int, char>{1, 'z'}));
    std::println("{}", dbg::debug(std::optional<int>{5}));
    std::println("{}", dbg::debug(std::optional<int>{}));   // empty optional
    std::println("{}", dbg::debug(std::variant<int, Dir>{Dir::South}));

    std::println("---- 8. tagged union (TagUnionTrait) ----");
    Value vn{}; vn.kind = ValueKind::Num;  vn.num = 42;
    Value vt{}; vt.kind = ValueKind::Text; vt.text = "hello";
    int n = 7;
    Value vp{}; vp.kind = ValueKind::Ptr;  vp.ptr = &n;
    std::println("{}", dbg::debug(vn));            // discriminator + active arm (num)
    std::println("{}", dbg::debug(vt));            // active arm is a C-string
    std::println("{}", dbg::debug(vp));            // active arm pointer -> address
    std::println("{}", dbg::debug<true>(vp));      // Deref reaches the active arm -> &7
    std::println("{}", dbg::debug(std::vector<Value>{vn, vt})); // tagged unions as range elems
}
