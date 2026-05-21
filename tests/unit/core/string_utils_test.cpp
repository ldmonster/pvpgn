// SPDX-License-Identifier: GPL-2.0-or-later
// Catch2 unit tests for core/string_utils.hpp
// Migrated from src/common/util.{h,cpp} (R103).

#include <ctime>
#include <optional>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "core/string_utils.hpp"

using namespace pvpgn::core;

// ===========================================================================
// str_starts_with_word
// ===========================================================================

TEST_CASE("str_starts_with_word: exact match", "[core][string_utils]") {
    REQUIRE(str_starts_with_word("hello", "hello"));
}

TEST_CASE("str_starts_with_word: prefix followed by space", "[core][string_utils]") {
    REQUIRE(str_starts_with_word("hello world", "hello"));
}

TEST_CASE("str_starts_with_word: prefix followed by tab", "[core][string_utils]") {
    REQUIRE(str_starts_with_word("hello\tworld", "hello"));
}

TEST_CASE("str_starts_with_word: case-insensitive match", "[core][string_utils]") {
    REQUIRE(str_starts_with_word("HELLO world", "hello"));
    REQUIRE(str_starts_with_word("hello world", "HELLO"));
    REQUIRE(str_starts_with_word("HeLLo world", "hElLo"));
}

TEST_CASE("str_starts_with_word: no match — different prefix", "[core][string_utils]") {
    REQUIRE_FALSE(str_starts_with_word("world hello", "hello"));
}

TEST_CASE("str_starts_with_word: no match — prefix is substring without word boundary",
          "[core][string_utils]") {
    // "helloworld" starts with "hello" but next char is 'w', not space/tab/end
    REQUIRE_FALSE(str_starts_with_word("helloworld", "hello"));
}

TEST_CASE("str_starts_with_word: full shorter than part", "[core][string_utils]") {
    REQUIRE_FALSE(str_starts_with_word("hi", "hello"));
}

TEST_CASE("str_starts_with_word: empty part matches any full", "[core][string_utils]") {
    // empty part: full.size() >= 0 always, and full[0] is checked as boundary
    // Legacy strstart("anything", "") returns 0 (match) when full[0] is space/NUL
    // Our impl: part.size()==0, full.size()>=0, full.size()==part.size() only if
    // full is also empty; otherwise next char is full[0].
    // For "hello", next char is 'h' — not a boundary → false.
    REQUIRE_FALSE(str_starts_with_word("hello", ""));
    // For empty full and empty part → exact match → true
    REQUIRE(str_starts_with_word("", ""));
}

// ===========================================================================
// str_reverse
// ===========================================================================

TEST_CASE("str_reverse: basic reversal", "[core][string_utils]") {
    REQUIRE(str_reverse("hello") == "olleh");
}

TEST_CASE("str_reverse: single character", "[core][string_utils]") {
    REQUIRE(str_reverse("x") == "x");
}

TEST_CASE("str_reverse: empty string", "[core][string_utils]") {
    REQUIRE(str_reverse("") == "");
}

TEST_CASE("str_reverse: palindrome", "[core][string_utils]") {
    REQUIRE(str_reverse("abcba") == "abcba");
}

TEST_CASE("str_reverse: does not modify original", "[core][string_utils]") {
    std::string_view sv = "hello";
    const std::string rev = str_reverse(sv);
    REQUIRE(rev == "olleh");
    REQUIRE(sv == "hello"); // original unchanged
}

// ===========================================================================
// str_to_uint
// ===========================================================================

TEST_CASE("str_to_uint: basic integer", "[core][string_utils]") {
    REQUIRE(str_to_uint("42") == 42u);
}

TEST_CASE("str_to_uint: zero", "[core][string_utils]") {
    REQUIRE(str_to_uint("0") == 0u);
}

TEST_CASE("str_to_uint: leading whitespace", "[core][string_utils]") {
    REQUIRE(str_to_uint("  123") == 123u);
    REQUIRE(str_to_uint("\t456") == 456u);
}

TEST_CASE("str_to_uint: leading plus sign", "[core][string_utils]") {
    REQUIRE(str_to_uint("+99") == 99u);
    REQUIRE(str_to_uint("  +7") == 7u);
}

TEST_CASE("str_to_uint: max uint", "[core][string_utils]") {
    REQUIRE(str_to_uint("4294967295") == 4294967295u);
}

