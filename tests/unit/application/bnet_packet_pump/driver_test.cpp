// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "application/bnet_packet_pump/driver.hpp"
#include "protocol/bnet/init_wire_types.hpp"

#include <array>
#include <cstddef>

namespace pump = pvpgn::application::bnet_packet_pump;
namespace init = pvpgn::protocol::bnet::init;

namespace {
constexpr std::array<std::byte, 1> one(std::uint8_t v) {
    return { static_cast<std::byte>(v) };
}
}  // namespace

TEST_CASE("driver: fresh driver is kAwaitingInit", "[application][bnet_packet_pump][driver]")
{
    pump::PacketPumpDriver d{};
    CHECK(d.state() == pump::Lifecycle::kAwaitingInit);
    CHECK(d.class_now() == pump::ConnClass::kNone);
    CHECK_FALSE(d.is_open());
    CHECK_FALSE(d.is_closed());
    CHECK_FALSE(d.is_rejected());
}

TEST_CASE("driver: feed(kClassBnet) accepts and opens", "[application][bnet_packet_pump][driver]")
{
    pump::PacketPumpDriver d{};
    auto const buf = one(init::kClassBnet);
    auto const o   = d.feed(buf);
    CHECK(o == pump::FeedOutcome::kAccepted);
    CHECK(d.is_open());
    CHECK(d.state() == pump::Lifecycle::kDispatching);
    CHECK(d.class_now() == pump::ConnClass::kBnet);
}

TEST_CASE("driver: feed(unknown cclass) rejects", "[application][bnet_packet_pump][driver]")
{
    pump::PacketPumpDriver d{};
    auto const buf = one(0x42);
    auto const o   = d.feed(buf);
    CHECK(o == pump::FeedOutcome::kRejected);
    CHECK(d.is_rejected());
    CHECK(d.state() == pump::Lifecycle::kRejected);
    CHECK(d.class_now() == pump::ConnClass::kNone);
}

TEST_CASE("driver: feed(empty) is malformed and preserves state",
          "[application][bnet_packet_pump][driver]")
{
    pump::PacketPumpDriver d{};
    std::array<std::byte, 0> empty{};
    auto const o = d.feed(empty);
    CHECK(o == pump::FeedOutcome::kMalformed);
    CHECK(d.state() == pump::Lifecycle::kAwaitingInit);  // unchanged
}

TEST_CASE("driver: feed(2 bytes) is malformed and preserves state",
          "[application][bnet_packet_pump][driver]")
{
    pump::PacketPumpDriver d{};
    std::array<std::byte, 2> two{ std::byte{0x01}, std::byte{0x02} };
    auto const o = d.feed(two);
    CHECK(o == pump::FeedOutcome::kMalformed);
    CHECK(d.state() == pump::Lifecycle::kAwaitingInit);
}

TEST_CASE("driver: after open, further feeds return kAlreadyOpen",
          "[application][bnet_packet_pump][driver]")
{
    pump::PacketPumpDriver d{};
    REQUIRE(d.feed(one(init::kClassBnet)) == pump::FeedOutcome::kAccepted);

    std::array<std::byte, 8> blob{};
    CHECK(d.feed(blob) == pump::FeedOutcome::kAlreadyOpen);
    CHECK(d.feed(one(init::kClassBot)) == pump::FeedOutcome::kAlreadyOpen);
    CHECK(d.is_open());
    CHECK(d.class_now() == pump::ConnClass::kBnet);  // class not changed by post-open feeds
}

TEST_CASE("driver: after reject, further feeds return kClosed",
          "[application][bnet_packet_pump][driver]")
{
    pump::PacketPumpDriver d{};
    REQUIRE(d.feed(one(0x42)) == pump::FeedOutcome::kRejected);

    CHECK(d.feed(one(init::kClassBnet)) == pump::FeedOutcome::kClosed);
    CHECK(d.is_rejected());  // state stays kRejected (terminal)
}

TEST_CASE("driver: close() is idempotent and terminal",
          "[application][bnet_packet_pump][driver]")
{
    pump::PacketPumpDriver d{};
    d.close();
    CHECK(d.is_closed());
    d.close();
    CHECK(d.is_closed());
    CHECK(d.feed(one(init::kClassBnet)) == pump::FeedOutcome::kClosed);
}

TEST_CASE("driver: every documented cclass opens with the right class",
          "[application][bnet_packet_pump][driver]")
{
    struct Pair { std::uint8_t cclass; pump::ConnClass expected; };
    Pair const cases[] = {
        { init::kClassBnet,         pump::ConnClass::kBnet       },
        { init::kClassFile,         pump::ConnClass::kFile       },
        { init::kClassBot,          pump::ConnClass::kBot        },
        { init::kClassEnc,          pump::ConnClass::kBnet       },
        { init::kClassTelnet,       pump::ConnClass::kTelnet     },
        { init::kClassD2gs,         pump::ConnClass::kD2cs       },
        { init::kClassD2csBnetd,    pump::ConnClass::kD2csBnetd  },
        { init::kClassLocalMachine, pump::ConnClass::kBnet       },
    };
    for (auto const& tc : cases) {
        pump::PacketPumpDriver d{};
        REQUIRE(d.feed(one(tc.cclass)) == pump::FeedOutcome::kAccepted);
        CHECK(d.class_now() == tc.expected);
        CHECK(d.is_open());
    }
}

