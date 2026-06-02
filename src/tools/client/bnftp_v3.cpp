// SPDX-License-Identifier: GPL-2.0-or-later
//
// Modern, self-contained rewrite of the legacy `bnftp` client.
//
// `bnftp` connects to a bnetd-style server, sends the FILE-class
// init octet (`0x02`), issues a `CLIENT_FILE_REQ` packet, and
// streams the resulting file body to disk.  It does NOT use the
// BNet handshake -- no auth, no login.  This makes it the smallest
// of the four legacy client tools.
//
// Compared to the legacy `bnftp.cpp` (~800 LoC) this version:
//   * drops the dependency on `common/*` / `compat/*`; links only
//     `core ${NETWORK_LIBRARIES}` under `pvpgn_v3_apply_flags()`
//     (`-Wall -Wextra -Wpedantic -Werror`);
//   * uses the vendored `bnclient_net.hpp` for portable POSIX /
//     Winsock2 socket calls and an inline FILE-class header
//     (`{ size, type }`, LE -- note the order is the OPPOSITE of
//     the BNet `{ type, size }` header);
//   * keeps the protocol surface simple but supports BOTH the
//     legacy `CLIENT_FILE_REQ` (type `0x0100`) and the
//     Warcraft III three-step exchange
//     (`CLIENT_FILE_REQ2` + 4-byte unknown reply +
//     raw `CLIENT_FILE_REQ3`).  Pass `--war3` (or use the WAR3 /
//     W3XP client tag) to enable the W3 path.
//   * no interactive prompts -- `-f FILE` is required, plus optional
//     `--exists=ACTION` (overwrite | backup | resume).
//
// Usage:
//   bnftp -f FILENAME [-c CLIENTTAG] [-a ARCHTAG]
//         [--startoffset=N] [--exists=O|B|R]
//         [HOST [PORT]]
//
// Defaults: HOST=localhost, PORT=6112, CLIENTTAG=STAR, ARCHTAG=IX86,
//           startoffset=0, exists=overwrite.

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <print>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <vector>

#include "bnclient_net.hpp"
#include "bnclient_proto.hpp"

