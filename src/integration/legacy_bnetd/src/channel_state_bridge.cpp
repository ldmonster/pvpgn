// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridges for SID_JOINCHANNEL / SID_LEAVECHANNEL. The
// legacy `_client_joinchannel` / `_client_leavechannel` handlers
// emit no direct server reply (the user-visible JOIN/LEAVE goes out
// as a SID_CHATEVENT and is owned by the chatevent compose bridge);
// these bridges therefore only structured-log the intent. They
// always return 0 so the legacy state machine retains full
// authority over the side-effects (channel membership, kick/ban
// flags, etc).

#include "integration/legacy_bnetd/channel_state_bridge.hpp"

#include <cstdint>
#include <string>
#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

namespace {

const char* join_flag_name(unsigned int flag) noexcept {
    switch (flag) {
        case 0: return "NORMAL";
        case 1: return "GENERIC";
        case 2: return "CREATE";
        default: return "?";
    }
}

}  // namespace

extern "C" int pvpgn_v3_joinchannel(void* conn_ptr,
                                        char const* channel_name,
                                        unsigned int flag) {
    if (conn_ptr == nullptr) return 0;

    std::string_view name = (channel_name != nullptr)
                                ? std::string_view{channel_name}
                                : std::string_view{};

    const std::string flag_str = std::to_string(flag);
    const pvpgn::core::ILogger::Field fields[] = {
        {"channel", name},
        {"flag",    std::string_view{join_flag_name(flag)}},
        {"raw",     std::string_view{flag_str}},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_channel_state_bridge",
        "joinchannel intent observed",
        {fields[0], fields[1], fields[2]});

    return 0;
}

extern "C" int pvpgn_v3_leavechannel(void* conn_ptr) {
    if (conn_ptr == nullptr) return 0;

    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_channel_state_bridge",
        "leavechannel intent observed",
        {});

    return 0;
}
