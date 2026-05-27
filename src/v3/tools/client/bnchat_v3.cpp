// SPDX-License-Identifier: GPL-2.0-or-later
//
// Modern, self-contained rewrite of the legacy `bnchat` client.
//
// `bnchat` is the largest of the four legacy client tools
// (~1968 LoC of imperative C-with-classes spread across
// bnchat.cpp + client.cpp + client_connect.cpp + udptest.cpp).
// This v3 rewrite focuses on the core flow:
//
//   1. BNet handshake (via `bnclient_login::Session`).
//   2. CLIENT_LOGINREQ1 -- the double-SHA1 password ladder
//      (re-implemented here using `bnclient_hash::bnet_hash`).
//   3. CLIENT_PROGIDENT2 -> drain SERVER_CHANNELLIST.
//   4. CLIENT_JOINCHANNEL.
//   5. Chat loop -- `select()` over stdin + socket, with
//      SERVER_MESSAGE rendering and CLIENT_MESSAGE send.
//
// Compared to the legacy tool we deliberately drop:
//   * the interactive `client_get_comm()` line editor (no
//     character-at-a-time termios mode -- plain line-buffered
//     stdin is enough for piping/scripting);
//   * the `ansi_term` colour glue;
//   * UDP test (`udptest.cpp` / NETINFO);
//   * Diablo 1 PLAYERINFOREQ stat upload.
//
// R197: account creation (`CLIENT_CREATEACCTREQ1`) was previously
// dropped, but the docker-compose end-to-end smoke needs to bootstrap
// an account against a fresh bnetd. Reinstated as an opt-in `-C` /
// `--create-account` flag -- sends CLIENT_CREATEACCTREQ1 once *after*
// the BNet handshake but *before* CLIENT_LOGINREQ1, then proceeds with
// the normal login flow. Result OK and "account already exists" are
// both treated as success (the smoke runs the client multiple times
// against the same persistent state).
//
// Those are valuable but tangential to the modernization
// objective -- the build now produces a `bnchat` binary that
// links only `core` + `${NETWORK_LIBRARIES}` under
// `pvpgn_v3_apply_flags()` (`-Wall -Wextra -Wpedantic -Werror`).

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "bnclient_bnet_packets.hpp"
#include "bnclient_hash.hpp"
#include "bnclient_login.hpp"
#include "bnclient_net.hpp"
#include "bnclient_proto.hpp"

