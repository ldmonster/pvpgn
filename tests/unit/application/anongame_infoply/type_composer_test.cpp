// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/anongame_infoply/type_composer.hpp"

using pvpgn::application::anongame_infoply::compose_type_payload;
using pvpgn::application::anongame_infoply::kAnonGameDefaultPrefix;
using pvpgn::application::anongame_infoply::kAnonGameQueueCount;

namespace pb = pvpgn::protocol::bnet;

namespace {

// Build an empty 18-slot per-queue index table.
std::array<std::vector<std::uint8_t>, kAnonGameQueueCount>
make_empty_queues() {
    return {};
}

}  // namespace

TEST_CASE("type_composer: empty input -> empty payload",
          "[application][anongame_infoply][type]") {
    auto q = make_empty_queues();
    auto out = compose_type_payload(q);
    REQUIRE(out.sections.empty());
}

TEST_CASE("type_composer: only PG queues -> single PG section",
          "[application][anongame_infoply][type]") {
    auto q = make_empty_queues();
    q[0] = {0, 1, 2};   // PG 1v1, three maps
    q[1] = {3};         // PG 2v2, one map
    auto out = compose_type_payload(q);
    REQUIRE(out.sections.size() == 1);
    REQUIRE(out.sections[0].section_id == 0x00);
    REQUIRE(out.sections[0].gamestyles.size() == 2);
    REQUIRE(out.sections[0].gamestyles[0].prefix ==
            kAnonGameDefaultPrefix[0]);
    REQUIRE(out.sections[0].gamestyles[0].map_indices ==
            (std::vector<std::uint8_t>{0, 1, 2}));
    REQUIRE(out.sections[0].gamestyles[1].prefix ==
            kAnonGameDefaultPrefix[1]);
}

TEST_CASE("type_composer: only AT queues -> single AT section",
          "[application][anongame_infoply][type]") {
    auto q = make_empty_queues();
    q[5] = {0};   // AT 2v2
    q[8] = {1};   // AT 4v4
    auto out = compose_type_payload(q);
    REQUIRE(out.sections.size() == 1);
    REQUIRE(out.sections[0].section_id == 0x01);
    REQUIRE(out.sections[0].gamestyles.size() == 2);
}

TEST_CASE("type_composer: only TY queue -> single TY section",
          "[application][anongame_infoply][type]") {
    auto q = make_empty_queues();
    q[9] = {0};
    auto out = compose_type_payload(q);
    REQUIRE(out.sections.size() == 1);
    REQUIRE(out.sections[0].section_id == 0x02);
    REQUIRE(out.sections[0].gamestyles.size() == 1);
}

TEST_CASE("type_composer: PG + AT + TY ordered as 0x00, 0x01, 0x02",
          "[application][anongame_infoply][type]") {
    auto q = make_empty_queues();
    q[0]  = {0};   // PG
    q[5]  = {1};   // AT
    q[9]  = {2};   // TY
    auto out = compose_type_payload(q);
    REQUIRE(out.sections.size() == 3);
    REQUIRE(out.sections[0].section_id == 0x00);
    REQUIRE(out.sections[1].section_id == 0x01);
    REQUIRE(out.sections[2].section_id == 0x02);
}

TEST_CASE("type_composer: queues with no maps are skipped",
          "[application][anongame_infoply][type]") {
    auto q = make_empty_queues();
    q[0] = {};         // PG 1v1 — no maps
    q[1] = {0};        // PG 2v2 — has maps
    auto out = compose_type_payload(q);
    REQUIRE(out.sections.size() == 1);
    REQUIRE(out.sections[0].gamestyles.size() == 1);
    REQUIRE(out.sections[0].gamestyles[0].prefix ==
            kAnonGameDefaultPrefix[1]);
}

TEST_CASE("type_composer: wrong-size span yields empty payload",
          "[application][anongame_infoply][type]") {
    std::vector<std::vector<std::uint8_t>> short_q(5);
    auto out = compose_type_payload(short_q);
    REQUIRE(out.sections.empty());
}

TEST_CASE("type_composer: prefix override applied per-row",
          "[application][anongame_infoply][type]") {
    auto q = make_empty_queues();
    q[0] = {0};
    auto override_prefix = kAnonGameDefaultPrefix;
    override_prefix[0][2] = 0x07;  // change thumbsdown for PG 1v1
    auto out = compose_type_payload(q, override_prefix);
    REQUIRE(out.sections.size() == 1);
    REQUIRE(out.sections[0].gamestyles[0].prefix[2] == 0x07);
}
