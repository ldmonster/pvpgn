// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/scripting/plugin/abi_capability_parity_test.cpp -- Plan 12.
//
// The public C ABI (`pvpgn/plugin/abi.h`) and the host's internal capability
// enforcement enum MUST agree on every capability bit value — otherwise a
// plugin declaring a capability would be granted/denied the wrong one. These
// compile-time assertions pin that parity; a drift fails the build.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "infra/scripting/plugin/capability.hpp"
#include "pvpgn/plugin/abi.h"

using pvpgn::infra::scripting::Capability;

namespace {
constexpr std::uint32_t host(Capability c) {
    return static_cast<std::uint32_t>(c);
}
}  // namespace

static_assert(PVPGN_CAP_CHAT_SEND          == host(Capability::CHAT_SEND));
static_assert(PVPGN_CAP_CHAT_EMOTE         == host(Capability::CHAT_EMOTE));
static_assert(PVPGN_CAP_DB_READ            == host(Capability::DB_READ));
static_assert(PVPGN_CAP_DB_WRITE           == host(Capability::DB_WRITE));
static_assert(PVPGN_CAP_EVENTS_SUBSCRIBE   == host(Capability::EVENTS_SUBSCRIBE));
static_assert(PVPGN_CAP_EVENTS_PUBLISH     == host(Capability::EVENTS_PUBLISH));
static_assert(PVPGN_CAP_FS_READ            == host(Capability::FS_READ));
static_assert(PVPGN_CAP_FS_WRITE           == host(Capability::FS_WRITE));
static_assert(PVPGN_CAP_NET_HTTP           == host(Capability::NET_HTTP));
static_assert(PVPGN_CAP_NET_SOCKET         == host(Capability::NET_SOCKET));
static_assert(PVPGN_CAP_COMMANDS_REGISTER  == host(Capability::COMMANDS_REGISTER));
static_assert(PVPGN_CAP_MODERATION_BAN     == host(Capability::MODERATION_BAN));
static_assert(PVPGN_CAP_MODERATION_KICK    == host(Capability::MODERATION_KICK));
static_assert(PVPGN_CAP_STORE_READ         == host(Capability::STORE_READ));
static_assert(PVPGN_CAP_STORE_WRITE        == host(Capability::STORE_WRITE));
static_assert(PVPGN_CAP_ADMIN_RELOAD_CONFIG == host(Capability::ADMIN_RELOAD_CONFIG));
static_assert(PVPGN_CAP_ADMIN_SHUTDOWN     == host(Capability::ADMIN_SHUTDOWN));

TEST_CASE("plugin ABI: public capability bits match the host enum",
          "[infra][plugin][abi]") {
    // All parity is checked at compile time above; this also pins the version.
    CHECK(PVPGN_PLUGIN_ABI_VERSION == 1);
}