namespace {

using namespace pvpgn::client_v3;

struct Options {
    std::string   host      = "localhost";
    std::uint16_t port      = 6112;
    std::string   clienttag = "CHAT";
    std::string   user;
    std::string   password;
    std::string   channel;
    std::string   cdkey;
    std::string   cdowner   = "owner";
    bool          send_cdkey2 = false;
    bool          create_account = false;
};

[[noreturn]] void usage(const char* prog) {
    std::fprintf(stderr,
        "usage: %s -u USER -p PASS [<options>] [<host> [<port>]]\n"
        "  -u USER, --user=USER       account name (required)\n"
        "  -p PASS, --password=PASS   account password (required)\n"
        "  -c TAG,  --client=TAG      STAR | SEXP | SSHR | DRTL | DSHR\n"
        "                             | W2BN | D2DV | D2XP | WAR3 | CHAT\n"
        "                             (default CHAT)\n"
        "  --channel=NAME             channel to join (default \"<clienttag>\")\n"
        "  -k KEY,  --cdkey=KEY       set CD key (auto-enables CDKEY2 stage)\n"
        "  -o NAME, --cdowner=NAME    CD key owner string\n"
        "  -C,      --create-account  send CLIENT_CREATEACCTREQ1 before LOGINREQ1\n"
        "  -h, --help                 show this help\n"
        "  -v, --version              print version and exit\n",
        prog);
    std::exit(EXIT_FAILURE);
}

bool starts_with(std::string_view s, std::string_view p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
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

Options parse_args(int argc, char** argv) {
    Options o;
    std::vector<std::string> positional;
    auto need_value = [&](int& i, std::string_view flag) -> const char* {
        if (i + 1 >= argc) {
            std::fprintf(stderr, "%s: %.*s requires a value\n",
                argv[0], static_cast<int>(flag.size()), flag.data());
            usage(argv[0]);
        }
        return argv[++i];
    };
    for (int i = 1; i < argc; ++i) {
        std::string_view a{argv[i]};
        if (a == "-h" || a == "--help" || a == "--usage") {
            usage(argv[0]);
        } else if (a == "-v" || a == "--version") {
            std::printf("bnchat (pvpgn v3)\n");
            std::exit(EXIT_SUCCESS);
        } else if (a == "-u") {
            o.user = need_value(i, a);
        } else if (starts_with(a, "--user=")) {
            o.user = std::string{a.substr(7)};
        } else if (a == "-p") {
            o.password = need_value(i, a);
        } else if (starts_with(a, "--password=")) {
            o.password = std::string{a.substr(11)};
        } else if (a == "-c") {
            o.clienttag = need_value(i, a);
        } else if (starts_with(a, "--client=")) {
            o.clienttag = std::string{a.substr(9)};
        } else if (starts_with(a, "--channel=")) {
            o.channel = std::string{a.substr(10)};
        } else if (a == "-k") {
            o.cdkey = need_value(i, a);
            o.send_cdkey2 = true;
        } else if (starts_with(a, "--cdkey=")) {
            o.cdkey = std::string{a.substr(8)};
            o.send_cdkey2 = true;
        } else if (a == "-o") {
            o.cdowner = need_value(i, a);
        } else if (starts_with(a, "--cdowner=")) {
            o.cdowner = std::string{a.substr(10)};
        } else if (a == "-C" || a == "--create-account") {
            o.create_account = true;
        } else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "%s: unknown option \"%.*s\"\n",
                argv[0], static_cast<int>(a.size()), a.data());
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
            std::fprintf(stderr, "%s: bad port \"%s\"\n",
                argv[0], positional[1].c_str());
            usage(argv[0]);
        }
    }
    if (o.user.empty() || o.password.empty()) {
        std::fprintf(stderr, "%s: -u and -p are required\n", argv[0]);
        usage(argv[0]);
    }
    if (o.channel.empty()) {
        o.channel = o.clienttag;
    }
    return o;
}

std::string to_lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return s;
}

// ---- LOGINREQ1 helper ------------------------------------------------

bool send_loginreq1(net::socket_t sd, const std::string& user,
                    const std::string& password, std::uint32_t sessionkey) {
    const std::string pw = to_lower(password);

    // hash1 = bnet_hash(password)
    const hash::HashDigest hash1 =
        hash::bnet_hash(pw.data(), pw.size());

    // Build the 28-byte preimage for hash2: ticks=0, sessionkey,
    // then hash1 as 5 LE 32-bit ints.
#pragma pack(push, 1)
    struct {
        proto::bn_int ticks;
        proto::bn_int sessionkey;
        proto::bn_int hash1[5];
    } seed{};
#pragma pack(pop)
    static_assert(sizeof(seed) == 28);
    proto::int_set(seed.ticks,      0);
    proto::int_set(seed.sessionkey, sessionkey);
    for (std::size_t i = 0; i < 5; ++i) {
        proto::int_set(seed.hash1[i], hash1[i]);
    }
    const hash::HashDigest hash2 = hash::bnet_hash(&seed, sizeof(seed));

    proto::Packet p;
    p.resize(proto::kBnetHeaderSize + sizeof(bnet::CClientLoginReq1));
    p.set_bnet_type(bnet::packet_id::CLIENT_LOGINREQ1);
    auto* req = p.body_as<bnet::CClientLoginReq1>();
    proto::int_set(req->ticks,      0);
    proto::int_set(req->sessionkey, sessionkey);
    for (std::size_t i = 0; i < 5; ++i) {
        proto::int_set(req->password_hash2[i], hash2[i]);
    }
    p.append_cstr(user.c_str());
    p.set_bnet_size(static_cast<std::uint16_t>(p.size()));
    return proto::send_bnet(sd, p);
}