TEST_CASE("str_to_uint: overflow returns nullopt", "[core][string_utils]") {
    REQUIRE_FALSE(str_to_uint("4294967296").has_value());
    REQUIRE_FALSE(str_to_uint("99999999999").has_value());
}

TEST_CASE("str_to_uint: non-digit returns nullopt", "[core][string_utils]") {
    REQUIRE_FALSE(str_to_uint("abc").has_value());
    REQUIRE_FALSE(str_to_uint("12abc").has_value());
    REQUIRE_FALSE(str_to_uint("-1").has_value());
}

TEST_CASE("str_to_uint: empty string returns nullopt", "[core][string_utils]") {
    REQUIRE_FALSE(str_to_uint("").has_value());
    REQUIRE_FALSE(str_to_uint("   ").has_value());
}

// ===========================================================================
// str_to_ushort
// ===========================================================================

TEST_CASE("str_to_ushort: basic value", "[core][string_utils]") {
    REQUIRE(str_to_ushort("1234") == static_cast<unsigned short>(1234));
}

TEST_CASE("str_to_ushort: max ushort", "[core][string_utils]") {
    REQUIRE(str_to_ushort("65535") == static_cast<unsigned short>(65535));
}

TEST_CASE("str_to_ushort: overflow returns nullopt", "[core][string_utils]") {
    REQUIRE_FALSE(str_to_ushort("65536").has_value());
    REQUIRE_FALSE(str_to_ushort("100000").has_value());
}

TEST_CASE("str_to_ushort: invalid returns nullopt", "[core][string_utils]") {
    REQUIRE_FALSE(str_to_ushort("abc").has_value());
}

// ===========================================================================
// str_get_bool
// ===========================================================================

TEST_CASE("str_get_bool: true values", "[core][string_utils]") {
    REQUIRE(str_get_bool("true")  == true);
    REQUIRE(str_get_bool("TRUE")  == true);
    REQUIRE(str_get_bool("True")  == true);
    REQUIRE(str_get_bool("yes")   == true);
    REQUIRE(str_get_bool("YES")   == true);
    REQUIRE(str_get_bool("on")    == true);
    REQUIRE(str_get_bool("ON")    == true);
    REQUIRE(str_get_bool("1")     == true);
}

TEST_CASE("str_get_bool: false values", "[core][string_utils]") {
    REQUIRE(str_get_bool("false") == false);
    REQUIRE(str_get_bool("FALSE") == false);
    REQUIRE(str_get_bool("False") == false);
    REQUIRE(str_get_bool("no")    == false);
    REQUIRE(str_get_bool("NO")    == false);
    REQUIRE(str_get_bool("off")   == false);
    REQUIRE(str_get_bool("OFF")   == false);
    REQUIRE(str_get_bool("0")     == false);
}

TEST_CASE("str_get_bool: unrecognised returns nullopt", "[core][string_utils]") {
    REQUIRE_FALSE(str_get_bool("maybe").has_value());
    REQUIRE_FALSE(str_get_bool("2").has_value());
    REQUIRE_FALSE(str_get_bool("").has_value());
    REQUIRE_FALSE(str_get_bool("yep").has_value());
}

// ===========================================================================
// seconds_to_timestr
// ===========================================================================

TEST_CASE("seconds_to_timestr: seconds only", "[core][string_utils]") {
    REQUIRE(seconds_to_timestr(0)  == "0 seconds.");
    REQUIRE(seconds_to_timestr(1)  == "1 second.");
    REQUIRE(seconds_to_timestr(59) == "59 seconds.");
}

TEST_CASE("seconds_to_timestr: minutes and seconds", "[core][string_utils]") {
    REQUIRE(seconds_to_timestr(60)  == "1 minute 0 seconds");
    REQUIRE(seconds_to_timestr(61)  == "1 minute 1 second");
    REQUIRE(seconds_to_timestr(121) == "2 minutes 1 second");
    REQUIRE(seconds_to_timestr(122) == "2 minutes 2 seconds");
}

TEST_CASE("seconds_to_timestr: hours, minutes, seconds", "[core][string_utils]") {
    REQUIRE(seconds_to_timestr(3600)  == "1 hour 0 minutes 0 seconds");
    REQUIRE(seconds_to_timestr(3661)  == "1 hour 1 minute 1 second");
    REQUIRE(seconds_to_timestr(7322)  == "2 hours 2 minutes 2 seconds");
}

