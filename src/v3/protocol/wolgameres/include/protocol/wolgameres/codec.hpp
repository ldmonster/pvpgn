// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec.hpp
/// Pure decoder for the Westwood Online "gameres" report.
///
/// Wire format (all multi-byte ints **big-endian**, matching the legacy
/// `bn_int_nget` / `bn_short_nget` macros):
///
///   u16 size            ─┐ total packet length, header included
///   u16 rngd_size       ─┘ length of the optional RNDG sub-block
///   [optional 4 bytes zero]    — present when the first u32 of the body is 0
///   repeated TLVs:
///     u32 tag           — FourCC, e.g. 'SER#' (0x53455223)
///     u16 data_type     — kByte/kBool/kTime/kInt/kString/kBigInt
///     u16 data_len      — payload length in bytes
///     bytes data        — `data_len` bytes
///
/// This implementation provides the framing + TLV walker. Tag semantics
/// live above this layer (in the gameres aggregate / application code),
/// so callers receive a `Report{rngd_size, entries}` and decide what to
/// do with each entry's `tag`.

#include <cstdint>
#include <string_view>
#include <vector>

#include "core/bytes.hpp"
#include "core/result.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::wolgameres {

enum class DataType : std::uint16_t {
    kByte   = 1,
    kBool   = 2,
    kTime   = 5,
    kInt    = 6,
    kString = 7,
    kBigInt = 20,
};

struct Header {
    std::uint16_t size      = 0;
    std::uint16_t rngd_size = 0;
    static constexpr std::size_t kSize = 4;
    bool operator==(const Header&) const = default;
};

struct Entry {
    std::uint32_t tag       = 0;
    DataType      type      = DataType::kByte;
    /// Raw payload bytes (data_len bytes copied from the wire).
    std::vector<std::byte> data;
    bool operator==(const Entry&) const = default;
};

struct Report {
    Header header;
    /// True when the body began with a 4-byte zero prefix (RNDG marker).
    bool   has_rndg_prefix = false;
    std::vector<Entry> entries;
    bool operator==(const Report&) const = default;
};

/// Convenience views over the raw `Entry::data` payload, BE-decoded.
core::Result<std::uint8_t>  read_byte (const Entry&);
core::Result<std::uint32_t> read_int  (const Entry&);
core::Result<std::uint64_t> read_bigint(const Entry&);
/// Returns a view into `e.data`; valid for the lifetime of the entry.
std::string_view            read_string(const Entry& e);

core::Result<Header> parse_header(core::ByteView buf);
core::Result<Report> decode(core::ByteView buf);

/// Test-friendly encoder. Serializes a `Report` to wire bytes (header
/// `size` is recomputed; `rngd_size` is taken verbatim from the input).
core::Status<> encode(Writer& w, const Report& r);

}  // namespace pvpgn::protocol::wolgameres
