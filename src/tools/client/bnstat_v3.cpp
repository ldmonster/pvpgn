// SPDX-License-Identifier: GPL-2.0-or-later
//
// Modern, self-contained rewrite of the legacy `bnstat` client.
//
// Connects to a bnetd server, runs the BNet pre-login handshake via
// `pvpgn::client_v3::login::Session`, then for each requested player
// name sends a `CLIENT_STATSREQ` with a clienttag-specific key set
// and prints the `SERVER_STATSREPLY` field-by-field.
//
// Compared to the legacy `bnstat.cpp` (~1150 LoC) this version:
//   * drops the dependency on `common/*` / `compat/*` -- it links
//     only `core` + `${NETWORK_LIBRARIES}` and compiles under
//     `pvpgn_v3_apply_flags()` (`-Wall -Wextra -Wpedantic -Werror`);
//   * skips the elaborate per-key pretty printer and just prints
//     `key = value` lines (one per requested field).  The wire
//     side is identical, only the screen formatting is simpler;
//   * runs once per player on the command line (no interactive
//     `client_get_comm` loop) -- pass repeated `-p NAME` for
//     multiple players, or use stdin (`-` reads one player name
//     per line).
//
// Usage:
//   bnstat [-c CLIENTTAG] [-i] [-o OWNER] [-k CDKEY]
//          [--bnetd] [--fsgs] -p NAME [-p NAME ...] [HOST [PORT]]
//
// Defaults: HOST=localhost, PORT=6112, CLIENTTAG=STAR, ignoreversion=true.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "bnclient_bnet_packets.hpp"
#include "bnclient_login.hpp"
#include "bnclient_net.hpp"
#include "bnclient_proto.hpp"

namespace {

using namespace pvpgn::client_v3;

struct Options {
    std::string              host          = "localhost";
    std::uint16_t            port          = 6112;
    std::string              clienttag     = "STAR";
    std::string              cdowner       = "owner";
    std::string              cdkey;
    bool                     ignoreversion = true;
    bool                     bnetd         = false;
    bool                     fsgs          = false;
    std::vector<std::string> players;
    bool                     read_stdin    = false;
};

[[noreturn]] void usage(const char* prog) {
    std::fprintf(stderr,
        "usage: %s [<options>] [<host> [<port>]]\n"
        "  -c TAG, --client=TAG    STAR | SEXP | SSHR | DRTL | DSHR\n"
        "                          | W2BN | D2DV | D2XP | WAR3 (default STAR)\n"
        "  -p NAME, --player=NAME  query stats for NAME (repeatable)\n"
        "  --stdin                 also read player names from stdin\n"
        "  -o NAME, --owner=NAME   report CD owner as NAME\n"
        "  -k KEY,  --cdkey=KEY    report CD key as KEY\n"
        "  -i, --ignore-version    skip the version-check round (default)\n"
        "      --check-version     run the version-check round (legacy)\n"
        "      --bnetd             include BNET\\\\acct\\\\* fields\n"
        "      --fsgs              include FSGS\\\\Created field\n"
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
                std::fprintf(stderr, "%s: %.*s requires a value\n",
                    argv[0], static_cast<int>(flag.size()), flag.data());
                usage(argv[0]);
            }
            return argv[++i];
        };
        if (a == "-h" || a == "--help" || a == "--usage") {
            usage(argv[0]);
        } else if (a == "-v" || a == "--version") {
            std::printf("bnstat (pvpgn v3)\n");
            std::exit(EXIT_SUCCESS);
        } else if (a == "-c") {
            o.clienttag = need_value(a);
        } else if (starts_with(a, "--client=")) {
            o.clienttag = std::string{a.substr(9)};
        } else if (a == "-p") {
            o.players.emplace_back(need_value(a));
        } else if (starts_with(a, "--player=")) {
            o.players.emplace_back(a.substr(9));
        } else if (a == "-o") {
            o.cdowner = need_value(a);
        } else if (starts_with(a, "--owner=")) {
            o.cdowner = std::string{a.substr(8)};
        } else if (a == "-k") {
            o.cdkey = need_value(a);
        } else if (starts_with(a, "--cdkey=")) {
            o.cdkey = std::string{a.substr(8)};
        } else if (a == "-i" || a == "--ignore-version") {
            o.ignoreversion = true;
        } else if (a == "--check-version") {
            o.ignoreversion = false;
        } else if (a == "--bnetd") {
            o.bnetd = true;
        } else if (a == "--fsgs") {
            o.fsgs = true;
        } else if (a == "--stdin") {
            o.read_stdin = true;
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
            std::fprintf(stderr, "%s: \"%s\" should be a positive port number\n",
                argv[0], positional[1].c_str());
            usage(argv[0]);
        }
    }
    if (o.players.empty() && !o.read_stdin) {
        std::fprintf(stderr,
            "%s: no -p PLAYER specified (and --stdin not given)\n",
            argv[0]);
        usage(argv[0]);
    }
    return o;
}