// ---- CREATEACCTREQ1 helper (R197) ------------------------------------

bool send_createacctreq1(net::socket_t sd, const std::string& user,
                         const std::string& password) {
    const std::string pw = to_lower(password);
    const hash::HashDigest hash1 =
        hash::bnet_hash(pw.data(), pw.size());

    proto::Packet p;
    p.resize(proto::kBnetHeaderSize + sizeof(bnet::CClientCreateAcctReq1));
    p.set_bnet_type(bnet::packet_id::CLIENT_CREATEACCTREQ1);
    auto* req = p.body_as<bnet::CClientCreateAcctReq1>();
    for (std::size_t i = 0; i < 5; ++i) {
        proto::int_set(req->password_hash1[i], hash1[i]);
    }
    p.append_cstr(user.c_str());
    p.set_bnet_size(static_cast<std::uint16_t>(p.size()));
    return proto::send_bnet(sd, p);
}

const char* msg_type_str(std::uint32_t t) {
    switch (t) {
        case bnet::kMsgType_AddUser:    return "USER";
        case bnet::kMsgType_Join:       return "JOIN";
        case bnet::kMsgType_Part:       return "PART";
        case bnet::kMsgType_Whisper:    return "FROM";
        case bnet::kMsgType_Talk:       return "TALK";
        case bnet::kMsgType_Broadcast:  return "BCST";
        case bnet::kMsgType_Channel:    return "CHAN";
        case bnet::kMsgType_UserFlags:  return "FLAG";
        case bnet::kMsgType_WhisperAck: return "TO  ";
        case bnet::kMsgType_Info:       return "INFO";
        case bnet::kMsgType_Error:      return "ERR ";
        case bnet::kMsgType_Emote:      return "EMOT";
        default:                        return "????";
    }
}

void render_server_message(const proto::Packet& p) {
    const auto* m = p.body_as<bnet::SServerMessage>();
    if (!m) {
        return;
    }
    const std::uint32_t type = proto::int_get(m->type);
    // Strings start right after the SServerMessage body.
    const auto* base = reinterpret_cast<const char*>(m);
    const std::size_t body_max =
        p.size() >= proto::kBnetHeaderSize
            ? p.size() - proto::kBnetHeaderSize
            : 0;
    if (body_max < sizeof(bnet::SServerMessage)) {
        return;
    }
    const char* end = base + body_max;
    const char* name = base + sizeof(bnet::SServerMessage);
    if (name >= end) {
        return;
    }
    const std::size_t name_len = ::strnlen(name, end - name);
    const char* text = name + name_len + 1;
    if (text > end) {
        text = "";
    }
    const std::size_t text_len = ::strnlen(text, end - text);
    std::printf("[%s] %.*s: %.*s\n", msg_type_str(type),
        static_cast<int>(name_len), name,
        static_cast<int>(text_len), text);
    std::fflush(stdout);
}

// ---- send CLIENT_MESSAGE ---------------------------------------------

bool send_chat_message(net::socket_t sd, std::string_view text) {
    proto::Packet p;
    p.resize(proto::kBnetHeaderSize);
    p.set_bnet_type(bnet::packet_id::CLIENT_MESSAGE);
    p.append(text.data(), text.size());
    const char nul = '\0';
    p.append(&nul, 1);
    p.set_bnet_size(static_cast<std::uint16_t>(p.size()));
    return proto::send_bnet(sd, p);
}

} // namespace

