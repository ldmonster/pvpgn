// SPDX-License-Identifier: GPL-2.0-or-later

/// @file bnftp_session_test.cpp
/// Unit tests for the BNFTP session wiring layer:
///   - FileSessionFactory (callable that creates BnftpTcpSession instances)
///   - BnftpFsm state transitions via a FakeFileContext
///   - TcpSessionEgress null-safety (no live socket needed)
///
/// These tests do NOT require a live Asio io_context. They use a
/// `FakeFileContext` (IFileSessionContext stub) to exercise the FSM
/// without network I/O, and verify the FileSessionFactory API.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "app/bnetd/file_session_factory.hpp"
#include "core/result.hpp"
#include "protocol/file/bnftp_fsm.hpp"
#include "protocol/file/file_session_context.hpp"

using namespace pvpgn;
using namespace pvpgn::app::bnetd;
using namespace pvpgn::protocol::file;

// ---------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------

namespace {

/// Fake IFileSessionContext — records send_bytes and close() calls.
class FakeFileContext final : public IFileSessionContext {
public:
    std::vector<std::byte> sent;
    int                    close_count  = 0;
    bool                   fail_on_send = false;

    core::Status<> send_bytes(std::span<const std::byte> bytes) override {
        if (fail_on_send) {
            return core::fail(core::Error{core::StatusCode::Internal,
                                          "injected send failure"});
        }
        sent.insert(sent.end(), bytes.begin(), bytes.end());
        return core::ok();
    }

    void close() override { ++close_count; }
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

std::vector<std::byte> make_file_req(std::string_view filename,
                                     std::uint32_t    start_offset = 0,
                                     std::uint32_t    ad_id        = 0,
                                     std::uint32_t    ext_tag      = 0) {
    const std::size_t total = 4 + 28 + filename.size() + 1;
    std::vector<std::byte> pkt;
    pkt.reserve(total);
    write_u16le(pkt, static_cast<std::uint16_t>(total));
    write_u16le(pkt, 0x0100u);
    write_u32le(pkt, 0u);           // arch_tag
    write_u32le(pkt, 0u);           // client_tag
    write_u32le(pkt, ad_id);
    write_u32le(pkt, ext_tag);
    write_u32le(pkt, start_offset);
    write_u64le(pkt, 0u);           // timestamp
    for (char c : filename) pkt.push_back(static_cast<std::byte>(c));
    pkt.push_back(static_cast<std::byte>(0));  // NUL terminator
    return pkt;
}

// Init-class octet that opens a real BNFTP connection on the wire
// (CLIENT_INITCONN_CLASS_FILE). The shared-port dispatch peeks this byte
// to route to BnftpFsm, then must consume it before replaying the rest.
constexpr std::byte kInitClassFile{0x02};

// Build a full on-the-wire BNFTP opening stream: the leading init-class
// byte (0x02) immediately followed by a CLIENT_FILE_REQ packet — exactly
// what a real client sends and what the dispatch buffers in its peek.
std::vector<std::byte> make_wire_open(std::string_view filename) {
    std::vector<std::byte> wire;
    wire.push_back(kInitClassFile);
    auto req = make_file_req(filename);
    wire.insert(wire.end(), req.begin(), req.end());
    return wire;
}

// Mirror of the shared-port dispatch's BNFTP branch (post-fix): peek the
// first byte, and when it is the file init-class octet, strip it before
// replaying the remainder into the FSM. Keeping this in lock-step with
// bnet_bnftp_dispatch.cpp lets us regression-test the byte handling that
// reproduces Finding 1 without standing up the full Asio/SessionManager
// dispatch factory.
std::span<const std::byte> dispatch_strip_init(
    std::span<const std::byte> wire) {
    if (!wire.empty() && wire.front() == kInitClassFile) {
        return wire.subspan(1);
    }
    return wire;
}

}  // namespace

// ===========================================================================
// TEST SUITE 1: BnftpFsm state transitions via FakeFileContext
// ===========================================================================

TEST_CASE("BnftpFsm: initial state is AwaitingRequest",
          "[bnftp_session][fsm_state]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/tmp"};

    CHECK(fsm.state() == BnftpFsm::State::AwaitingRequest);
}

TEST_CASE("BnftpFsm: on_close() transitions to Done",
          "[bnftp_session][fsm_state]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/tmp"};

    REQUIRE(fsm.state() == BnftpFsm::State::AwaitingRequest);
    fsm.on_close();
    CHECK(fsm.state() == BnftpFsm::State::Done);
}