// ---- key tables -------------------------------------------------------

std::vector<std::string> keys_for(const Options& o) {
    std::vector<std::string> keys;
    if (o.bnetd) {
        keys.emplace_back("BNET\\acct\\username");
        keys.emplace_back("BNET\\acct\\userid");
        keys.emplace_back("BNET\\acct\\lastlogin_clienttag");
        keys.emplace_back("BNET\\acct\\lastlogin_connection");
        keys.emplace_back("BNET\\acct\\lastlogin_time");
        keys.emplace_back("BNET\\acct\\firstlogin_clienttag");
        keys.emplace_back("BNET\\acct\\firstlogin_connection");
        keys.emplace_back("BNET\\acct\\firstlogin_time");
    }
    if (o.fsgs) {
        keys.emplace_back("FSGS\\Created");
    }
    keys.emplace_back("profile\\sex");
    keys.emplace_back("profile\\age");
    keys.emplace_back("profile\\location");
    keys.emplace_back("profile\\description");

    auto add_record_block = [&](std::string_view tag, int slot,
                                bool with_rating) {
        std::string base = "Record\\";
        base.append(tag);
        base.push_back('\\');
        base.append(std::to_string(slot));
        base.push_back('\\');
        keys.emplace_back(base + "last game");
        keys.emplace_back(base + "last game result");
        if (with_rating) {
            keys.emplace_back(base + "rating");
            keys.emplace_back(base + "active rating");
            keys.emplace_back(base + "high rating");
            keys.emplace_back(base + "rank");
            keys.emplace_back(base + "active rank");
            keys.emplace_back(base + "high rank");
        }
        keys.emplace_back(base + "wins");
        keys.emplace_back(base + "losses");
        keys.emplace_back(base + "disconnects");
        keys.emplace_back(base + "draws");
    };

    if (o.clienttag == "STAR") {
        add_record_block("STAR", 0, false);
        add_record_block("STAR", 1, true);
    } else if (o.clienttag == "SEXP") {
        add_record_block("SEXP", 0, false);
        add_record_block("SEXP", 1, true);
    } else if (o.clienttag == "SSHR") {
        add_record_block("SSHR", 0, false);
    } else if (o.clienttag == "W2BN") {
        add_record_block("W2BN", 0, false);
        add_record_block("W2BN", 1, true);
        add_record_block("W2BN", 3, true);
    } else if ((o.clienttag == "DRTL" || o.clienttag == "DSHR") && o.bnetd) {
        keys.emplace_back("BNET\\Record\\DRTL\\0\\level");
        keys.emplace_back("BNET\\Record\\DRTL\\0\\class");
        keys.emplace_back("BNET\\Record\\DRTL\\0\\strength");
        keys.emplace_back("BNET\\Record\\DRTL\\0\\magic");
        keys.emplace_back("BNET\\Record\\DRTL\\0\\dexterity");
        keys.emplace_back("BNET\\Record\\DRTL\\0\\vitality");
        keys.emplace_back("BNET\\Record\\DRTL\\0\\gold");
        keys.emplace_back("BNET\\Record\\DRTL\\0\\diablo kills");
    }
    return keys;
}

// ---- one stat round ---------------------------------------------------

