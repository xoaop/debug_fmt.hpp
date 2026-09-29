#pragma once

// debug_fmt.hpp — generic Debug printer. Everything is data; a small set of
// structural rules dispatched by if constexpr, no per-type special cases:
//   0 char[N] : length-bounded string (no C-string over-read)
//   1 leaf    : anything std::formattable — print directly
//   2 enum    : reflect to a name, else the underlying value
//   3 range   : [e, ...], recurse each element
//   4 pointer : address by default; Deref follows it (null prints "null").
//               char*/void* fall through to leaf.
//   5 class   : recurse direct bases (incl. private), then all data members
//   6 union   : replay the bytes as each member type; skip non-trivial members
//   7 tag-union : via dbg::TagUnionTrait<T>: shared fields + the live arm only
//
// Public API: dbg::debug(x) prints pointer addresses; dbg::debug<true>(x)
// follows them. Deref is a compile-time flag propagated down the recursion.
// Also public, usable on their own: dbg::to_string(enum) yields the reflected
// name as const char*, dbg::string_tag_union_val(x[, Deref]) yields the live
// arm formatted as std::string. The printer's recursion lives in private
// members of the std::formatter specialization below — no other translation
// unit can name or reach it.
// Requires P2996 + P1306 (GCC: -freflection).

#include <meta>
#include <bit>
#include <cstddef>
#include <cstring>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dbg {






// The handle users get from debug(); only meaningful when passed to std::format.
// Holds a reference only, never copies the printed object.
template <typename T, bool Deref = false>
struct Debug {
    const T& ref;
};

// The only entry point: dbg::debug(x) / dbg::debug<true>(x).
template <bool Deref = false, typename T>
constexpr Debug<T, Deref> debug(const T& value) {
    return Debug<T, Deref>{value}; 
}

template <typename T> requires (std::is_enum_v<T>)
constexpr const char *to_string(const T& value) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^T))) {
        if (value == [:e:]) {
            return std::define_static_string(std::meta::identifier_of(e));
        }
    }

    return "<unknown>";
}


// TagUnionTrait — 为「tagged union」类型特化：显式声明判别式 tag() 与 tag→成员 的
// 映射 union_val<E>()。库不对字段顺序/命名做任何假设。未特化即非 tagged union。
template <typename T> struct TagUnionTrait;

template <typename T>
concept TagUnion = requires (const T& x) { dbg::TagUnionTrait<T>::tag(x); };

// 独立自由函数：返回 tagged union 当前活跃臂格式化后的字符串（只是活跃成员的值，
// 不含类型名/共享字段）。遍历 tag 枚举匹配，取对应成员交给 debug 打印。
// Deref 透传给 debug：活跃臂里的指针是否解引用跟随调用方。
template <TagUnion T, bool Deref = false>
std::string string_tag_union_val(const T& x) {
    using TR = dbg::TagUnionTrait<T>;
    std::string result;

    template for (constexpr auto e :
                  std::define_static_array(std::meta::enumerators_of(
                      ^^decltype(TR::tag(x))))) {
        if (TR::tag(x) == [:e:]) {
            result = std::format("{}", dbg::debug<Deref>(TR::template union_val<([:e:])>(x)));
        }
    }

    return result;
}


} // namespace dbg

