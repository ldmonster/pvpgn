// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "protocol/file/bnftp_fsm.hpp"
#include "protocol/file/file_session_context.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::file;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

namespace {

/// Fake session context that records every byte sent and whether close() was called.
class FakeFileContext : public IFileSessionContext {
public:
    std::vector<std::byte> sent;
    bool                   closed = false;

    core::Status<> send_bytes(std::span<const std::byte> bytes) override {
        sent.insert(sent.end(), bytes.begin(), bytes.end());
        return core::ok();
    }

    void close() override { closed = true; }
};

// ---------------------------------------------------------------------------
// Wire-format helpers — build a CLIENT_FILE_REQ (0x0100) packet.
// Layout (all LE):
//   u16 size, u16 type=0x0100,
//   u32 arch_tag, u32 client_tag, u32 ad_id, u32 extension_tag,
//   u32 start_offset, u64 timestamp,
//   cstring filename (NUL-terminated)
// ---------------------------------------------------------------------------

void write_u16le(std::vector<std::byte>& v, std::uint16_t x) {
    v.push_back(static_cast<std::byte>(x & 0xFF));
    v.push_back(static_cast<std::byte>((x >> 8) & 0xFF));
}

void write_u32le(std::vector<std::byte>& v, std::uint32_t x) {
    v.push_back(static_cast<std::byte>(x & 0xFF));
    v.push_back(static_cast<std::byte>((x >> 8) & 0xFF));
    v.push_back(static_cast<std::byte>((x >> 16) & 0xFF));
    v.push_back(static_cast<std::byte>((x >> 24) & 0xFF));
}

void write_u64le(std::vector<std::byte>& v, std::uint64_t x) {
    write_u32le(v, static_cast<std::uint32_t>(x & 0xFFFFFFFFULL));
    write_u32le(v, static_cast<std::uint32_t>(x >> 32));
}

/// Build a complete CLIENT_FILE_REQ packet.
std::vector<std::byte> make_file_req(std::string_view filename,
                                     std::uint32_t    start_offset = 0,
                                     std::uint32_t    ad_id        = 0,
                                     std::uint32_t    ext_tag      = 0) {
    // Fixed body: arch(4) + client(4) + ad_id(4) + ext(4) + offset(4) + ts(8) = 28 bytes
    // Plus header (4) + filename + NUL
    const std::size_t total = 4 + 28 + filename.size() + 1;

    std::vector<std::byte> pkt;
    pkt.reserve(total);

    write_u16le(pkt, static_cast<std::uint16_t>(total));  // size
    write_u16le(pkt, 0x0100u);                             // type = CLIENT_FILE_REQ
    write_u32le(pkt, 0u);                                  // arch_tag
    write_u32le(pkt, 0u);                                  // client_tag
    write_u32le(pkt, ad_id);
    write_u32le(pkt, ext_tag);
    write_u32le(pkt, start_offset);
    write_u64le(pkt, 0ULL);                                // timestamp

    for (char c : filename) pkt.push_back(static_cast<std::byte>(c));
    pkt.push_back(std::byte{0});  // NUL terminator

    return pkt;
}

// ---------------------------------------------------------------------------
// Parse the SERVER_FILE_REPLY header from the beginning of a byte buffer.
// Layout (all LE):
//   u16 size, u16 type=0x0000,
//   u32 filelen, u32 adid, u32 extensiontag, u64 timestamp,
//   cstring filename
// ---------------------------------------------------------------------------

struct ParsedReply {
    std::uint16_t pkt_size;
    std::uint16_t pkt_type;
    std::uint32_t filelen;
    std::uint32_t adid;
    std::uint32_t extensiontag;
    std::uint64_t timestamp;
    std::string   filename;
    std::size_t   header_bytes;  // total bytes consumed by the header packet
};

std::uint16_t read_u16le(const std::byte* p) {
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}

std::uint32_t read_u32le(const std::byte* p) {
    return static_cast<std::uint32_t>(p[0])
         | (static_cast<std::uint32_t>(p[1]) << 8)
         | (static_cast<std::uint32_t>(p[2]) << 16)
         | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint64_t read_u64le(const std::byte* p) {
    std::uint64_t lo = read_u32le(p);
    std::uint64_t hi = read_u32le(p + 4);
    return lo | (hi << 32);
}

ParsedReply parse_reply_header(const std::vector<std::byte>& buf) {
    REQUIRE(buf.size() >= 24);
    const auto* p = buf.data();

    ParsedReply r{};
    r.pkt_size     = read_u16le(p);
    r.pkt_type     = read_u16le(p + 2);
    r.filelen      = read_u32le(p + 4);
    r.adid         = read_u32le(p + 8);
    r.extensiontag = read_u32le(p + 12);
    r.timestamp    = read_u64le(p + 16);

    // NUL-terminated filename starting at offset 24.
    const char* fname = reinterpret_cast<const char*>(p + 24);
    std::size_t max   = r.pkt_size > 24u ? static_cast<std::size_t>(r.pkt_size) - 24u : 0u;
    r.filename        = std::string(fname, ::strnlen(fname, max));
    r.header_bytes    = r.pkt_size;

    return r;
}

// ---------------------------------------------------------------------------
// RAII temp-file helper
// ---------------------------------------------------------------------------

struct TempFile {
    std::filesystem::path path;
    std::string           dir;

    TempFile(std::string_view name, std::string_view content) {
        auto tmp = std::filesystem::temp_directory_path() / "pvpgn_bnftp_test";
        std::filesystem::create_directories(tmp);
        dir  = tmp.string();
        path = tmp / std::string(name);

        std::ofstream f(path, std::ios::binary);
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

}  // namespace

// ===========================================================================
// Tests
// ===========================================================================

TEST_CASE("BnftpFsm: initial state is AwaitingRequest",
          "[protocol][file][bnftp]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/tmp"};
    REQUIRE(fsm.state() == BnftpFsm::State::AwaitingRequest);
}

// ---------------------------------------------------------------------------
// Buffering: partial packet must not trigger dispatch
// ---------------------------------------------------------------------------

TEST_CASE("BnftpFsm: partial packet is buffered without dispatch",
          "[protocol][file][bnftp]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/nonexistent_dir"};

    auto pkt = make_file_req("test.mpq");

    // Feed all but the last byte.
    std::span<const std::byte> partial{pkt.data(), pkt.size() - 1};
    auto st = fsm.on_bytes(partial);

    REQUIRE(st.has_value());
    REQUIRE(ctx->sent.empty());
    REQUIRE_FALSE(ctx->closed);
    REQUIRE(fsm.state() == BnftpFsm::State::AwaitingRequest);
}

TEST_CASE("BnftpFsm: only 3 bytes (no header yet) — no dispatch",
          "[protocol][file][bnftp]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/nonexistent_dir"};