namespace {

using namespace pvpgn::client_v3;

// ---- FILE-class header & packet ids ----------------------------------
//
// The FILE-class transport uses a 4-byte little-endian header
// laid out as `{ size, type }` -- size first.  That is the
// OPPOSITE of the BNet class header `{ type, size }` in
// `bnclient_proto.hpp`, so we cannot reuse `Packet::set_bnet_*`
// here and inline a tiny framing helper instead.

#pragma pack(push, 1)
struct FileHeader {
    proto::bn_short size;
    proto::bn_short type;
};
static_assert(sizeof(FileHeader) == 4);

struct CClientFileReq {
    FileHeader   h;
    proto::bn_int  archtag;
    proto::bn_int  clienttag;
    proto::bn_int  adid;
    proto::bn_int  extensiontag;
    proto::bn_int  startoffset;
    proto::bn_long timestamp;
    // followed by NUL-terminated filename
};
static_assert(sizeof(CClientFileReq) == 4 + 4 + 4 + 4 + 4 + 4 + 8);

// Warcraft III 3-step protocol: first the client sends a tiny
// REQ2 with just arch/client tags, the server echoes back a
// 4-byte unknown body, and then the client sends a *raw*
// (no FILE header!) REQ3 carrying the timestamp/unknown fields
// followed by the requested filename.
struct CClientFileReq2 {
    FileHeader   h;
    proto::bn_int  archtag;
    proto::bn_int  clienttag;
    proto::bn_long unknown1;
};
static_assert(sizeof(CClientFileReq2) == 4 + 4 + 4 + 8);

struct CClientFileReq3 {
    // NOTE: no FileHeader -- this packet ships as raw bytes
    // immediately after the server's 4-byte unknown reply.
    proto::bn_int  unknown1;
    proto::bn_long timestamp;
    proto::bn_long unknown2;
    proto::bn_long unknown3;
    proto::bn_long unknown4;
    proto::bn_long unknown5;
    proto::bn_long unknown6;
    // followed by NUL-terminated filename
};
static_assert(sizeof(CClientFileReq3) == 4 + 8 * 6);

struct SServerFileReply {
    FileHeader   h;
    proto::bn_int  filelen;
    proto::bn_int  adid;
    proto::bn_int  extensiontag;
    proto::bn_long timestamp;
    // followed by NUL-terminated filename
};
static_assert(sizeof(SServerFileReply) == 4 + 4 + 4 + 4 + 8);
#pragma pack(pop)

constexpr std::uint16_t kClientFileReqType   = 0x0100;
constexpr std::uint16_t kClientFileReq2Type  = 0x0200;
constexpr std::uint16_t kServerFileReplyType = 0x0000;
constexpr std::size_t   kMaxFileChunk        = 4096;

// ---- CLI ----------------------------------------------------------------

enum class ExistsAction { Overwrite, Backup, Resume };

struct Options {
    std::string   host          = "localhost";
    std::uint16_t port          = 6112;
    std::string   clienttag     = "STAR";
    std::string   archtag       = "IX86";
    std::string   reqfile;
    std::uint32_t startoffset   = 0;
    ExistsAction  exists        = ExistsAction::Overwrite;
    bool          want_war3     = false;
};

[[noreturn]] void usage(const char* prog) {
    std::print(stderr,
        "usage: {} -f FILE [<options>] [<host> [<port>]]\n"
        "  -f FILE, --file=FILE    file name to fetch (required)\n"
        "  -c TAG,  --client=TAG   STAR | SEXP | SSHR | DRTL | DSHR\n"
        "                          | W2BN | D2DV | D2XP | WAR3 (default STAR)\n"
        "  -a TAG,  --arch=TAG     IX86 | PMAC | XMAC (default IX86)\n"
        "  --startoffset=N         resume offset, default 0\n"
        "  --exists=ACTION         overwrite | backup | resume (default overwrite)\n"
        "  --war3                  use the W3-style 3-step protocol\n"
        "                          (auto-enabled for --client=WAR3/W3XP)\n"
        "  -h, --help              show this help\n"
        "  -v, --version           print version and exit\n",
        prog);
    std::exit(EXIT_FAILURE);
}

bool parse_ushort(const char* s, std::uint16_t& out) {
    char* end = nullptr;
    const long v = std::strtol(s, &end, 10);
    if (!end || *end != '\0' || v < 0 || v > 0xffff) {
        return false;
    }
    out = static_cast<std::uint16_t>(v);
    return true;
}

bool parse_uint(const char* s, std::uint32_t& out) {
    char* end = nullptr;
    const long long v = std::strtoll(s, &end, 0);
    if (!end || *end != '\0' || v < 0) {
        return false;
    }
    out = static_cast<std::uint32_t>(v);
    return true;
}

bool starts_with(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size()
        && s.compare(0, prefix.size(), prefix) == 0;
}

Options parse_args(int argc, char** argv) {
    Options o;
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string_view a{argv[i]};
        auto need_value = [&](std::string_view flag) -> const char* {
            if (i + 1 >= argc) {
                std::println(stderr, "{}: {} requires a value", argv[0], flag);
                usage(argv[0]);
            }
            return argv[++i];
        };
        if (a == "-h" || a == "--help" || a == "--usage") {
            usage(argv[0]);
        } else if (a == "-v" || a == "--version") {
            std::println("bnftp (pvpgn v3)");
            std::exit(EXIT_SUCCESS);
        } else if (a == "-f") {
            o.reqfile = need_value(a);
        } else if (starts_with(a, "--file=")) {
            o.reqfile = std::string{a.substr(7)};
        } else if (a == "-c") {
            o.clienttag = need_value(a);
        } else if (starts_with(a, "--client=")) {
            o.clienttag = std::string{a.substr(9)};
        } else if (a == "-a") {
            o.archtag = need_value(a);
        } else if (starts_with(a, "--arch=")) {
            o.archtag = std::string{a.substr(7)};
        } else if (starts_with(a, "--startoffset=")) {
            if (!parse_uint(a.substr(14).data(), o.startoffset)) {
                std::println(stderr, "{}: bad --startoffset value", argv[0]);
                usage(argv[0]);
            }
        } else if (starts_with(a, "--exists=")) {
            std::string_view val = a.substr(9);
            if (val == "O" || val == "Overwrite" || val == "overwrite") {
                o.exists = ExistsAction::Overwrite;
            } else if (val == "B" || val == "Backup" || val == "backup") {
                o.exists = ExistsAction::Backup;
            } else if (val == "R" || val == "Resume" || val == "resume") {
                o.exists = ExistsAction::Resume;
            } else {
                std::println(stderr, "{}: --exists must be O|B|R", argv[0]);
                usage(argv[0]);
            }
        } else if (a == "--war3") {
            o.want_war3 = true;
        } else if (!a.empty() && a[0] == '-') {
            std::println(stderr, "{}: unknown option \"{}\"", argv[0], a);
            usage(argv[0]);
        } else {
            positional.emplace_back(a);
        }
    }

