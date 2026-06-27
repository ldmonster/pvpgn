// SPDX-License-Identifier: GPL-2.0-or-later

#include "protocol/file/bnftp_fsm.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "core/error.hpp"
#include "protocol/common/writer.hpp"
#include "protocol/file/codec.hpp"

// GCC 14+ (alpine:latest) raises a false-positive `-Wnull-dereference`
// on the trivial `write_uXXle()` helpers below because their first
// parameter has no `nonnull` annotation. All call sites pass pointers
// derived from a `std::vector<std::byte>` after a size check, so the
// pointers are guaranteed non-null at runtime. Locally disable the
// warning instead of sprinkling `assert()`s through hot serialisation
// loops.
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

namespace pvpgn::protocol::file {

namespace {

// Maximum filename length accepted from the client. Mirrors the legacy
// MAX_FILENAME_STR cap (field_sizes.h: 2048, including the terminating NUL),
// so v3 serves the same range of names the original would (names between 129
// and 2047 chars were previously rejected with a size-0 reply — a divergence
// from the oracle for any on-disk file whose name exceeds 128 chars).
constexpr std::size_t kMaxFilenameLen = 2047;

// Read-buffer chunk size when streaming file data.
constexpr std::size_t kChunkSize = 4096;

// Minimum CLIENT_FILE_REQ packet size:
//   4 (header) + 4*5 (arch/client/ad/ext/offset) + 8 (timestamp) + 1 (NUL) = 33
constexpr std::size_t kMinReqSize = 4 + 4 * 5 + 8 + 1;

/// Return the file modification time as a 64-bit Windows FILETIME
/// (100-nanosecond intervals since 1601-01-01 UTC).
std::uint64_t mtime_to_filetime(std::time_t t) noexcept {
    // Windows FILETIME epoch offset: 11644473600 seconds before Unix epoch.
    constexpr std::uint64_t kEpochDelta = 11644473600ULL;
    constexpr std::uint64_t kTicksPerSec = 10000000ULL;
    return (static_cast<std::uint64_t>(t) + kEpochDelta) * kTicksPerSec;
}

/// Sanitise a client-supplied filename: reject path separators and
/// empty names. Returns false if the name is unsafe.
bool is_safe_filename(std::string_view name) noexcept {
    if (name.empty() || name.size() > kMaxFilenameLen) return false;
    for (char c : name) {
        if (c == '/' || c == '\\' || c == '\0') return false;
    }
    return true;
}

/// Read 4 LE bytes from buf at offset, advance offset.
std::uint32_t read_u32le(const std::byte* p) noexcept {
    return static_cast<std::uint32_t>(p[0])
         | (static_cast<std::uint32_t>(p[1]) << 8)
         | (static_cast<std::uint32_t>(p[2]) << 16)
         | (static_cast<std::uint32_t>(p[3]) << 24);
}

/// Read 8 LE bytes from buf at offset.
std::uint64_t read_u64le(const std::byte* p) noexcept {
    std::uint64_t lo = read_u32le(p);
    std::uint64_t hi = read_u32le(p + 4);
    return lo | (hi << 32);
}

/// Write 2 LE bytes.
void write_u16le(std::byte* p, std::uint16_t v) noexcept {
    p[0] = static_cast<std::byte>(v & 0xFF);
    p[1] = static_cast<std::byte>((v >> 8) & 0xFF);
}

/// Write 4 LE bytes.
void write_u32le(std::byte* p, std::uint32_t v) noexcept {
    p[0] = static_cast<std::byte>(v & 0xFF);
    p[1] = static_cast<std::byte>((v >> 8) & 0xFF);
    p[2] = static_cast<std::byte>((v >> 16) & 0xFF);
    p[3] = static_cast<std::byte>((v >> 24) & 0xFF);
}

/// Write 8 LE bytes.
void write_u64le(std::byte* p, std::uint64_t v) noexcept {
    write_u32le(p,     static_cast<std::uint32_t>(v & 0xFFFFFFFFULL));
    write_u32le(p + 4, static_cast<std::uint32_t>(v >> 32));
}

}  // namespace

// ---------------------------------------------------------------------------
// BnftpFsm::on_bytes
// ---------------------------------------------------------------------------

core::Status<> BnftpFsm::on_bytes(std::span<const std::byte> bytes) {
    if (state_ == State::Done) {
        // Ignore trailing bytes after transfer is complete.
        return core::ok();
    }
    if (!ctx_) {
        return core::fail(core::make_error(core::StatusCode::Internal,
                                           "bnftp fsm: no session context"));
    }

    // Accumulate into internal buffer.
    buf_.insert(buf_.end(), bytes.begin(), bytes.end());

    if (state_ == State::AwaitingRequest) {
        return try_dispatch();
    }

    // State::PendingRaw — we have acked a CLIENT_FILE_REQ2 with 0xdeadbeef
    // and the oracle keeps the connection open in conn_state_pending_raw to
    // await the raw CLIENT_FILE_REQ3 record. We mirror "keep open": accumulate
    // any trailing bytes without closing. (Serving the W3 two-step download is
    // not yet implemented; the connection simply idles, matching the oracle's
    // behaviour for a client that never completes REQ3.)
    // State::Serving — we don't expect more client data after the request.
    return core::ok();
}

// ---------------------------------------------------------------------------
// BnftpFsm::on_close
// ---------------------------------------------------------------------------

void BnftpFsm::on_close() {
    state_ = State::Done;
    buf_.clear();
}

// ---------------------------------------------------------------------------
// BnftpFsm::try_dispatch
// ---------------------------------------------------------------------------

core::Status<> BnftpFsm::try_dispatch() {
    // Need at least the 4-byte header to know the total packet size.
    if (buf_.size() < 4) return core::ok();

    // Parse the 4-byte header: u16 size, u16 type (both LE).
    const auto* p = buf_.data();
    std::uint16_t pkt_size = static_cast<std::uint16_t>(p[0])
                           | (static_cast<std::uint16_t>(p[1]) << 8);
    std::uint16_t pkt_type = static_cast<std::uint16_t>(p[2])
                           | (static_cast<std::uint16_t>(p[3]) << 8);

    // Validate declared size.
    if (pkt_size < 4) {
        state_ = State::Done;
        ctx_->close();
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "bnftp fsm: packet size < 4"));
    }

    // Wait for the full packet.
    if (buf_.size() < pkt_size) return core::ok();

    if (pkt_type == kClientFileReq2) {
        // CLIENT_FILE_REQ2 (0x0200): the War3 two-step download start.
        // The oracle replies with a raw 4-byte SERVER_FILE_UNKNOWN1
        // (0xdeadbeef, LE) and transitions to conn_state_pending_raw,
        // keeping the connection open to await CLIENT_FILE_REQ3
        // (src/bnetd/handle_file.cpp). Mirror that exactly.
        buf_.erase(buf_.begin(), buf_.begin() + pkt_size);

        std::array<std::byte, 4> reply{};
        write_u32le(reply.data(), kServerFileUnknown1);
        auto st = ctx_->send_bytes(std::span<const std::byte>{reply.data(), reply.size()});
        if (!st) {
            state_ = State::Done;
            ctx_->close();
            return st;
        }
        state_ = State::PendingRaw;
        return core::ok();
    }

    if (pkt_type != kClientFileReq) {
        // Unknown request type. Original parity: handle_file_packet's
        // conn_state_connected default case logs an error but RETURNS 0,
        // leaving the connection in conn_state_connected — the next packet
        // is parsed normally, so a subsequent valid CLIENT_FILE_REQ is still
        // served (src/bnetd/handle_file.cpp). Mirror that: consume just this
        // packet, stay in AwaitingRequest, and re-dispatch any remaining
        // buffered bytes instead of tearing the connection down.
        buf_.erase(buf_.begin(), buf_.begin() + pkt_size);
        return try_dispatch();
    }

    // Minimum body: 4*5 + 8 + 1 (NUL-terminated filename) = 29 bytes after header.
    if (pkt_size < kMinReqSize) {
        state_ = State::Done;
        ctx_->close();
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "bnftp fsm: CLIENT_FILE_REQ too short"));
    }

    // Parse body (skip 4-byte header).
    const auto* body = p + 4;
    std::uint32_t arch_tag      = read_u32le(body);      (void)arch_tag;
    std::uint32_t client_tag    = read_u32le(body + 4);  (void)client_tag;
    std::uint32_t ad_id         = read_u32le(body + 8);
    std::uint32_t extension_tag = read_u32le(body + 12);
    std::uint32_t start_offset  = read_u32le(body + 16);
    std::uint64_t timestamp     = read_u64le(body + 20); (void)timestamp;

    // Filename is a NUL-terminated string starting at body+28.
    const char* fname_start = reinterpret_cast<const char*>(body + 28);
    std::size_t fname_max   = static_cast<std::size_t>(pkt_size) - 4 - 28;
    std::size_t fname_len   = ::strnlen(fname_start, fname_max);

    // Consume the packet from the buffer.
    buf_.erase(buf_.begin(), buf_.begin() + pkt_size);

    // Original parity: packet_get_str_const (src/common/packet.cpp) scans for a
    // NUL terminator within the declared packet size and returns NULL when none
    // is found; handle_file_packet then logs "missing or too long filename" and
    // returns -1 WITHOUT calling file_send, so the server sends NOTHING. Match
    // that: if strnlen consumed the whole field without finding a NUL, the
    // filename is unterminated — emit no reply and close gracefully.
    if (fname_len == fname_max) {
        state_ = State::Done;
        ctx_->close();
        return core::ok();
    }

    std::string filename(fname_start, fname_len);

    // Serve the file.
    state_ = State::Serving;
    auto st = handle_file_request(filename, ad_id, extension_tag, start_offset);
    state_ = State::Done;
    ctx_->close();
    return st;
}