    std::array<std::byte, 3> tiny{std::byte{0x25}, std::byte{0x00},
                                   std::byte{0x00}};
    auto st = fsm.on_bytes(std::span<const std::byte>{tiny.data(), tiny.size()});

    REQUIRE(st.has_value());
    REQUIRE(ctx->sent.empty());
    REQUIRE(fsm.state() == BnftpFsm::State::AwaitingRequest);
}

// ---------------------------------------------------------------------------
// File not found → zero-length reply header, then close
// ---------------------------------------------------------------------------

TEST_CASE("BnftpFsm: file not found sends zero-length reply and closes",
          "[protocol][file][bnftp]") {
    auto ctx = std::make_shared<FakeFileContext>();
    // Use a directory that definitely does not contain "missing.mpq".
    BnftpFsm fsm{ctx, "/nonexistent_dir_pvpgn_test"};

    auto pkt = make_file_req("missing.mpq", 0, 0xABCD, 0x1234);
    auto st  = fsm.on_bytes(std::span<const std::byte>{pkt.data(), pkt.size()});

    REQUIRE(st.has_value());
    REQUIRE_FALSE(ctx->sent.empty());

    // Parse the reply header.
    auto reply = parse_reply_header(ctx->sent);
    REQUIRE(reply.pkt_type == 0x0000u);   // SERVER_FILE_REPLY
    REQUIRE(reply.filelen  == 0u);         // zero-length → file not found
    REQUIRE(reply.adid     == 0xABCDu);
    REQUIRE(reply.extensiontag == 0x1234u);
    REQUIRE(reply.filename == "missing.mpq");

    // No file data after the header.
    REQUIRE(ctx->sent.size() == reply.header_bytes);

    // FSM must be Done and connection closed.
    REQUIRE(fsm.state() == BnftpFsm::State::Done);
    REQUIRE(ctx->closed);
}