TEST_CASE("BnftpFsm: on_bytes with truncated packet stays AwaitingRequest",
          "[bnftp_session][fsm_state]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/tmp"};

    // Only 2 bytes — not enough for even the header
    const std::vector<std::byte> partial = {std::byte{0x20}, std::byte{0x00}};
    (void)fsm.on_bytes(std::span<const std::byte>(partial));

    // Partial data: FSM buffers it, stays in AwaitingRequest
    CHECK(fsm.state() == BnftpFsm::State::AwaitingRequest);
    // No bytes sent yet
    CHECK(ctx->sent.empty());
}

TEST_CASE("BnftpFsm: on_bytes with file-not-found request transitions to Done",
          "[bnftp_session][fsm_state]") {
    auto ctx = std::make_shared<FakeFileContext>();
    // Use a directory that definitely has no "nonexistent_file.mpq"
    BnftpFsm fsm{ctx, "/tmp"};

    auto pkt = make_file_req("nonexistent_file_pvpgn_test_r126.mpq");
    (void)fsm.on_bytes(std::span<const std::byte>(pkt));

    // File not found → FSM transitions to Done (error path) and, matching the
    // original (file_send returns -1 before pushing any packet), sends nothing.
    CHECK(fsm.state() == BnftpFsm::State::Done);
    CHECK(ctx->sent.empty());
}

TEST_CASE("BnftpFsm: on_close() after Done is idempotent",
          "[bnftp_session][fsm_state]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/tmp"};

    fsm.on_close();
    REQUIRE(fsm.state() == BnftpFsm::State::Done);

    // Second on_close must not crash
    REQUIRE_NOTHROW(fsm.on_close());
    CHECK(fsm.state() == BnftpFsm::State::Done);
}

TEST_CASE("BnftpFsm: on_bytes with empty span is a no-op",
          "[bnftp_session][fsm_state]") {
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/tmp"};

    const std::vector<std::byte> empty;
    auto result = fsm.on_bytes(std::span<const std::byte>(empty));

    CHECK(result);  // ok() — empty input is not an error
    CHECK(fsm.state() == BnftpFsm::State::AwaitingRequest);
    CHECK(ctx->sent.empty());
    CHECK(ctx->close_count == 0);
}

TEST_CASE("BnftpFsm: send failure from context transitions to Done",
          "[bnftp_session][fsm_state]") {
    auto ctx = std::make_shared<FakeFileContext>();
    ctx->fail_on_send = true;
    BnftpFsm fsm{ctx, "/tmp"};

    // Feed a complete, valid-looking packet for a file that exists.
    // We need a real file — use /tmp itself as a directory, but request
    // a file that exists on any Linux system.
    // Actually, we just need the FSM to try to send something.
    // Use /proc/version which always exists on Linux.
    auto pkt = make_file_req("version");
    BnftpFsm fsm2{ctx, "/proc"};
    (void)fsm2.on_bytes(std::span<const std::byte>(pkt));

    // send_bytes fails → FSM should be Done
    CHECK(fsm2.state() == BnftpFsm::State::Done);
}

// ===========================================================================
// TEST SUITE 1b: Shared-port dispatch init-class byte handling
//   Regression for Finding 1 — the BNFTP init-class octet (0x02) must be
//   consumed by the dispatch before the CLIENT_FILE_REQ is replayed into
//   BnftpFsm, otherwise the {size,type} header is read one byte too early,
//   pkt_type != 0x0100, and the FSM silently closes (every real BNFTP
//   download fails).
// ===========================================================================

TEST_CASE("dispatch: leading 0x02 init byte fed verbatim breaks the FSM (bug)",
          "[bnftp_session][dispatch][regression]") {
    // Demonstrates the failure mode the fix prevents: if the dispatch
    // forwarded the *entire* peek buffer (including the 0x02 init byte)
    // into the FSM, the header is shifted by one and the request is NOT
    // recognized as CLIENT_FILE_REQ — the FSM closes without replying.
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/proc"};

    auto wire = make_wire_open("version");  // exists under /proc
    // Intentionally do NOT strip the init byte — replay verbatim.
    (void)fsm.on_bytes(std::span<const std::byte>(wire));

    // Misparsed header (shifted by the stray 0x02): the request is NOT
    // recognized as CLIENT_FILE_REQ, so the FSM produces NO SERVER_FILE_REPLY
    // — the download silently fails. (It does not reach a successful reply;
    // the stripped path below is what makes it work.)
    CHECK(ctx->sent.empty());
    CHECK(fsm.state() != BnftpFsm::State::Done);
}

