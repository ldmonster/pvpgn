// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/bench/micro/micro_main.cpp -- Plan 13 microbenchmark suite.
//
// A standalone benchmark executable (EXCLUDE_FROM_ALL; never run by ctest).
// Build + run via scripts/dev/run-bench.sh micro. Each case measures one
// hot-path operation; see microbench.hpp for the median+MAD methodology.

#include <array>
#include <iostream>
#include <string>
#include <string_view>

#include "microbench.hpp"

#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages/messages_chat.hpp"
#include "protocol/bnet/messages/messages_common.hpp"
#include "protocol/common/packet.hpp"
#include "protocol/common/writer.hpp"

#include "infra/scripting/plugin/capability.hpp"

using namespace pvpgn;

int main(int argc, char** argv) {
    std::string json_path;
    std::string git_rev = "unknown";
    std::string host    = "unknown";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string {
            return (i + 1 < argc) ? std::string(argv[++i]) : std::string();
        };
        if (a == "--json") json_path = next();
        else if (a == "--git-rev") git_rev = next();
        else if (a == "--host") host = next();
    }

    bench::Microbench mb;

    // 1. bnet_codec_roundtrip — Ping (fixed-size, no heap): encode → frame →
    //    decode through the real wire path.
    mb.run("bnet_codec_roundtrip/ping", 200000, [] {
        protocol::Writer w;
        const protocol::bnet::Ping in{0xDEADBEEFu};
        (void)protocol::bnet::encode(w, in);
        auto fp = protocol::parse_packet(w.view());
        if (fp.has_value()) {
            auto r = protocol::bnet::decode_client(fp.value().packet);
            bench::do_not_optimize(r);
        }
    });

    // 2. bnet_codec_roundtrip — JoinChannel (carries a string payload).
    mb.run("bnet_codec_roundtrip/joinchannel", 100000, [] {
        protocol::Writer w;
        const protocol::bnet::JoinChannel in{1u, "PvPGN Lobby"};
        (void)protocol::bnet::encode(w, in);
        auto fp = protocol::parse_packet(w.view());
        if (fp.has_value()) {
            auto r = protocol::bnet::decode_client(fp.value().packet);
            bench::do_not_optimize(r);
        }
    });

    // 3. tag_table_lookup — the Plan 09 capability flat-map (sorted constexpr
    //    array + binary search). One "op" resolves all 6 tokens below.
    static constexpr std::array<std::string_view, 6> kToks = {
        "chat.send",    "db.read",      "events.subscribe",
        "moderation.ban", "store.write", "admin.shutdown"};
    mb.run("tag_table_lookup/parse_capability_x6", 500000, [&] {
        for (const auto t : kToks) {
            auto c = infra::scripting::parse_capability(t);
            bench::do_not_optimize(c);
        }
    });

    mb.print_table(std::cout);
    if (!json_path.empty()) {
        mb.write_json(json_path, git_rev, host);
        std::cout << "  wrote " << json_path << "\n";
    }
    return 0;
}