// ---------------------------------------------------------------------------
// Unsafe filename (path traversal) → zero-length reply
// ---------------------------------------------------------------------------

TEST_CASE("BnftpFsm: path traversal filename sends zero-length reply",
          "[protocol][file][bnftp]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/nonexistent_dir_pvpgn_test"};

    auto pkt = make_file_req("../etc/passwd");
    auto st  = fsm.on_bytes(std::span<const std::byte>{pkt.data(), pkt.size()});

    REQUIRE(st.has_value());
    REQUIRE_FALSE(ctx->sent.empty());

    auto reply = parse_reply_header(ctx->sent);
    REQUIRE(reply.pkt_type == 0x0000u);
    REQUIRE(reply.filelen  == 0u);

    REQUIRE(fsm.state() == BnftpFsm::State::Done);
    REQUIRE(ctx->closed);
}

TEST_CASE("BnftpFsm: backslash path traversal sends zero-length reply",
          "[protocol][file][bnftp]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/nonexistent_dir_pvpgn_test"};

    auto pkt = make_file_req("..\\windows\\system32\\cmd.exe");
    auto st  = fsm.on_bytes(std::span<const std::byte>{pkt.data(), pkt.size()});

    REQUIRE(st.has_value());
    REQUIRE_FALSE(ctx->sent.empty());

    auto reply = parse_reply_header(ctx->sent);
    REQUIRE(reply.filelen == 0u);

    REQUIRE(fsm.state() == BnftpFsm::State::Done);
    REQUIRE(ctx->closed);
}

// ---------------------------------------------------------------------------
// Valid file request → reply header + file data
// ---------------------------------------------------------------------------

TEST_CASE("BnftpFsm: valid file request sends header + full file data",
          "[protocol][file][bnftp]") {
    const std::string content = "Hello, BNFTP world!";
    TempFile          tf{"test_bnftp.mpq", content};

    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, tf.dir};

    auto pkt = make_file_req("test_bnftp.mpq", 0, 0x11, 0x22);
    auto st  = fsm.on_bytes(std::span<const std::byte>{pkt.data(), pkt.size()});

    REQUIRE(st.has_value());
    REQUIRE_FALSE(ctx->sent.empty());

    auto reply = parse_reply_header(ctx->sent);
    REQUIRE(reply.pkt_type == 0x0000u);
    REQUIRE(reply.filelen  == static_cast<std::uint32_t>(content.size()));
    REQUIRE(reply.adid     == 0x11u);
    REQUIRE(reply.extensiontag == 0x22u);
    REQUIRE(reply.filename == "test_bnftp.mpq");

    // File data follows the header.
    REQUIRE(ctx->sent.size() == reply.header_bytes + content.size());

    const auto* data_start = ctx->sent.data() + reply.header_bytes;
    std::string received(reinterpret_cast<const char*>(data_start), content.size());
    REQUIRE(received == content);

    REQUIRE(fsm.state() == BnftpFsm::State::Done);
    REQUIRE(ctx->closed);
}

TEST_CASE("BnftpFsm: valid file request with start_offset skips bytes",
          "[protocol][file][bnftp]") {
    const std::string content = "ABCDEFGHIJ";  // 10 bytes
    TempFile          tf{"test_bnftp_offset.mpq", content};

    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, tf.dir};

    // Request starting at offset 4 → should receive "EFGHIJ" (6 bytes).
    auto pkt = make_file_req("test_bnftp_offset.mpq", 4);
    auto st  = fsm.on_bytes(std::span<const std::byte>{pkt.data(), pkt.size()});

    REQUIRE(st.has_value());

    auto reply = parse_reply_header(ctx->sent);
    // filelen in the reply always carries the FULL file size (10), matching the
    // original pvpgn; the streamed payload is file_size - start_offset (6 bytes).
    REQUIRE(reply.filelen == static_cast<std::uint32_t>(content.size()));

    REQUIRE(ctx->sent.size() == reply.header_bytes + 6u);

    const auto* data_start = ctx->sent.data() + reply.header_bytes;
    std::string received(reinterpret_cast<const char*>(data_start), 6u);
    REQUIRE(received == "EFGHIJ");

    REQUIRE(fsm.state() == BnftpFsm::State::Done);
    REQUIRE(ctx->closed);
}

