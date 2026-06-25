// SPDX-License-Identifier: GPL-2.0-or-later
//
// GCC 13's `-Warray-bounds` produces a false positive when copying a
// `std::vector<std::byte>` that contains exactly one element (the
// inlined `__builtin_memmove` is flagged even though it stays within
// the allocated block). Suppress the diagnostic for the duration of
// this translation unit.
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif
#include <array>
#include <cstring>

#include <catch2/catch_test_macros.hpp>

#include "protocol/wolgameres/codec.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::wolgameres;

namespace {

std::vector<std::byte> bytes_of(std::string_view s) {
    std::vector<std::byte> out(s.size() + 1, std::byte{0});
    std::memcpy(out.data(), s.data(), s.size());
    return out;
}

std::vector<std::byte> u32_be_bytes(std::uint32_t v) {
    std::vector<std::byte> out(4);
    out[0] = std::byte{static_cast<std::uint8_t>((v >> 24) & 0xFF)};
    out[1] = std::byte{static_cast<std::uint8_t>((v >> 16) & 0xFF)};
    out[2] = std::byte{static_cast<std::uint8_t>((v >>  8) & 0xFF)};
    out[3] = std::byte{static_cast<std::uint8_t>(v        & 0xFF)};
    return out;
}

}  // namespace

TEST_CASE("wolgameres: round-trip empty report", "[protocol][wolgameres]") {
    Report in{};
    in.header.rngd_size = 0;
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto r = decode(w.view());
    REQUIRE(r.has_value());
    REQUIRE(r.value().header.size == Header::kSize);
    REQUIRE(r.value().entries.empty());
}

TEST_CASE("wolgameres: round-trip mixed TLVs", "[protocol][wolgameres]") {
    Report in{};
    in.header.rngd_size = 12;
    in.has_rndg_prefix  = true;

    // SER# (string)
    Entry sern;
    sern.tag  = 0x53455223u;  // 'SER#'
    sern.type = DataType::kString;
    sern.data = bytes_of("ABC-123");

    Entry idno;
    idno.tag  = 0x49444E4Fu;  // 'IDNO'
    idno.type = DataType::kInt;
    idno.data = u32_be_bytes(0xCAFEBABEu);

    Entry fini;
    fini.tag  = 0x46494E49u;  // 'FINI'
    fini.type = DataType::kByte;
    fini.data.push_back(std::byte{0x01});

    in.entries.emplace_back(sern);
    in.entries.emplace_back(idno);
    in.entries.emplace_back(fini);

    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());

    auto r = decode(w.view());
    REQUIRE(r.has_value());
    const auto& out = r.value();
    REQUIRE(out.has_rndg_prefix);
    REQUIRE(out.entries.size() == 3);
    REQUIRE(out.entries[0] == sern);
    REQUIRE(out.entries[1] == idno);
    REQUIRE(out.entries[2] == fini);

    REQUIRE(std::string{read_string(out.entries[0])} ==
            std::string{"ABC-123"});
    auto v = read_int(out.entries[1]);
    REQUIRE(v.has_value());
    REQUIRE(v.value() == 0xCAFEBABEu);
    auto b = read_byte(out.entries[2]);
    REQUIRE(b.has_value());
    REQUIRE(b.value() == 0x01);
}

namespace {

// Build an Entry whose value is exactly `n` raw bytes (0x01, 0x02, ...),
// independent of the NUL-appending `bytes_of` helper, so the value length
// can be a genuine odd (non-multiple-of-4) number.
Entry raw_entry(std::uint32_t tag, DataType type, std::size_t n) {
    Entry e;
    e.tag  = tag;
    e.type = type;
    e.data.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        e.data[i] = std::byte{static_cast<std::uint8_t>(i + 1)};
    }
    return e;
}

}  // namespace

// Regression for the 4-byte record-alignment bug (finding F3): an odd-length
// value must be followed on the wire by pad bytes that round its advance up to
// a 4-byte boundary. If decode does not skip that padding, the TLV walk
// desyncs and every subsequent record is misread.
TEST_CASE("wolgameres: odd-length record stays 4-byte aligned",
          "[protocol][wolgameres]") {
    for (std::size_t odd_len : {std::size_t{1}, std::size_t{3},
                                std::size_t{5}, std::size_t{7}}) {
        // First record has an odd-length value; a second record follows.
        Entry first  = raw_entry(0x4F444431u /* 'ODD1' */,
                                 DataType::kString, odd_len);
        Entry second = raw_entry(0x4E455854u /* 'NEXT' */,
                                 DataType::kInt, 4);

        Report in{};
        in.header.rngd_size = 0;
        in.entries.emplace_back(first);
        in.entries.emplace_back(second);

        protocol::Writer w;
        REQUIRE(encode(w, in).has_value());

        auto r = decode(w.view());
        REQUIRE(r.has_value());
        const auto& out = r.value();
        // Both records decode correctly => the walk re-aligned after the
        // odd-length value's padding.
        REQUIRE(out.entries.size() == 2);
        REQUIRE(out.entries[0] == first);
        REQUIRE(out.entries[1] == second);
    }
}

// Encode -> decode round-trip of an odd-length value must preserve it exactly.
TEST_CASE("wolgameres: odd-length value round-trips",
          "[protocol][wolgameres]") {
    Entry odd = raw_entry(0x4F444456u /* 'ODDV' */, DataType::kString, 5);

    Report in{};
    in.header.rngd_size = 0;
    in.entries.emplace_back(odd);

    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());

    auto r = decode(w.view());
    REQUIRE(r.has_value());
    REQUIRE(r.value().entries.size() == 1);
    REQUIRE(r.value().entries[0] == odd);
    REQUIRE(r.value().entries[0].data.size() == 5u);
}

TEST_CASE("wolgameres: rejects unknown data_type",
          "[protocol][wolgameres]") {
    // Header (size=14, rngd=0) + tag 'XXXX' + type 0x0099 + len 0
    std::array<std::byte, 14> raw{
        std::byte{0x00}, std::byte{0x0E},  // size
        std::byte{0x00}, std::byte{0x00},  // rngd_size
        std::byte{'X'},  std::byte{'X'}, std::byte{'X'}, std::byte{'X'},
        std::byte{0x00}, std::byte{0x99},  // bad data_type
        std::byte{0x00}, std::byte{0x00},  // datalen
        std::byte{0x00}, std::byte{0x00},
    };
    auto r = decode(core::ByteView{raw});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("wolgameres: short header rejected", "[protocol][wolgameres]") {
    std::array<std::byte, 4> raw{
        std::byte{0x00}, std::byte{0x02},  // size < kSize
        std::byte{0x00}, std::byte{0x00}};
    auto r = parse_header(core::ByteView{raw});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}
