// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wire_types.hpp
/// BNFTP wire types, mirrored from `src/common/file_protocol.h`.
///
/// All multi-byte fields are little-endian on the wire. These
/// structs describe the FIXED prefix only; the trailing
/// NUL-terminated filename is handled by the codec layer.

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace pvpgn::protocol::file::wire {

/// `t_file_header`: 4-byte common header.
struct FileHeader
{
    std::uint16_t size = 0;
    std::uint16_t type = 0;
    constexpr bool operator==(const FileHeader&) const = default;
};
static_assert(sizeof(FileHeader) == 4);
static_assert(std::is_trivially_copyable_v<FileHeader>);

// ---- Message type codes -----------------------------------------------

inline constexpr std::uint16_t kClientFileReq      = 0x0100;
inline constexpr std::uint16_t kClientFileReq2     = 0x0200;
inline constexpr std::uint16_t kClientFileReq3     = 0x0000;
inline constexpr std::uint16_t kServerFileReply    = 0x0000;
inline constexpr std::uint32_t kServerFileUnknown1 = 0xdeadbeef;

// ---- Message bodies (fixed prefix; trailing filename omitted) ---------

/// `CLIENT_FILE_REQ` (0x0100). Wire size = 36 bytes.
struct ClientFileReq
{
    FileHeader     h{};
    std::uint32_t  archtag      = 0;
    std::uint32_t  clienttag    = 0;
    std::uint32_t  adid         = 0;
    std::uint32_t  extensiontag = 0;
    std::uint32_t  startoffset  = 0;
    std::uint64_t  timestamp    = 0;
    constexpr bool operator==(const ClientFileReq&) const = default;
};

/// `CLIENT_FILE_REQ2` (0x0200). Wire size = 20 bytes.
struct ClientFileReq2
{
    FileHeader     h{};
    std::uint32_t  archtag   = 0;
    std::uint32_t  clienttag = 0;
    std::uint64_t  unknown1  = 0;
    constexpr bool operator==(const ClientFileReq2&) const = default;
};

/// `CLIENT_FILE_REQ3` (0x0000). Raw record with no `FileHeader`;
/// wire size = 52 bytes.
struct ClientFileReq3
{
    std::uint32_t unknown1  = 0;
    std::uint64_t timestamp = 0;
    std::uint64_t unknown2  = 0;
    std::uint64_t unknown3  = 0;
    std::uint64_t unknown4  = 0;
    std::uint64_t unknown5  = 0;
    std::uint64_t unknown6  = 0;
    constexpr bool operator==(const ClientFileReq3&) const = default;
};

/// `SERVER_FILE_REPLY` (0x0000). Wire size = 24 bytes.
struct ServerFileReply
{
    FileHeader    h{};
    std::uint32_t filelen      = 0;
    std::uint32_t adid         = 0;
    std::uint32_t extensiontag = 0;
    std::uint64_t timestamp    = 0;
    constexpr bool operator==(const ServerFileReply&) const = default;
};

/// `SERVER_FILE_UNKNOWN1` (0xdeadbeef). Wire size = 4 bytes.
struct ServerFileUnknown1
{
    std::uint32_t unknown = 0;
    constexpr bool operator==(const ServerFileUnknown1&) const = default;
};

static_assert(std::is_trivially_copyable_v<ClientFileReq>);
static_assert(std::is_trivially_copyable_v<ClientFileReq2>);
static_assert(std::is_trivially_copyable_v<ClientFileReq3>);
static_assert(std::is_trivially_copyable_v<ServerFileReply>);
static_assert(std::is_trivially_copyable_v<ServerFileUnknown1>);

// NOTE: We do not `static_assert(sizeof(...) == N)` for these
// structs because their natural alignment on common ABIs differs
// from the legacy `PACKED_ATTR` layout (e.g., `uint32_t` followed
// by `uint64_t` inserts 4 bytes of padding). The v3 codec writes
// each field explicitly via `Writer` instead of `memcpy`, so the
// canonical wire byte count is exposed via the `kWireBytes*`
// constants below.

/// Canonical wire byte counts (excluding trailing filename).
inline constexpr std::size_t kWireBytesClientFileReq   = 36;
inline constexpr std::size_t kWireBytesClientFileReq2  = 20;
inline constexpr std::size_t kWireBytesClientFileReq3  = 52;
inline constexpr std::size_t kWireBytesServerFileReply = 24;

}  // namespace pvpgn::protocol::file::wire