bool query_player(net::socket_t sd,
                  const std::string& player,
                  const std::vector<std::string>& keys) {
    proto::Packet pkt;
    pkt.set_bnet_type(bnet::packet_id::CLIENT_STATSREQ);
    pkt.set_bnet_size(static_cast<std::uint16_t>(
        proto::kBnetHeaderSize + sizeof(bnet::CClientStatsReq)));
    auto* body = pkt.body_as<bnet::CClientStatsReq>();
    proto::int_set(body->name_count, 1);
    proto::int_set(body->key_count, static_cast<std::uint32_t>(keys.size()));
    proto::int_set(body->requestid, bnet::kStatsReqRequestId);

    if (!pkt.append_cstr(player.c_str())) {
        std::fprintf(stderr, "STATSREQ: player name too long\n");
        return false;
    }
    for (const auto& k : keys) {
        if (!pkt.append_cstr(k.c_str())) {
            std::fprintf(stderr, "STATSREQ: too many keys\n");
            return false;
        }
    }
    pkt.set_bnet_size(static_cast<std::uint16_t>(pkt.size()));
    if (!proto::send_bnet(sd, pkt)) {
        std::fprintf(stderr, "STATSREQ: send failed\n");
        return false;
    }

    // Drain until SERVER_STATSREPLY.
    proto::Packet rpkt;
    for (;;) {
        if (!proto::recv_bnet(sd, rpkt)) {
            std::fprintf(stderr, "STATSREPLY: server closed connection\n");
            return false;
        }
        if (rpkt.bnet_type() == bnet::packet_id::SERVER_STATSREPLY) {
            break;
        }
    }

    const auto* rep = rpkt.body_as<bnet::SServerStatsReply>();
    const std::uint32_t name_count = proto::int_get(rep->name_count);
    const std::uint32_t key_count  = proto::int_get(rep->key_count);

    std::printf("---- %s ----\n", player.c_str());
    if (name_count == 0) {
        std::printf("(no such account)\n");
        return true;
    }

    // Walk the NUL-terminated value strings that follow the body.
    const std::size_t total = rpkt.size();
    std::size_t pos = proto::kBnetHeaderSize + sizeof(bnet::SServerStatsReply);
    for (std::uint32_t i = 0; i < key_count; ++i) {
        if (pos >= total) {
            std::fprintf(stderr,
                "STATSREPLY: truncated at key %u of %u\n", i, key_count);
            return false;
        }
        const char* val = reinterpret_cast<const char*>(rpkt.data() + pos);
        // Find the NUL terminator within bounds.
        std::size_t end = pos;
        while (end < total && rpkt.data()[end] != 0) {
            ++end;
        }
        if (end >= total) {
            std::fprintf(stderr,
                "STATSREPLY: unterminated value at key %u\n", i);
            return false;
        }
        const std::string label =
            (i < keys.size()) ? keys[i] : std::string{"(extra)"};
        std::printf("%-40s = %s\n", label.c_str(), val);
        pos = end + 1;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Options opts = parse_args(argc, argv);

    login::Config cfg;
    cfg.server        = opts.host;
    cfg.port          = opts.port;
    cfg.clienttag     = opts.clienttag;
    cfg.cdowner       = opts.cdowner;
    cfg.cdkey         = opts.cdkey;
    cfg.ignoreversion = opts.ignoreversion;
    cfg.send_cdkey2   = (opts.clienttag == "STAR" ||
                        opts.clienttag == "SEXP" ||
                        opts.clienttag == "W2BN");

    login::Session ses{cfg};
    login::Result  res;
    if (!ses.run(res)) {
        std::fprintf(stderr,
            "%s: handshake failed: %s\n", argv[0], ses.error().c_str());
        return EXIT_FAILURE;
    }
    std::fprintf(stderr,
        "%s: connected to %s:%u (sessionkey=0x%08x sessionnum=0x%08x)\n",
        argv[0], opts.host.c_str(), opts.port,
        res.sessionkey, res.sessionnum);

    net::socket_holder sock{res.sock};

    const auto keys = keys_for(opts);

    int rc = EXIT_SUCCESS;
    for (const auto& player : opts.players) {
        if (!query_player(sock.get(), player, keys)) {
            rc = EXIT_FAILURE;
            break;
        }
    }
    if (opts.read_stdin) {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }
            if (!query_player(sock.get(), line, keys)) {
                rc = EXIT_FAILURE;
                break;
            }
        }
    }
    return rc;
}
