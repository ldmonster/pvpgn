/*
 * pvpgn compatibility shim for libfmt.
 *
 * The codebase originally targeted the vendored fmt v5 (lib/fmt,
 * circa 2018), which:
 *   - auto-formats arbitrary enums via the underlying type, and
 *   - accepts a runtime fmt::string_view as the format argument of
 *     fmt::format / fmt::print without complaint.
 *
 * fmt 9+ removed both of those conveniences.  This header papers over
 * the difference so call sites stay identical regardless of whether
 * the build was wired to the vendored fmt or a modern external one
 * (e.g. vcpkg fmt 10/11/12).
 */
#ifndef INCLUDED_PVPGN_FMT_COMPAT_H
#define INCLUDED_PVPGN_FMT_COMPAT_H

#include <fmt/format.h>
#include <cstddef>
#include <type_traits>

#if FMT_VERSION >= 90000
    /* Wrap a non-constexpr format string so modern fmt accepts it
       without applying compile-time format-string checking. */
    #define PVPGN_FMT_RUNTIME(s) ::fmt::runtime(s)

    namespace fmt {
        /* Generic formatter for any enum: forwards to the underlying
           integer type's formatter.  Matches fmt's 3-parameter primary
           template (T, Char, Enable) by pinning Char=char and using
           the Enable slot for SFINAE on std::is_enum. */
        template <typename E>
        struct formatter<E, char, std::enable_if_t<std::is_enum<E>::value>>
            : formatter<std::underlying_type_t<E>, char>
        {
            template <typename FormatContext>
            auto format(E e, FormatContext& ctx) const
                -> decltype(ctx.out())
            {
                return formatter<std::underlying_type_t<E>, char>::format(
                    static_cast<std::underlying_type_t<E>>(e), ctx);
            }
        };

        /* The legacy d2dbs / bnetd code stores IP-as-string in
           `unsigned char[16]` fields and logs them through {} format
           specifiers.  Old fmt v5 accepted those; fmt 9+ does not.
           These specializations forward to the const-char* formatter
           so the buffer is rendered as a NUL-terminated C string,
           matching the historical behavior. */
        template <std::size_t N>
        struct formatter<unsigned char[N], char>
            : formatter<const char*, char>
        {
            template <typename FormatContext>
            auto format(const unsigned char (&arr)[N], FormatContext& ctx) const
                -> decltype(ctx.out())
            {
                return formatter<const char*, char>::format(
                    reinterpret_cast<const char*>(arr), ctx);
            }
        };
    }
#else
    /* Old vendored fmt v5: format strings are accepted as-is, and
       enums already format implicitly via their underlying type. */
    #define PVPGN_FMT_RUNTIME(s) (s)
#endif

#endif /* INCLUDED_PVPGN_FMT_COMPAT_H */