TEST_CASE("dispatch: init byte stripped → CLIENT_FILE_REQ parsed and reply sent",
          "[bnftp_session][dispatch][regression]") {
    // The fix: dispatch consumes the leading 0x02 init-class octet, so the
    // remaining bytes start cleanly at the CLIENT_FILE_REQ {size,type}
    // header. The FSM must recognize the request (type 0x0100) and emit a
    // SERVER_FILE_REPLY rather than closing the connection unanswered.
    auto ctx = std::make_shared<FakeFileContext>();
    BnftpFsm fsm{ctx, "/proc"};

    auto wire = make_wire_open("version");  // /proc/version always exists
    auto payload = dispatch_strip_init(std::span<const std::byte>(wire));

    // Sanity: the strip removed exactly the init byte.
    REQUIRE(payload.size() + 1 == wire.size());

    (void)fsm.on_bytes(payload);

    // A reply header was produced (request recognized as CLIENT_FILE_REQ).
    REQUIRE(ctx->sent.size() >= 24);

    // Reply header type field (bytes 2..3) is SERVER_FILE_REPLY == 0x0000,
    // confirming the file-request path was taken (not a misparse/close).
    const auto type_lo = static_cast<std::uint8_t>(ctx->sent[2]);
    const auto type_hi = static_cast<std::uint8_t>(ctx->sent[3]);
    CHECK(static_cast<std::uint16_t>(type_lo | (type_hi << 8)) == 0x0000u);

    // The reply echoes the requested filename ("version") at offset 24.
    REQUIRE(ctx->sent.size() >= 24 + 7);
    const char* fname = reinterpret_cast<const char*>(ctx->sent.data() + 24);
    CHECK(std::string(fname, 7) == "version");
}

TEST_CASE("dispatch: strip helper leaves a non-init first byte untouched",
          "[bnftp_session][dispatch]") {
    // Defensive: the strip must only fire for the 0x02 init octet so it
    // never eats a real header byte on paths that don't carry the init
    // prefix (and never the BNCS 0xFF path, which uses a different branch).
    auto req = make_file_req("foo");
    auto out = dispatch_strip_init(std::span<const std::byte>(req));
    CHECK(out.size() == req.size());
    CHECK(out.data() == req.data());
}

// ===========================================================================
// TEST SUITE 2: FileSessionFactory
// ===========================================================================

TEST_CASE("FileSessionFactory: stores data_dir correctly",
          "[bnftp_session][factory]") {
    const std::filesystem::path dir{"/srv/pvpgn/files"};
    FileSessionFactory factory{dir};

    CHECK(factory.data_dir() == dir);
}

TEST_CASE("FileSessionFactory: null TcpSession is handled gracefully",
          "[bnftp_session][factory]") {
    FileSessionFactory factory{std::filesystem::path{"/tmp"}};

    // Passing nullptr must not crash (factory guards against it)
    REQUIRE_NOTHROW(factory(nullptr));
}

TEST_CASE("FileSessionFactory: data_dir is preserved across copies",
          "[bnftp_session][factory]") {
    const std::filesystem::path dir{"/data/files"};
    FileSessionFactory original{dir};
    FileSessionFactory copy = original;  // FileSessionFactory is copyable

    CHECK(copy.data_dir() == dir);
    CHECK(copy.data_dir() == original.data_dir());
}

TEST_CASE("FileSessionFactory: different instances have independent data_dirs",
          "[bnftp_session][factory]") {
    const std::filesystem::path dir_a{"/srv/a"};
    const std::filesystem::path dir_b{"/srv/b"};

    FileSessionFactory factory_a{dir_a};
    FileSessionFactory factory_b{dir_b};

    CHECK(factory_a.data_dir() == dir_a);
    CHECK(factory_b.data_dir() == dir_b);
    CHECK(factory_a.data_dir() != factory_b.data_dir());
}

TEST_CASE("FileSessionFactory: data_dir survives move construction",
          "[bnftp_session][factory]") {
    const std::filesystem::path dir{"/srv/pvpgn"};
    FileSessionFactory original{dir};
    FileSessionFactory moved = std::move(original);

    CHECK(moved.data_dir() == dir);
}