int main(int argc, char** argv) {
    Options opts = parse_args(argc, argv);

    // ---- BNet handshake via the shared login driver. ----
    login::Config cfg;
    cfg.server      = opts.host;
    cfg.port        = opts.port;
    cfg.clienttag   = opts.clienttag;
    cfg.cdowner     = opts.cdowner;
    cfg.cdkey       = opts.cdkey;
    cfg.ignoreversion = true;
    cfg.send_cdkey2 = opts.send_cdkey2
        || opts.clienttag == "STAR"
        || opts.clienttag == "SEXP"
        || opts.clienttag == "W2BN";

    login::Session sess{cfg};
    login::Result lr{};
    if (!sess.run(lr)) {
        std::fprintf(stderr, "%s: handshake failed: %s\n",
            argv[0], sess.error().c_str());
        return EXIT_FAILURE;
    }
    net::socket_holder sock{lr.sock};
    std::fprintf(stderr,
        "%s: handshake ok, sessionkey=0x%08x sessionnum=0x%08x\n",
        argv[0], lr.sessionkey, lr.sessionnum);

    // ---- (R197) optional CLIENT_CREATEACCTREQ1 ----
    proto::Packet p;
    if (opts.create_account) {
        if (!send_createacctreq1(sock.get(), opts.user, opts.password)) {
            std::fprintf(stderr,
                "%s: send CLIENT_CREATEACCTREQ1 failed\n", argv[0]);
            return EXIT_FAILURE;
        }
        for (;;) {
            if (!proto::recv_bnet(sock.get(), p)) {
                std::fprintf(stderr,
                    "%s: server closed before CREATEACCTREPLY1\n", argv[0]);
                return EXIT_FAILURE;
            }
            if (p.bnet_type() == bnet::packet_id::SERVER_CREATEACCTREPLY1) {
                break;
            }
        }
        {
            const auto* reply = p.body_as<bnet::SServerCreateAcctReply1>();
            const std::uint32_t r =
                reply ? proto::int_get(reply->result) : 0;
            // Both Ok (new account) and No (already exists) are treated
            // as success so the smoke is idempotent across restarts.
            std::fprintf(stderr,
                "%s: createacctreply1 result=0x%08x (%s)\n",
                argv[0], r,
                r == bnet::kCreateAcctReply1_Ok
                    ? "created"
                    : "already-exists-or-rejected (continuing)");
        }
    }

    // ---- CLIENT_LOGINREQ1 -> SERVER_LOGINREPLY1 ----
    if (!send_loginreq1(sock.get(), opts.user, opts.password, lr.sessionkey)) {
        std::fprintf(stderr, "%s: send CLIENT_LOGINREQ1 failed\n", argv[0]);
        return EXIT_FAILURE;
    }
    for (;;) {
        if (!proto::recv_bnet(sock.get(), p)) {
            std::fprintf(stderr,
                "%s: server closed before LOGINREPLY1\n", argv[0]);
            return EXIT_FAILURE;
        }
        if (p.bnet_type() == bnet::packet_id::SERVER_LOGINREPLY1) {
            break;
        }
    }
    {
        const auto* reply = p.body_as<bnet::SServerLoginReply1>();
        const std::uint32_t msg =
            reply ? proto::int_get(reply->message) : 0;
        if (msg != bnet::kLoginReply1_Success) {
            std::fprintf(stderr,
                "%s: login refused (LOGINREPLY1 message=0x%08x)\n",
                argv[0], msg);
            return EXIT_FAILURE;
        }
    }
    std::fprintf(stderr, "%s: login ok as \"%s\"\n",
        argv[0], opts.user.c_str());

    // ---- CLIENT_PROGIDENT2 -> drain SERVER_CHANNELLIST ----
    {
        proto::Packet pi;
        pi.resize(proto::kBnetHeaderSize + sizeof(bnet::CClientProgIdent2));
        pi.set_bnet_type(bnet::packet_id::CLIENT_PROGIDENT2);
        auto* prog = pi.body_as<bnet::CClientProgIdent2>();
        char ct[4]{};
        for (std::size_t i = 0; i < 4; ++i) {
            ct[i] = (i < opts.clienttag.size()) ? opts.clienttag[i] : '\0';
        }
        proto::int_tag_set(prog->clienttag, ct);
        pi.set_bnet_size(static_cast<std::uint16_t>(pi.size()));
        if (!proto::send_bnet(sock.get(), pi)) {
            std::fprintf(stderr, "%s: send CLIENT_PROGIDENT2 failed\n",
                argv[0]);
            return EXIT_FAILURE;
        }
    }
    for (;;) {
        if (!proto::recv_bnet(sock.get(), p)) {
            std::fprintf(stderr,
                "%s: server closed before CHANNELLIST\n", argv[0]);
            return EXIT_FAILURE;
        }
        if (p.bnet_type() == bnet::packet_id::SERVER_CHANNELLIST) {
            break;
        }
    }

    // ---- CLIENT_JOINCHANNEL ----
    {
        proto::Packet pj;
        pj.resize(proto::kBnetHeaderSize + sizeof(bnet::CClientJoinChannel));
        pj.set_bnet_type(bnet::packet_id::CLIENT_JOINCHANNEL);
        auto* jc = pj.body_as<bnet::CClientJoinChannel>();
        proto::int_set(jc->channelflag, bnet::kJoinChannel_Generic);
        pj.append_cstr(opts.channel.c_str());
        pj.set_bnet_size(static_cast<std::uint16_t>(pj.size()));
        if (!proto::send_bnet(sock.get(), pj)) {
            std::fprintf(stderr, "%s: send CLIENT_JOINCHANNEL failed\n",
                argv[0]);
            return EXIT_FAILURE;
        }
    }
    std::fprintf(stderr, "%s: joining channel \"%s\"...\n",
        argv[0], opts.channel.c_str());

    // ---- Chat loop: select() on stdin + socket. -----------------------
    // Line-buffered stdin: read until newline -> CLIENT_MESSAGE.
    const int sd = static_cast<int>(sock.get());
    std::string line_buf;
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(sd, &rfds);
        const int maxfd = std::max(sd, STDIN_FILENO);
        const int n = ::select(maxfd + 1, &rfds, nullptr, nullptr, nullptr);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::fprintf(stderr, "%s: select: %s\n",
                argv[0], std::strerror(errno));
            return EXIT_FAILURE;
        }
        if (FD_ISSET(sd, &rfds)) {
            if (!proto::recv_bnet(sock.get(), p)) {
                std::fprintf(stderr, "%s: server disconnected\n", argv[0]);
                return EXIT_SUCCESS;
            }
            if (p.bnet_type() == bnet::packet_id::SERVER_MESSAGE) {
                render_server_message(p);
            }
            // Silently ignore other types (ping, etc.).
        }
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            char chunk[256];
            const ssize_t got =
                ::read(STDIN_FILENO, chunk, sizeof(chunk));
            if (got <= 0) {
                std::fprintf(stderr, "%s: stdin closed, exiting\n", argv[0]);
                return EXIT_SUCCESS;
            }
            line_buf.append(chunk, static_cast<std::size_t>(got));
            for (;;) {
                const auto nl = line_buf.find('\n');
                if (nl == std::string::npos) {
                    break;
                }
                std::string line = line_buf.substr(0, nl);
                line_buf.erase(0, nl + 1);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (line.empty()) {
                    continue;
                }
                if (line == "/quit" || line == "/exit") {
                    std::fprintf(stderr, "%s: bye\n", argv[0]);
                    return EXIT_SUCCESS;
                }
                if (!send_chat_message(sock.get(), line)) {
                    std::fprintf(stderr,
                        "%s: send CLIENT_MESSAGE failed\n", argv[0]);
                    return EXIT_FAILURE;
                }
            }
        }
    }
}