TEST_CASE("BnftpFsm: start_offset >= file_size sends zero data bytes",
          "[protocol][file][bnftp]") {
    const std::string content = "short";  // 5 bytes
    TempFile          tf{"test_bnftp_past_eof.mpq", content};

    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, tf.dir};

    // Offset beyond EOF.
    auto pkt = make_file_req("test_bnftp_past_eof.mpq", 100);
    auto st  = fsm.on_bytes(std::span<const std::byte>{pkt.data(), pkt.size()});

    REQUIRE(st.has_value());

    auto reply = parse_reply_header(ctx->sent);
    // filelen carries the FULL file size even past EOF (the original "keeps the
    // real filesize" — src/bnetd/file.cpp); no data bytes are streamed.
    REQUIRE(reply.filelen == static_cast<std::uint32_t>(content.size()));

    // Only the header packet, no file data.
    REQUIRE(ctx->sent.size() == reply.header_bytes);

    REQUIRE(fsm.state() == BnftpFsm::State::Done);
    REQUIRE(ctx->closed);
}

// ---------------------------------------------------------------------------
// Packet delivered in two chunks (split across two on_bytes calls)
// ---------------------------------------------------------------------------

TEST_CASE("BnftpFsm: packet split across two on_bytes calls is reassembled",
          "[protocol][file][bnftp]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/nonexistent_dir_pvpgn_test"};

    auto pkt = make_file_req("split_test.mpq");

    // Feed first half.
    std::size_t half = pkt.size() / 2;
    auto st1 = fsm.on_bytes(std::span<const std::byte>{pkt.data(), half});
    REQUIRE(st1.has_value());
    REQUIRE(ctx->sent.empty());
    REQUIRE(fsm.state() == BnftpFsm::State::AwaitingRequest);

    // Feed second half.
    auto st2 = fsm.on_bytes(
        std::span<const std::byte>{pkt.data() + half, pkt.size() - half});
    REQUIRE(st2.has_value());

    // File not found → zero-length reply.
    REQUIRE_FALSE(ctx->sent.empty());
    auto reply = parse_reply_header(ctx->sent);
    REQUIRE(reply.filelen == 0u);
    REQUIRE(reply.filename == "split_test.mpq");

    REQUIRE(fsm.state() == BnftpFsm::State::Done);
    REQUIRE(ctx->closed);
}

// ---------------------------------------------------------------------------
// Bytes after Done state are silently ignored
// ---------------------------------------------------------------------------

TEST_CASE("BnftpFsm: bytes received after Done are ignored",
          "[protocol][file][bnftp]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/nonexistent_dir_pvpgn_test"};

    // Complete a transfer.
    auto pkt = make_file_req("ignored.mpq");
    REQUIRE(fsm.on_bytes(std::span<const std::byte>{pkt.data(), pkt.size()}).has_value());
    REQUIRE(fsm.state() == BnftpFsm::State::Done);

    std::size_t sent_before = ctx->sent.size();

    // Feed more bytes — must be silently dropped.
    auto pkt2 = make_file_req("second.mpq");
    auto st   = fsm.on_bytes(std::span<const std::byte>{pkt2.data(), pkt2.size()});
    REQUIRE(st.has_value());
    REQUIRE(ctx->sent.size() == sent_before);  // no new bytes sent
}

// ---------------------------------------------------------------------------
// on_close transitions to Done
// ---------------------------------------------------------------------------

TEST_CASE("BnftpFsm: on_close transitions to Done",
          "[protocol][file][bnftp]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/tmp"};

    REQUIRE(fsm.state() == BnftpFsm::State::AwaitingRequest);
    fsm.on_close();
    REQUIRE(fsm.state() == BnftpFsm::State::Done);
}

// ---------------------------------------------------------------------------
// Malformed packet: declared size < 4 → error + close
// ---------------------------------------------------------------------------

TEST_CASE("BnftpFsm: packet with size < 4 returns error and closes",
          "[protocol][file][bnftp]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/tmp"};

    // Craft a 4-byte packet with size field = 2 (invalid).
    std::array<std::byte, 4> bad{
        std::byte{0x02}, std::byte{0x00},  // size = 2
        std::byte{0x00}, std::byte{0x01},  // type = CLIENT_FILE_REQ
    };
    auto st = fsm.on_bytes(std::span<const std::byte>{bad.data(), bad.size()});

    REQUIRE_FALSE(st.has_value());
    REQUIRE(ctx->closed);
    REQUIRE(fsm.state() == BnftpFsm::State::Done);
}