TEST_CASE("driver: FeedOutcome::to_string covers every enumerator",
          "[application][bnet_packet_pump][driver]")
{
    CHECK(pump::to_string(pump::FeedOutcome::kAccepted    ) == "accepted");
    CHECK(pump::to_string(pump::FeedOutcome::kRejected    ) == "rejected");
    CHECK(pump::to_string(pump::FeedOutcome::kAlreadyOpen ) == "already_open");
    CHECK(pump::to_string(pump::FeedOutcome::kMalformed   ) == "malformed");
    CHECK(pump::to_string(pump::FeedOutcome::kClosed      ) == "closed");
    CHECK(pump::to_string(pump::FeedOutcome::kRateLimited ) == "rate_limited");
    CHECK(pump::to_string(pump::FeedOutcome::kD2csIpDenied) == "d2cs_ip_denied");
}

// ---- R182.a: policy-aware feed overload ----

TEST_CASE("driver(policy): default PumpPolicy preserves byte-only semantics",
          "[application][bnet_packet_pump][driver][policy]")
{
    pump::PacketPumpDriver d{};
    pump::PumpPolicy pol{};  // rate-limit disabled, d2cs_ip_allowed=true
    auto const o = d.feed(one(init::kClassBnet), pol);
    CHECK(o == pump::FeedOutcome::kAccepted);
    CHECK(d.is_open());
    CHECK(d.class_now() == pump::ConnClass::kBnet);
}

TEST_CASE("driver(policy): rate-limit exceeded -> kRateLimited + state goes to kRejected",
          "[application][bnet_packet_pump][driver][policy]")
{
    pump::PacketPumpDriver d{};
    pump::PumpPolicy pol{};
    pol.conn_count       = 10;
    pol.max_conns_per_ip = 5;
    auto const o = d.feed(one(init::kClassBnet), pol);
    CHECK(o == pump::FeedOutcome::kRateLimited);
    CHECK(d.is_rejected());
    CHECK(d.class_now() == pump::ConnClass::kNone);
    // Driver is terminal -- further feeds return kClosed.
    CHECK(d.feed(one(init::kClassBnet)) == pump::FeedOutcome::kClosed);
}

TEST_CASE("driver(policy): rate-limit DOES NOT apply to kClassD2csBnetd",
          "[application][bnet_packet_pump][driver][policy]")
{
    pump::PacketPumpDriver d{};
    pump::PumpPolicy pol{};
    pol.conn_count       = 100;
    pol.max_conns_per_ip = 5;
    pol.d2cs_ip_allowed  = true;
    auto const o = d.feed(one(init::kClassD2csBnetd), pol);
    CHECK(o == pump::FeedOutcome::kAccepted);
    CHECK(d.class_now() == pump::ConnClass::kD2csBnetd);
}

TEST_CASE("driver(policy): D2CS_BNETD client with denied IP -> kD2csIpDenied",
          "[application][bnet_packet_pump][driver][policy]")
{
    pump::PacketPumpDriver d{};
    pump::PumpPolicy pol{};
    pol.d2cs_ip_allowed = false;
    auto const o = d.feed(one(init::kClassD2csBnetd), pol);
    CHECK(o == pump::FeedOutcome::kD2csIpDenied);
    CHECK(d.is_rejected());
}

TEST_CASE("driver(policy): d2cs_ip_allowed=false has no effect on non-D2CS classes",
          "[application][bnet_packet_pump][driver][policy]")
{
    pump::PacketPumpDriver d{};
    pump::PumpPolicy pol{};
    pol.d2cs_ip_allowed = false;
    auto const o = d.feed(one(init::kClassBnet), pol);
    CHECK(o == pump::FeedOutcome::kAccepted);
    CHECK(d.class_now() == pump::ConnClass::kBnet);
}

TEST_CASE("driver(policy): rate-limit equal-to-max is allowed (strict >)",
          "[application][bnet_packet_pump][driver][policy]")
{
    pump::PacketPumpDriver d{};
    pump::PumpPolicy pol{};
    pol.conn_count       = 5;
    pol.max_conns_per_ip = 5;
    auto const o = d.feed(one(init::kClassBnet), pol);
    CHECK(o == pump::FeedOutcome::kAccepted);
}

TEST_CASE("driver(policy): max_conns_per_ip=0 disables the rate-limit gate",
          "[application][bnet_packet_pump][driver][policy]")
{
    pump::PacketPumpDriver d{};
    pump::PumpPolicy pol{};
    pol.conn_count       = 9999;
    pol.max_conns_per_ip = 0;
    auto const o = d.feed(one(init::kClassBnet), pol);
    CHECK(o == pump::FeedOutcome::kAccepted);
}