template <typename T, bool Deref>
struct std::formatter<dbg::Debug<T, Deref>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin(); // only the empty spec "{}" is supported
    }

    auto format(const dbg::Debug<T, Deref>& d, auto& ctx) const {
        namespace meta = std::meta;
        const T& o = d.ref;

        if constexpr (std::is_array_v<T> &&
                      std::is_same_v<std::remove_cv_t<std::remove_extent_t<T>>, char>) {
            // 0) char[N]: bound to the array length; a non-terminated array
            //    (e.g. union byte replay) must not be read as a C-string.
            std::string_view sv(o, std::extent_v<T>);
            if (auto z = sv.find('\0'); z != std::string_view::npos) {
                sv = sv.substr(0, z);
            }

            return std::format_to(ctx.out(), "{}", sv);

        } else if constexpr (requires (std::formatter<std::remove_cvref_t<T>, char> f,
                                       const std::remove_cvref_t<T>& v,
                                       std::format_context& fc) { f.format(v, fc); }) {
            // 1) leaf: 有可用 formatter 就直接交给它。不用 std::formattable——
            //    libstdc++ 对 __int128 等有 formatter 的类型会误判为不可格式化。
            return std::format_to(ctx.out(), "{}", o);

        } else if constexpr (std::is_enum_v<T>) {
            // 2) enum: name if matched, else the underlying value
            template for (constexpr auto e :
                          std::define_static_array(meta::enumerators_of(^^T))) {
                if (o == [:e:]) {
                    return std::format_to(ctx.out(), "{}", meta::identifier_of(e));
                }
            }

            return std::format_to(ctx.out(), "{}", std::to_underlying(o));

        } else if constexpr (std::ranges::range<T>) {
            // 3) range: recurse each element
            auto out = std::format_to(ctx.out(), "[");
            bool first = true;

            for (const auto& e : o) {
                out = std::format_to(out, "{}{}", first ? "" : ", ", wrap<Deref>(e));
                first = false;
            }

            return std::format_to(out, "]");

        } else if constexpr (std::is_pointer_v<T>) {
            // 4) pointer: address by default; Deref follows it, never null
            if constexpr (Deref && std::is_object_v<std::remove_pointer_t<T>>) {
                if (o == nullptr) {
                    return std::format_to(ctx.out(), "null");
                }
                return std::format_to(ctx.out(), "&{}", wrap<Deref>(*o));
            } else {
                return std::format_to(ctx.out(), "{}",
                                      reinterpret_cast<const void*>(o));
            }

        } else if constexpr (dbg::TagUnion<T>) {
            // tagged union：打印类型名 + 各具名共享字段（判别式枚举也会一并显示）
            // + 当前活跃臂（复用 string_tag_union_val，只读活跃成员，无 UB）。
            auto out = std::format_to(ctx.out(), "{} {{", meta::display_string_of(^^T));
            bool first = true;

            template for (constexpr auto m :
                          std::define_static_array(meta::nonstatic_data_members_of(
                              ^^T, meta::access_context::unchecked()))) {
                // 具名成员 = 共享字段；匿名 union 无名字，走下面的活跃臂
                if constexpr (meta::has_identifier(m)) {
                    out = std::format_to(out, "{}{} = {}", first ? " " : ", ",
                                         meta::identifier_of(m), wrap<Deref>(o.[:m:]));
                    first = false;
                }
            }

            // 活跃臂：复用 string_tag_union_val（内部遍历 tag 枚举并格式化活跃成员）；
            // 透传 Deref，让活跃臂里的指针一样跟随 debug<true>。
            out = std::format_to(out, "{}{}", first ? " " : ", ",
                                 dbg::string_tag_union_val<T, Deref>(o));

            return std::format_to(out, " }}");

        } else if constexpr (std::is_union_v<T>) {
            // 6) union: reinterpret the bytes as each member type. Reading an
            //    inactive member directly is UB, so format_union_member copies
            //    the bytes out instead.
            auto out = std::format_to(ctx.out(), "{} {{",
                                      meta::display_string_of(^^T));
            bool first = true;

            template for (constexpr auto m :
                          std::define_static_array(meta::nonstatic_data_members_of(
                              ^^T, meta::access_context::unchecked()))) {
                out = format_union_member<m>(out, o, first);
                first = false;
            }

            return std::format_to(out, " }}");

        } else if constexpr (std::is_class_v<T>) {
            // 5) class: recurse direct bases, then every data member (incl. private)
            auto out = std::format_to(ctx.out(), "{} {{",
                                      meta::display_string_of(^^T));
            bool first = true;

            template for (constexpr auto base :
                          std::define_static_array(meta::bases_of(
                              ^^T, meta::access_context::unchecked()))) {
                // Splicing the base subobject bypasses access control, so private
                // bases work where static_cast would fail on an inaccessible base.
                out = std::format_to(out, "{}{}", first ? " " : ", ",
                                     wrap<Deref>(o.[:base:]));
                first = false;
            }

            template for (constexpr auto m :
                          std::define_static_array(meta::nonstatic_data_members_of(
                              ^^T, meta::access_context::unchecked()))) {
                if constexpr (meta::has_identifier(m)) {
                    out = std::format_to(out, "{}{} = {}", first ? " " : ", ",
                                         meta::identifier_of(m), wrap<Deref>(o.[:m:]));
                } else {
                    // 匿名成员（如匿名 union）无名字，直接递归其内容
                    out = std::format_to(out, "{}{}", first ? " " : ", ",
                                         wrap<Deref>(o.[:m:]));
                }
                first = false;
            }

            return std::format_to(out, " }}");

        } else {
            static_assert(false, "dbg::debug: cannot format this type");
        }
    }

private:
    // Implementation — private members, unreachable from other translation units.

    // Re-wrap a child value, propagating Deref down the recursion.
    template <bool D, typename U>
    static dbg::Debug<U, D> wrap(const U& v) {
        return dbg::Debug<U, D>{v};
    }

    // One union member: replay the shared bytes as this member's type. M comes
    // from the reflection template parameter, not a local `using M = [:...:]`
    // inside template for — that miscompiles on GCC 16.1.0 (every member takes
    // the first member's type; a minimal case even ICEs).
    template <std::meta::info Member, typename U, typename Out>
    static Out format_union_member(Out out, const U& o, bool first) {
        namespace meta = std::meta;
        using M = [:meta::type_of(Member):];

        out = std::format_to(out, "{}{} as {} = ", first ? " " : ", ",
                             meta::identifier_of(Member),
                             meta::display_string_of(meta::type_of(Member)));

        if constexpr (std::is_trivially_copyable_v<M>) {
            // bit_cast rebuilds the value without construction — the only defined
            // way to read an inactive union member. Box wraps M for array /
            // non-default-constructible members.
            struct Box {
                M value;
            };

            unsigned char buf[sizeof(Box)] = {};
            std::memcpy(buf, &o, sizeof(M) < sizeof(Box) ? sizeof(M) : sizeof(Box));
            Box b = std::bit_cast<Box>(buf);

            // Replayed bytes are bogus pointers; force Deref=false.
            out = std::format_to(out, "{}", wrap<false>(b.value));
        } else {
            out = std::format_to(out, "<non-trivial, skipped>");
        }

        return out;
    }
};