// ---------------------------------------------------------------------------
// BnftpFsm::handle_file_request
// ---------------------------------------------------------------------------

core::Status<> BnftpFsm::handle_file_request(std::string_view filename,
                                              std::uint32_t    ad_id,
                                              std::uint32_t    extension_tag,
                                              std::uint32_t    start_offset) {
    if (!is_safe_filename(filename)) {
        // Original parity: an unsafe rawname (contains '/' or '\\') makes
        // file_get_info throw, so file_send returns -1 BEFORE pushing any
        // packet — the server sends NOTHING (src/bnetd/file.cpp). Match that:
        // emit no reply at all (the caller closes the connection afterwards).
        return core::ok();
    }

    // Build the full path: files_dir_ / filename.
    std::filesystem::path full_path =
        std::filesystem::path(files_dir_) / std::string(filename);

    // Stat the file to get size and mtime.
    std::error_code ec;
    auto file_size = std::filesystem::file_size(full_path, ec);
    if (ec) {
        // Original parity: a missing file makes file_get_info throw (stat
        // fails), so file_send returns -1 BEFORE pushing any packet — the
        // server sends NOTHING for a not-found file (src/bnetd/file.cpp).
        // Match that: emit no reply (the caller closes afterwards).
        return core::ok();
    }

    auto last_write = std::filesystem::last_write_time(full_path, ec);
    std::uint64_t filetime = 0;
    if (!ec) {
        // Convert file_time_type to time_t via system_clock.
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            last_write - std::filesystem::file_time_type::clock::now()
            + std::chrono::system_clock::now());
        std::time_t t = std::chrono::system_clock::to_time_t(sctp);
        filetime = mtime_to_filetime(t);
    }

    // The reply's `filelen` always carries the FULL file size, independent of
    // start_offset, matching the original pvpgn (file_send sets filelen from
    // stat() and only then fseeks to startoffset — src/bnetd/file.cpp). The
    // streamed payload below is file_size - start_offset bytes.
    std::uint32_t full_len = static_cast<std::uint32_t>(file_size);

    // Clamp start_offset.
    if (start_offset > file_size) start_offset = static_cast<std::uint32_t>(file_size);
    std::uint32_t send_len = static_cast<std::uint32_t>(file_size - start_offset);

    // Send the reply header (full file size, per the original wire format).
    auto hdr_st = send_reply_header(full_len, ad_id, extension_tag, filetime, filename);
    if (!hdr_st) return hdr_st;

    if (send_len == 0) return core::ok();

    // Stream the file data.
    return stream_file(full_path.string(), start_offset);
}