TEST_CASE("seconds_to_timestr: days, hours, minutes, seconds", "[core][string_utils]") {
    const unsigned int one_day = 24u * 60u * 60u;
    REQUIRE(seconds_to_timestr(one_day) == "1 day 0 hours 0 minutes 0 seconds");
    REQUIRE(seconds_to_timestr(one_day + 3661) == "1 day 1 hour 1 minute 1 second");
    REQUIRE(seconds_to_timestr(2 * one_day + 7322) == "2 days 2 hours 2 minutes 2 seconds");
}

// ===========================================================================
// clockstr_to_seconds
// ===========================================================================

TEST_CASE("clockstr_to_seconds: HH:MM:SS", "[core][string_utils]") {
    REQUIRE(clockstr_to_seconds("01:02:03") == 1u * 3600u + 2u * 60u + 3u);
    REQUIRE(clockstr_to_seconds("00:00:00") == 0u);
    REQUIRE(clockstr_to_seconds("10:30:45") == 10u * 3600u + 30u * 60u + 45u);
}

TEST_CASE("clockstr_to_seconds: MM:SS", "[core][string_utils]") {
    REQUIRE(clockstr_to_seconds("05:30") == 5u * 60u + 30u);
    REQUIRE(clockstr_to_seconds("00:00") == 0u);
}

TEST_CASE("clockstr_to_seconds: single segment (seconds only)", "[core][string_utils]") {
    REQUIRE(clockstr_to_seconds("42") == 42u);
}

TEST_CASE("clockstr_to_seconds: empty string returns nullopt", "[core][string_utils]") {
    REQUIRE_FALSE(clockstr_to_seconds("").has_value());
}

TEST_CASE("clockstr_to_seconds: invalid chars return nullopt", "[core][string_utils]") {
    REQUIRE_FALSE(clockstr_to_seconds("01:ab:03").has_value());
    REQUIRE_FALSE(clockstr_to_seconds("xx:yy").has_value());
}

// ===========================================================================
// escape_fs_chars
// ===========================================================================

TEST_CASE("escape_fs_chars: no special chars pass through", "[core][string_utils]") {
    REQUIRE(escape_fs_chars("hello") == "hello");
    REQUIRE(escape_fs_chars("abc123") == "abc123");
}

TEST_CASE("escape_fs_chars: slash encoded", "[core][string_utils]") {
    REQUIRE(escape_fs_chars("/") == "%2F");
}

TEST_CASE("escape_fs_chars: backslash encoded", "[core][string_utils]") {
    REQUIRE(escape_fs_chars("\\") == "%5C");
}

TEST_CASE("escape_fs_chars: colon encoded", "[core][string_utils]") {
    REQUIRE(escape_fs_chars(":") == "%3A");
}

TEST_CASE("escape_fs_chars: percent encoded", "[core][string_utils]") {
    REQUIRE(escape_fs_chars("%") == "%25");
}

TEST_CASE("escape_fs_chars: NUL byte encoded", "[core][string_utils]") {
    const std::string_view with_nul{"a\0b", 3};
    REQUIRE(escape_fs_chars(with_nul) == "a%00b");
}

TEST_CASE("escape_fs_chars: mixed path", "[core][string_utils]") {
    REQUIRE(escape_fs_chars("C:\\Users\\foo") == "C%3A%5CUsers%5Cfoo");
}

TEST_CASE("escape_fs_chars: empty string", "[core][string_utils]") {
    REQUIRE(escape_fs_chars("") == "");
}

// ===========================================================================
// escape_chars / unescape_chars
// ===========================================================================

TEST_CASE("escape_chars: printable chars pass through", "[core][string_utils]") {
    REQUIRE(escape_chars("hello world") == "hello world");
}

TEST_CASE("escape_chars: backslash doubled", "[core][string_utils]") {
    REQUIRE(escape_chars("\\") == "\\\\");
}

TEST_CASE("escape_chars: double-quote escaped", "[core][string_utils]") {
    REQUIRE(escape_chars("\"") == "\\\"");
}

