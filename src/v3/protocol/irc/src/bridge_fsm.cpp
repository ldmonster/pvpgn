// SPDX-License-Identifier: GPL-2.0-or-later

#include "protocol/irc/bridge_fsm.hpp"

namespace pvpgn::protocol::irc {

std::string IrcBridgeFsm::irc_to_bnet_channel(std::string_view irc_channel) {
    // Strip leading '#' if present
    if (!irc_channel.empty() && irc_channel[0] == '#') {
        return std::string{irc_channel.substr(1)};
    }
    return std::string{irc_channel};
}

std::string IrcBridgeFsm::bnet_to_irc_channel(std::string_view bnet_channel) {
    return "#" + std::string{bnet_channel};
}

}  // namespace pvpgn::protocol::irc
