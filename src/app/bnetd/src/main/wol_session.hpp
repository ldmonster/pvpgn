// SPDX-License-Identifier: GPL-2.0-or-later
// main/wol_session.hpp — WOL session factory declaration.
//
// Creates a WolFsm-backed session for each new TCP connection on the WOL
// port (default 4000).
#pragma once

#include <memory>

#include "infra/net/tcp_session.hpp"
#include "app/bnetd/server_config.hpp"
#include "protocol/wol/wol_fsm.hpp"

namespace pvpgn::app::bnetd {

/// Wire up a WolFsm for the given TCP session and start it.
/// @param auth  Native Westwood Online auth collaborators (CVERS/APGAR flow).
///              When incomplete the FSM falls back to the legacy IRC path.
void make_wol_session(
    std::shared_ptr<infra::net::TcpSession> tcp,
    const ServerConfig&                      cfg,
    protocol::wol::WolAuthDeps               auth);

} // namespace pvpgn::app::bnetd
