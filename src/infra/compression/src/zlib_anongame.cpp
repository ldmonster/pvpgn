// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/compression/zlib_anongame.hpp"

#include <cstring>
#include <limits>

#include <zlib.h>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::compression {

namespace {

core::Error zerr(int rc, const char* ctx) {
    return core::make_error(
        core::StatusCode::Internal,
        std::string{"zlib "} + ctx + ": rc=" + std::to_string(rc));
}

}  // namespace

core::Result<std::vector<std::uint8_t>> anongame_compress(
    std::span<const std::uint8_t> raw) {
    if (raw.size() > std::numeric_limits<std::uint16_t>::max()) {
        return core::fail(core::make_error(
            core::StatusCode::OutOfRange,
            "anongame_compress: raw payload exceeds u16 length"));
    }

    z_stream z{};
    int rc = deflateInit(&z, 9);
    if (rc != Z_OK) return core::fail(zerr(rc, "deflateInit"));

    // Worst-case bound for deflate output.
    const uLong bound = deflateBound(&z, static_cast<uLong>(raw.size()));
    std::vector<std::uint8_t> deflated(bound);

    z.next_in   = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(raw.data()));
    z.avail_in  = static_cast<uInt>(raw.size());
    z.next_out  = reinterpret_cast<Bytef*>(deflated.data());
    z.avail_out = static_cast<uInt>(deflated.size());

    rc = deflate(&z, Z_FINISH);
    if (rc != Z_STREAM_END) {
        deflateEnd(&z);
        return core::fail(zerr(rc, "deflate"));
    }
    const std::size_t deflated_len = z.total_out;
    deflateEnd(&z);

    if (deflated_len > std::numeric_limits<std::uint16_t>::max()) {
        return core::fail(core::make_error(
            core::StatusCode::OutOfRange,
            "anongame_compress: deflated stream exceeds u16 length"));
    }

    std::vector<std::uint8_t> out;
    out.reserve(kAnonGameHeaderSize + deflated_len);
    const auto raw_len  = static_cast<std::uint16_t>(raw.size());
    const auto comp_len = static_cast<std::uint16_t>(deflated_len);
    out.push_back(static_cast<std::uint8_t>(raw_len & 0xFF));
    out.push_back(static_cast<std::uint8_t>((raw_len  >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(comp_len & 0xFF));
    out.push_back(static_cast<std::uint8_t>((comp_len >> 8) & 0xFF));
    out.insert(out.end(), deflated.begin(),
               deflated.begin() + static_cast<std::ptrdiff_t>(deflated_len));
    return out;
}

core::Result<std::vector<std::uint8_t>> anongame_decompress(
    std::span<const std::uint8_t> framed) {
    if (framed.size() < kAnonGameHeaderSize) {
        return core::fail(core::make_error(
            core::StatusCode::OutOfRange,
            "anongame_decompress: input shorter than 4-byte header"));
    }
    const std::uint16_t raw_len = static_cast<std::uint16_t>(framed[0]) |
                                  (static_cast<std::uint16_t>(framed[1]) << 8);
    const std::uint16_t comp_len = static_cast<std::uint16_t>(framed[2]) |
                                   (static_cast<std::uint16_t>(framed[3]) << 8);
    if (framed.size() - kAnonGameHeaderSize < comp_len) {
        return core::fail(core::make_error(
            core::StatusCode::OutOfRange,
            "anongame_decompress: framed buffer shorter than comp_len"));
    }

    std::vector<std::uint8_t> out(raw_len);

    z_stream z{};
    int rc = inflateInit(&z);
    if (rc != Z_OK) return core::fail(zerr(rc, "inflateInit"));

    z.next_in   = const_cast<Bytef*>(
        reinterpret_cast<const Bytef*>(framed.data() + kAnonGameHeaderSize));
    z.avail_in  = comp_len;
    // zlib requires a non-null output buffer even when avail_out == 0.
    static Bytef sink_byte = 0;
    z.next_out  = out.empty() ? &sink_byte
                              : reinterpret_cast<Bytef*>(out.data());
    z.avail_out = static_cast<uInt>(out.size());

    rc = inflate(&z, Z_FINISH);
    if (rc != Z_STREAM_END) {
        inflateEnd(&z);
        return core::fail(zerr(rc, "inflate"));
    }
    if (z.total_out != raw_len) {
        inflateEnd(&z);
        return core::fail(core::make_error(
            core::StatusCode::Internal,
            "anongame_decompress: inflated length does not match header"));
    }
    inflateEnd(&z);
    return out;
}

}  // namespace pvpgn::infra::compression
