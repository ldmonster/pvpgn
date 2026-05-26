// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "application/bnet_packet_pump/lifecycle.hpp"
#include "protocol/bnet/init_wire_types.hpp"

namespace pump = pvpgn::application::bnet_packet_pump;
namespace init = pvpgn::protocol::bnet::init;

TEST_CASE("lifecycle: kAwaitingInit + kClassBnet -> kDispatching/kBnet",
          "[application][bnet_packet_pump][lifecycle]")
{
    auto const step = pump::step_on_cclass_byte(pump::Lifecycle::kAwaitingInit, init::kClassBnet);
    CHECK(step.next_state == pump::Lifecycle::kDispatching);
    CHECK(step.class_now  == pump::ConnClass::kBnet);
}

TEST_CASE("lifecycle: every documented cclass maps deterministically",
          "[application][bnet_packet_pump][lifecycle]")
{
    auto step = [](std::uint8_t b) { return pump::step_on_cclass_byte(pump::Lifecycle::kAwaitingInit, b); };

    CHECK(step(init::kClassBnet        ).class_now == pump::ConnClass::kBnet);
    CHECK(step(init::kClassFile        ).class_now == pump::ConnClass::kFile);
    CHECK(step(init::kClassBot         ).class_now == pump::ConnClass::kBot);
    CHECK(step(init::kClassEnc         ).class_now == pump::ConnClass::kBnet);   // legacy treats Enc as Bnet
    CHECK(step(init::kClassTelnet      ).class_now == pump::ConnClass::kTelnet);
    CHECK(step(init::kClassD2gs        ).class_now == pump::ConnClass::kD2cs);
    CHECK(step(init::kClassD2csBnetd   ).class_now == pump::ConnClass::kD2csBnetd);
    CHECK(step(init::kClassLocalMachine).class_now == pump::ConnClass::kBnet);
}

TEST_CASE("lifecycle: unknown cclass byte -> kRejected (no class)",
          "[application][bnet_packet_pump][lifecycle]")
{
    auto const step = pump::step_on_cclass_byte(pump::Lifecycle::kAwaitingInit, 0x42);
    CHECK(step.next_state == pump::Lifecycle::kRejected);
    CHECK(step.class_now  == pump::ConnClass::kNone);
}

TEST_CASE("lifecycle: ALL unknown bytes route to kRejected",
          "[application][bnet_packet_pump][lifecycle]")
{
    for (unsigned v = 0; v < 256u; ++v) {
        bool known = (v == init::kClassBnet       ) || (v == init::kClassFile        )
                  || (v == init::kClassBot        ) || (v == init::kClassEnc         )
                  || (v == init::kClassTelnet     ) || (v == init::kClassD2gs        )
                  || (v == init::kClassD2csBnetd  ) || (v == init::kClassLocalMachine);
        auto const step = pump::step_on_cclass_byte(pump::Lifecycle::kAwaitingInit,
                                                    static_cast<std::uint8_t>(v));
        if (known) {
            CHECK(step.next_state == pump::Lifecycle::kDispatching);
            CHECK(step.class_now  != pump::ConnClass::kNone);
        } else {
            CHECK(step.next_state == pump::Lifecycle::kRejected);
            CHECK(step.class_now  == pump::ConnClass::kNone);
        }
    }
}

TEST_CASE("lifecycle: non-AwaitingInit states are no-ops",
          "[application][bnet_packet_pump][lifecycle]")
{
    for (auto s : { pump::Lifecycle::kDispatching, pump::Lifecycle::kRejected, pump::Lifecycle::kClosed }) {
        auto const step = pump::step_on_cclass_byte(s, init::kClassBnet);
        CHECK(step.next_state == s);
        CHECK(step.class_now  == pump::ConnClass::kNone);
    }
}

TEST_CASE("lifecycle: to_string covers every enumerator",
          "[application][bnet_packet_pump][lifecycle]")
{
    CHECK(pump::to_string(pump::Lifecycle::kAwaitingInit) == "awaiting_init");
    CHECK(pump::to_string(pump::Lifecycle::kDispatching ) == "dispatching");
    CHECK(pump::to_string(pump::Lifecycle::kRejected    ) == "rejected");
    CHECK(pump::to_string(pump::Lifecycle::kClosed      ) == "closed");

    CHECK(pump::to_string(pump::ConnClass::kNone)       == "none");
    CHECK(pump::to_string(pump::ConnClass::kInit)       == "init");
    CHECK(pump::to_string(pump::ConnClass::kBnet)       == "bnet");
    CHECK(pump::to_string(pump::ConnClass::kFile)       == "file");
    CHECK(pump::to_string(pump::ConnClass::kBot)        == "bot");
    CHECK(pump::to_string(pump::ConnClass::kTelnet)     == "telnet");
    CHECK(pump::to_string(pump::ConnClass::kIrc)        == "irc");
    CHECK(pump::to_string(pump::ConnClass::kD2cs)       == "d2cs");
    CHECK(pump::to_string(pump::ConnClass::kD2csBnetd)  == "d2cs_bnetd");
    CHECK(pump::to_string(pump::ConnClass::kW3route)    == "w3route");
    CHECK(pump::to_string(pump::ConnClass::kWol)        == "wol");
    CHECK(pump::to_string(pump::ConnClass::kWolGameres) == "wolgameres");
    CHECK(pump::to_string(pump::ConnClass::kWgameres)   == "wgameres");
    CHECK(pump::to_string(pump::ConnClass::kWserv)      == "wserv");
    CHECK(pump::to_string(pump::ConnClass::kApiReg)     == "apireg");
    CHECK(pump::to_string(pump::ConnClass::kAuthReq)    == "authreq");
}