    if (positional.size() > 2) {
        usage(argv[0]);
    }
    if (positional.size() >= 1) {
        o.host = positional[0];
    }
    if (positional.size() == 2) {
        if (!parse_ushort(positional[1].c_str(), o.port)) {
            std::println(stderr, "{}: \"{}\" should be a positive port number",
                argv[0], positional[1].c_str());
            usage(argv[0]);
        }
    }
    if (o.reqfile.empty()) {
        std::println(stderr, "{}: -f FILE is required", argv[0]);
        usage(argv[0]);
    }
    // WAR3/W3XP always use the 3-step protocol.
    if (o.clienttag == "WAR3" || o.clienttag == "W3XP") {
        o.want_war3 = true;
    }
    return o;
}

// ---- existence handling ----------------------------------------------

bool handle_existing(const Options& opts, std::uint32_t& startoffset) {
    struct stat st{};
    if (::stat(opts.reqfile.c_str(), &st) != 0) {
        return true;  // no pre-existing file -- nothing to do
    }
    switch (opts.exists) {
        case ExistsAction::Overwrite:
            return true;
        case ExistsAction::Resume:
            startoffset = static_cast<std::uint32_t>(st.st_size);
            return true;
        case ExistsAction::Backup: {
            for (unsigned n = 0; n < 100; ++n) {
                std::string bak = opts.reqfile + "." + std::to_string(n);
                struct stat bst{};
                if (::stat(bak.c_str(), &bst) == 0) {
                    continue;
                }
                if (std::rename(opts.reqfile.c_str(), bak.c_str()) != 0) {
                    std::println(stderr, "rename(\"{}\", \"{}\") failed: {}",
                        opts.reqfile.c_str(), bak.c_str(), std::strerror(errno));
                    return false;
                }
                std::println(stderr, "renamed existing \"{}\" -> \"{}\"",
                    opts.reqfile.c_str(), bak.c_str());
                return true;
            }
            std::println(stderr, "could not find an unused backup name for \"{}\"",
                opts.reqfile.c_str());
            return false;
        }
    }
    return true;
}

// ---- FILE-class framing helpers --------------------------------------

void file_header_set(FileHeader& h, std::uint16_t size, std::uint16_t type) {
    proto::short_set(h.size, size);
    proto::short_set(h.type, type);
}

bool send_buf(net::socket_t sd, const void* buf, std::size_t n) {
    return net::send_all(sd, buf, n);
}

bool recv_buf(net::socket_t sd, void* buf, std::size_t n) {
    return net::recv_all(sd, buf, n);
}

// Receive a FILE-class packet body of exactly `body_size` bytes
// preceded by a `{ size, type }` header.  Returns false on
// short-read or size mismatch.
[[maybe_unused]] bool recv_file_packet_fixed(net::socket_t sd, void* body, std::size_t body_size,
                            std::uint16_t& out_type, std::uint16_t& out_size) {
    FileHeader h{};
    if (!recv_buf(sd, &h, sizeof(h))) {
        return false;
    }
    out_size = proto::short_get(h.size);
    out_type = proto::short_get(h.type);
    if (out_size < sizeof(FileHeader)) {
        return false;
    }
    const std::size_t left = out_size - sizeof(FileHeader);
    if (left < body_size) {
        return false;
    }
    if (body_size > 0 && !recv_buf(sd, body, body_size)) {
        return false;
    }
    // Drain any extra bytes the server tacked on past `body_size`.
    std::size_t extra = left - body_size;
    char scratch[256];
    while (extra > 0) {
        const std::size_t chunk = std::min(extra, sizeof(scratch));
        if (!recv_buf(sd, scratch, chunk)) {
            return false;
        }
        extra -= chunk;
    }
    return true;
}

