// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec.hpp
/// Pure codec for the legacy BNFTP file-transfer protocol used to ship
/// `IX86ver1.mpq`, `tos.txt`, ad banners, icon files, etc.
///
/// Wire layout — small 4-byte header, all LE:
///   u16 size      ─┐  total packet length, header included
///   u16 type      ─┤  e.g. 0x0100 CLIENT_FILE_REQ
///
/// Two SIDs covered:
///   * CLIENT_FILE_REQ  (0x0100): arch/client/ad-id/extension/start
///                                offset/timestamp + cstring filename
///   * SERVER_FILE_REPLY (0x0000): filelen/ad-id/extension/timestamp +
///                                cstring filename
///
/// Following the reply the server streams raw file contents until
/// `filelen` bytes have been delivered. The streaming is the
/// `infra/net` layer's job; only the header packet is encoded here.

#include <cstdint>
#include <string>
#include <variant>

#include "core/bytes.hpp"
#include "core/result.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::file {

inline constexpr std::uint16_t kClientFileReq      = 0x0100;
inline constexpr std::uint16_t kClientFileReq2     = 0x0200;
inline constexpr std::uint16_t kServerFileReply    = 0x0000;
inline constexpr std::uint32_t kServerFileUnknown1 = 0xdeadbeef;

struct FileHeader {
    std::uint16_t size = 0;
    std::uint16_t type = 0;
    static constexpr std::size_t kSize = 4;
    bool operator==(const FileHeader&) const = default;
};

struct ClientFileReq {
    std::uint32_t arch_tag      = 0;
    std::uint32_t client_tag    = 0;
    std::uint32_t ad_id         = 0;
    std::uint32_t extension_tag = 0;
    std::uint32_t start_offset  = 0;
    std::uint64_t timestamp     = 0;
    std::string   filename;
    bool operator==(const ClientFileReq&) const = default;
};

struct ServerFileReply {
    std::uint32_t file_len      = 0;
    std::uint32_t ad_id         = 0;
    std::uint32_t extension_tag = 0;
    std::uint64_t timestamp     = 0;
    std::string   filename;
    bool operator==(const ServerFileReply&) const = default;
};

using Message = std::variant<ClientFileReq, ServerFileReply>;

core::Result<FileHeader> parse_header(core::ByteView buf);

/// Decode one fully-framed file packet. Returns the variant arm that
/// matches the `type` field.
core::Result<Message> decode(core::ByteView buf);

core::Status<> encode(Writer& w, const ClientFileReq& m);
core::Status<> encode(Writer& w, const ServerFileReply& m);

}  // namespace pvpgn::protocol::file