TEST_CASE("escape_chars: control characters", "[core][string_utils]") {
    REQUIRE(escape_chars("\a") == "\\a");
    REQUIRE(escape_chars("\b") == "\\b");
    REQUIRE(escape_chars("\t") == "\\t");
    REQUIRE(escape_chars("\n") == "\\n");
    REQUIRE(escape_chars("\v") == "\\v");
    REQUIRE(escape_chars("\f") == "\\f");
    REQUIRE(escape_chars("\r") == "\\r");
}

TEST_CASE("escape_chars: non-printable byte → octal", "[core][string_utils]") {
    // byte 0x01 → \001
    const std::string_view s{"\x01", 1};
    REQUIRE(escape_chars(s) == "\\001");
    // byte 0xFF → \377
    const std::string_view s2{"\xff", 1};
    REQUIRE(escape_chars(s2) == "\\377");
}

TEST_CASE("escape_chars: empty string", "[core][string_utils]") {
    REQUIRE(escape_chars("") == "");
}

TEST_CASE("unescape_chars: printable chars pass through", "[core][string_utils]") {
    REQUIRE(unescape_chars("hello") == "hello");
}

TEST_CASE("unescape_chars: backslash sequences", "[core][string_utils]") {
    REQUIRE(unescape_chars("\\\\") == "\\");
    REQUIRE(unescape_chars("\\\"") == "\"");
    REQUIRE(unescape_chars("\\a")  == "\a");
    REQUIRE(unescape_chars("\\b")  == "\b");
    REQUIRE(unescape_chars("\\t")  == "\t");
    REQUIRE(unescape_chars("\\n")  == "\n");
    REQUIRE(unescape_chars("\\v")  == "\v");
    REQUIRE(unescape_chars("\\f")  == "\f");
    REQUIRE(unescape_chars("\\r")  == "\r");
}

TEST_CASE("unescape_chars: octal escape", "[core][string_utils]") {
    // \001 → byte 0x01
    REQUIRE(unescape_chars("\\001") == std::string(1, '\x01'));
    // \377 → byte 0xFF
    REQUIRE(unescape_chars("\\377") == std::string(1, '\xff'));
}

TEST_CASE("unescape_chars: empty string", "[core][string_utils]") {
    REQUIRE(unescape_chars("") == "");
}

TEST_CASE("escape_chars / unescape_chars round-trip", "[core][string_utils]") {
    const std::string original = "hello\nworld\t\"test\"\\\x01\xff";
    REQUIRE(unescape_chars(escape_chars(original)) == original);
}

// ===========================================================================
// bytes_to_hex_str / hex_str_to_bytes
// ===========================================================================

TEST_CASE("bytes_to_hex_str: basic", "[core][string_utils]") {
    REQUIRE(bytes_to_hex_str("\xDE\xAD\xBE\xEF") == "DE AD BE EF ");
}

TEST_CASE("bytes_to_hex_str: single byte", "[core][string_utils]") {
    REQUIRE(bytes_to_hex_str("\x0A") == "0A ");
}

TEST_CASE("bytes_to_hex_str: empty", "[core][string_utils]") {
    REQUIRE(bytes_to_hex_str("") == "");
}

TEST_CASE("bytes_to_hex_str: all zeros", "[core][string_utils]") {
    const std::string_view zeros{"\x00\x00\x00", 3};
    REQUIRE(bytes_to_hex_str(zeros) == "00 00 00 ");
}

TEST_CASE("hex_str_to_bytes: basic round-trip", "[core][string_utils]") {
    const std::string_view data{"\xDE\xAD\xBE\xEF", 4};
    const std::string hex = bytes_to_hex_str(data);
    const auto decoded = hex_str_to_bytes(hex, 4);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == std::string(data));
}

TEST_CASE("hex_str_to_bytes: invalid hex returns nullopt", "[core][string_utils]") {
    REQUIRE_FALSE(hex_str_to_bytes("ZZ ", 1).has_value());
}

TEST_CASE("hex_str_to_bytes: source too short returns nullopt", "[core][string_utils]") {
    REQUIRE_FALSE(hex_str_to_bytes("DE", 2).has_value()); // need 6 chars for 2 bytes
}

// ===========================================================================
// timestr_to_time
// ===========================================================================

TEST_CASE("timestr_to_time: empty string returns 0", "[core][string_utils]") {
    const auto t = timestr_to_time("");
    REQUIRE(t.has_value());
    REQUIRE(*t == std::time_t{0});
}

