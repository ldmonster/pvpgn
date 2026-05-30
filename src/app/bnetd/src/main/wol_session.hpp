// SPDX-License-Identifier: GPL-2.0-or-later
// main/wol_session.hpp — WOL session factory declaration.
//
// Creates a WolFsm-backed session for each new TCP connection on the WOL
// port (default 4000).
#pragma once

#include <memory>

#include "infra/net/tcp_session.hpp"
#include "app/bnetd/server_config.hpp"

namespace pvpgn::app::bnetd {

/// Wire up a WolFsm for the given TCP session and start it.
void make_wol_session(
    std::shared_ptr<infra::net::TcpSession> tcp,
    const ServerConfig&                      cfg);

} // namespace pvpgn::app::bnetd