// Receive header + variable-length body into `out` (cleared first).
// On success `out` holds bytes WITHOUT the 4-byte header.
bool recv_file_packet(net::socket_t sd, std::vector<std::uint8_t>& out,
                      std::uint16_t& out_type) {
    FileHeader h{};
    if (!recv_buf(sd, &h, sizeof(h))) {
        return false;
    }
    const std::uint16_t sz = proto::short_get(h.size);
    out_type = proto::short_get(h.type);
    if (sz < sizeof(FileHeader)) {
        return false;
    }
    const std::size_t body = sz - sizeof(FileHeader);
    out.assign(body, std::uint8_t{0});
    if (body > 0 && !recv_buf(sd, out.data(), body)) {
        return false;
    }
    return true;
}

void pack_tag(const std::string& s, char out[4]) noexcept {
    for (std::size_t i = 0; i < 4; ++i) {
        out[i] = (i < s.size()) ? s[i] : '\0';
    }
}

} // namespace

int main(int argc, char** argv) {
    Options opts = parse_args(argc, argv);

    if (!handle_existing(opts, opts.startoffset)) {
        return EXIT_FAILURE;
    }

    net::sockets_startup();
    net::socket_holder sock{net::connect_tcp(opts.host, opts.port)};
    if (!sock.valid()) {
        std::println(stderr, "{}: connect to {}:{} failed", argv[0],
            opts.host.c_str(), opts.port);
        return EXIT_FAILURE;
    }
    std::println(stderr, "{}: connected to {}:{}",
        argv[0], opts.host.c_str(), opts.port);

    if (!proto::send_init_classbyte(sock.get(), proto::init_class::File)) {
        std::println(stderr, "{}: failed to send init class byte", argv[0]);
        return EXIT_FAILURE;
    }

    char arch_chars[4]{};  pack_tag(opts.archtag,   arch_chars);
    char ctag_chars[4]{};  pack_tag(opts.clienttag, ctag_chars);

    if (opts.want_war3) {
        // ---- 3-step W3 protocol ----
        // Step 1: send CLIENT_FILE_REQ2 (arch + clienttag only).
        std::vector<std::uint8_t> req2_buf(sizeof(CClientFileReq2),
            std::uint8_t{0});
        auto* r2 = reinterpret_cast<CClientFileReq2*>(req2_buf.data());
        file_header_set(r2->h,
            static_cast<std::uint16_t>(sizeof(CClientFileReq2)),
            kClientFileReq2Type);
        proto::int_tag_set(r2->archtag,   arch_chars);
        proto::int_tag_set(r2->clienttag, ctag_chars);
        // unknown1 stays zero
        if (!send_buf(sock.get(), req2_buf.data(), req2_buf.size())) {
            std::println(stderr, "{}: failed to send CLIENT_FILE_REQ2", argv[0]);
            return EXIT_FAILURE;
        }

        // Step 2: drain the server's 4-byte unknown reply (header
        // + 4 body bytes).  Anything sensible is fine -- we just
        // need to consume the bytes so the stream is positioned
        // correctly for REQ3.
        std::vector<std::uint8_t> unk_body;
        std::uint16_t unk_type = 0;
        if (!recv_file_packet(sock.get(), unk_body, unk_type)) {
            std::println(stderr, "{}: server closed before W3 unknown1 reply", argv[0]);
            return EXIT_FAILURE;
        }
        std::println(stderr, "{}: W3 server unknown1 reply type=0x{:04x} body={} byte(s)",
            argv[0], unk_type, unk_body.size());

        // Step 3: send CLIENT_FILE_REQ3 *raw* -- no FILE header --
        // body + filename.
        const std::size_t r3_size =
            sizeof(CClientFileReq3) + opts.reqfile.size() + 1;
        std::vector<std::uint8_t> r3_buf(r3_size, std::uint8_t{0});
        // all fields zero except the trailing filename.
        std::memcpy(r3_buf.data() + sizeof(CClientFileReq3),
            opts.reqfile.c_str(), opts.reqfile.size() + 1);
        if (!send_buf(sock.get(), r3_buf.data(), r3_buf.size())) {
            std::println(stderr, "{}: failed to send CLIENT_FILE_REQ3", argv[0]);
            return EXIT_FAILURE;
        }
    } else {
        // ---- legacy 1-step protocol ----
        // Build CLIENT_FILE_REQ directly into a local buffer
        // -- header + struct body + NUL-terminated filename.
        const std::size_t total_size =
            sizeof(CClientFileReq) + opts.reqfile.size() + 1;
        if (total_size > 0xffff) {
            std::println(stderr, "{}: requested filename too long", argv[0]);
            return EXIT_FAILURE;
        }
        std::vector<std::uint8_t> buf(total_size, std::uint8_t{0});
        auto* req = reinterpret_cast<CClientFileReq*>(buf.data());
        file_header_set(req->h,
            static_cast<std::uint16_t>(total_size), kClientFileReqType);
        proto::int_tag_set(req->archtag,      arch_chars);
        proto::int_tag_set(req->clienttag,    ctag_chars);
        proto::int_set    (req->adid,         0);
        proto::int_set    (req->extensiontag, 0);
        proto::int_set    (req->startoffset,  opts.startoffset);
        // timestamp left zeroed.
        std::memcpy(buf.data() + sizeof(CClientFileReq),
            opts.reqfile.c_str(), opts.reqfile.size() + 1);

        if (!send_buf(sock.get(), buf.data(), buf.size())) {
            std::println(stderr, "{}: failed to send CLIENT_FILE_REQ", argv[0]);
            return EXIT_FAILURE;
        }
    }
    std::println(stderr, "{}: requested \"{}\"{}",
        argv[0], opts.reqfile.c_str(),
        opts.want_war3 ? " (W3 protocol)" : "");

    // Receive the file reply: header { size, type=0x0000 } + body
    // (filelen + adid + extensiontag + timestamp + filename), then
    // the raw file bytes (size == filelen).
    std::vector<std::uint8_t> reply;
    std::uint16_t reply_type = 0;
    if (!recv_file_packet(sock.get(), reply, reply_type)) {
        std::println(stderr, "{}: server closed before SERVER_FILE_REPLY", argv[0]);
        return EXIT_FAILURE;
    }
    if (reply_type != kServerFileReplyType) {
        std::println(stderr, "{}: unexpected reply type 0x{:04x} (expected 0x0000)",
            argv[0], reply_type);
        return EXIT_FAILURE;
    }
    if (reply.size() < sizeof(SServerFileReply) - sizeof(FileHeader)) {
        std::println(stderr, "{}: truncated SERVER_FILE_REPLY", argv[0]);
        return EXIT_FAILURE;
    }
    const proto::bn_int* filelen_bytes =
        reinterpret_cast<const proto::bn_int*>(reply.data());
    const std::uint32_t filelen = proto::int_get(*filelen_bytes);

    const char* svr_name =
        reinterpret_cast<const char*>(reply.data())
        + (sizeof(SServerFileReply) - sizeof(FileHeader));
    std::println(stderr, "{}: server file = \"{}\", length = {} bytes",
        argv[0], svr_name, filelen);

    // Open the local file.
    const char* mode =
        (opts.exists == ExistsAction::Resume) ? "ab" : "wb";
    std::FILE* fp = std::fopen(opts.reqfile.c_str(), mode);
    if (!fp) {
        std::println(stderr, "{}: open \"{}\" ({}): {}",
            argv[0], opts.reqfile.c_str(), mode, std::strerror(errno));
        return EXIT_FAILURE;
    }

    std::uint32_t to_read = filelen;
    if (opts.startoffset > 0 && opts.startoffset <= filelen) {
        to_read = filelen - opts.startoffset;
        std::println(stderr, "{}: resuming at offset {} ({} bytes remaining)",
            argv[0], opts.startoffset, to_read);
    }

    std::vector<std::uint8_t> chunk(kMaxFileChunk);
    while (to_read > 0) {
        const std::size_t want =
            std::min<std::size_t>(to_read, chunk.size());
        if (!recv_buf(sock.get(), chunk.data(), want)) {
            std::println(stderr, "\n{}: server closed mid-stream with {} bytes left",
                argv[0], to_read);
            std::fclose(fp);
            return EXIT_FAILURE;
        }
        if (std::fwrite(chunk.data(), 1, want, fp) != want) {
            std::println(stderr, "\n{}: write to \"{}\" failed: {}",
                argv[0], opts.reqfile.c_str(), std::strerror(errno));
            std::fclose(fp);
            return EXIT_FAILURE;
        }
        to_read -= static_cast<std::uint32_t>(want);
    }

    if (std::fclose(fp) != 0) {
        std::println(stderr, "{}: close \"{}\" failed: {}",
            argv[0], opts.reqfile.c_str(), std::strerror(errno));
        return EXIT_FAILURE;
    }
    std::println(stderr, "{}: done, wrote {} bytes to \"{}\"",
        argv[0], filelen, opts.reqfile.c_str());
    return EXIT_SUCCESS;
}