TEST_CASE("timestr_to_time: date only yyyy/mm/dd", "[core][string_utils]") {
    const auto t = timestr_to_time("2000/01/01");
    REQUIRE(t.has_value());
    // Verify it parses to a reasonable time_t (year 2000)
    std::tm* tm_out = std::localtime(&*t);
    REQUIRE(tm_out != nullptr);
    REQUIRE(tm_out->tm_year == 100); // 2000 - 1900
    REQUIRE(tm_out->tm_mon  == 0);   // January
    REQUIRE(tm_out->tm_mday == 1);
}

TEST_CASE("timestr_to_time: date with dash separator", "[core][string_utils]") {
    const auto t = timestr_to_time("2020-06-15");
    REQUIRE(t.has_value());
    std::tm* tm_out = std::localtime(&*t);
    REQUIRE(tm_out != nullptr);
    REQUIRE(tm_out->tm_year == 120); // 2020 - 1900
    REQUIRE(tm_out->tm_mon  == 5);   // June (0-based)
    REQUIRE(tm_out->tm_mday == 15);
}

TEST_CASE("timestr_to_time: date with dot separator", "[core][string_utils]") {
    const auto t = timestr_to_time("2021.03.25");
    REQUIRE(t.has_value());
    std::tm* tm_out = std::localtime(&*t);
    REQUIRE(tm_out != nullptr);
    REQUIRE(tm_out->tm_year == 121);
    REQUIRE(tm_out->tm_mon  == 2);
    REQUIRE(tm_out->tm_mday == 25);
}

TEST_CASE("timestr_to_time: datetime yyyy/mm/dd hh:mm:ss", "[core][string_utils]") {
    const auto t = timestr_to_time("2000/01/01 12:30:45");
    REQUIRE(t.has_value());
    std::tm* tm_out = std::localtime(&*t);
    REQUIRE(tm_out != nullptr);
    REQUIRE(tm_out->tm_year == 100);
    REQUIRE(tm_out->tm_mon  == 0);
    REQUIRE(tm_out->tm_mday == 1);
    REQUIRE(tm_out->tm_hour == 12);
    REQUIRE(tm_out->tm_min  == 30);
    REQUIRE(tm_out->tm_sec  == 45);
}

// ===========================================================================
// str_skip_space / str_skip_word
// ===========================================================================

TEST_CASE("str_skip_space: removes leading spaces", "[core][string_utils]") {
    REQUIRE(str_skip_space("   hello") == "hello");
}

TEST_CASE("str_skip_space: removes leading tabs", "[core][string_utils]") {
    REQUIRE(str_skip_space("\t\thello") == "hello");
}

TEST_CASE("str_skip_space: mixed leading whitespace", "[core][string_utils]") {
    REQUIRE(str_skip_space(" \t hello") == "hello");
}

TEST_CASE("str_skip_space: no leading whitespace unchanged", "[core][string_utils]") {
    REQUIRE(str_skip_space("hello world") == "hello world");
}

TEST_CASE("str_skip_space: all whitespace → empty", "[core][string_utils]") {
    REQUIRE(str_skip_space("   ").empty());
}

TEST_CASE("str_skip_space: empty string → empty", "[core][string_utils]") {
    REQUIRE(str_skip_space("").empty());
}

TEST_CASE("str_skip_word: removes leading non-whitespace", "[core][string_utils]") {
    REQUIRE(str_skip_word("hello world") == " world");
}

TEST_CASE("str_skip_word: stops at tab", "[core][string_utils]") {
    REQUIRE(str_skip_word("hello\tworld") == "\tworld");
}

TEST_CASE("str_skip_word: all non-whitespace → empty", "[core][string_utils]") {
    REQUIRE(str_skip_word("hello").empty());
}

TEST_CASE("str_skip_word: empty string → empty", "[core][string_utils]") {
    REQUIRE(str_skip_word("").empty());
}

TEST_CASE("str_skip_space + str_skip_word: tokenise first word", "[core][string_utils]") {
    std::string_view sv = "  hello world";
    sv = str_skip_space(sv);   // "hello world"
    const auto word_end = sv.find_first_of(" \t");
    const auto word = sv.substr(0, word_end);
    REQUIRE(word == "hello");
    sv = str_skip_word(sv);    // " world"
    sv = str_skip_space(sv);   // "world"
    REQUIRE(sv == "world");
}