// ---------------------------------------------------------------------------
// BnftpFsm::send_reply_header
// ---------------------------------------------------------------------------

core::Status<> BnftpFsm::send_reply_header(std::uint32_t    file_len,
                                            std::uint32_t    ad_id,
                                            std::uint32_t    extension_tag,
                                            std::uint64_t    timestamp,
                                            std::string_view filename) {
    // SERVER_FILE_REPLY wire layout (all LE):
    //   u16 size          — total packet length
    //   u16 type          — 0x0000
    //   u32 filelen
    //   u32 adid
    //   u32 extensiontag
    //   u64 timestamp
    //   cstring filename  — NUL-terminated
    //
    // Fixed prefix = 4 + 4 + 4 + 4 + 8 = 24 bytes; plus filename + NUL.

    const std::size_t fname_len = filename.size();
    const std::size_t total     = 24 + fname_len + 1;  // +1 for NUL

    if (total > 0xFFFFu) {
        return core::fail(core::make_error(core::StatusCode::OutOfRange,
                                           "bnftp fsm: reply header too large"));
    }

    std::vector<std::byte> pkt(total);
    auto* p = pkt.data();

    write_u16le(p,      static_cast<std::uint16_t>(total));  // size
    write_u16le(p + 2,  0x0000u);                            // type = SERVER_FILE_REPLY
    write_u32le(p + 4,  file_len);
    write_u32le(p + 8,  ad_id);
    write_u32le(p + 12, extension_tag);
    write_u64le(p + 16, timestamp);
    std::memcpy(p + 24, filename.data(), fname_len);
    p[24 + fname_len] = std::byte{0};  // NUL terminator

    return ctx_->send_bytes(std::span<const std::byte>{pkt.data(), pkt.size()});
}

// ---------------------------------------------------------------------------
// BnftpFsm::stream_file
// ---------------------------------------------------------------------------

core::Status<> BnftpFsm::stream_file(const std::string& path,
                                     std::uint32_t      start_offset) {
    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        return core::fail(core::make_error(core::StatusCode::NotFound,
                                           "bnftp fsm: cannot open file"));
    }

    // Seek to start_offset.
    if (start_offset > 0) {
        if (std::fseek(fp, static_cast<long>(start_offset), SEEK_SET) != 0) {
            std::fclose(fp);
            return core::fail(core::make_error(core::StatusCode::Internal,
                                               "bnftp fsm: fseek failed"));
        }
    }

    std::vector<std::byte> chunk(kChunkSize);
    while (true) {
        std::size_t n = std::fread(chunk.data(), 1, kChunkSize, fp);
        if (n == 0) break;

        auto st = ctx_->send_bytes(std::span<const std::byte>{chunk.data(), n});
        if (!st) {
            std::fclose(fp);
            return st;
        }
    }

    std::fclose(fp);
    return core::ok();
}

}  // namespace pvpgn::protocol::file

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
