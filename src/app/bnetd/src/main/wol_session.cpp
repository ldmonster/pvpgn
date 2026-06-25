// SPDX-License-Identifier: GPL-2.0-or-later
// main/wol_session.cpp — WOL session factory implementation.

#include "main/wol_session.hpp"

#include <memory>
#include <span>

#include <boost/system/error_code.hpp>

#include "core/bytes.hpp"
#include "protocol/wol/wol_fsm.hpp"
#include "app/bnetd/tcp_session.hpp"

namespace pvpgn::app::bnetd {

void make_wol_session(
    std::shared_ptr<infra::net::TcpSession> tcp,
    const ServerConfig&                      cfg,
    protocol::wol::WolAuthDeps               auth) {

    auto egress = std::make_shared<TcpSessionEgress>(tcp);
    auto ctx    = std::make_shared<WolEgressContext>(egress, cfg.server_name);
    auto fsm    = std::make_shared<protocol::wol::WolFsm>(ctx, auth);

    tcp->set_on_bytes([fsm](core::ByteView bv) {
        auto sp = std::span<const std::byte>(bv.data(), bv.size());
        (void)fsm->on_bytes(sp);
    });

    tcp->set_on_close([fsm](const boost::system::error_code&) {
        fsm->on_close();
    });

    tcp->start();
}

} // namespace pvpgn::app::bnetd
