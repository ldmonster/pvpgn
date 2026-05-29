// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/bnetd/bnet_connection_adapter.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "application/auth/login_user.hpp"
#include "application/auth/login_user_nls.hpp"
#include "application/connection/connection_fsm.hpp"
#include "core/result.hpp"
#include "domain/connection/connection_context.hpp"

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

BnetConnectionAdapter::BnetConnectionAdapter(
    domain::connection::IConnectionContext& ctx,
    std::uint32_t session_id) noexcept
    : ctx_(ctx)
    , fsm_(std::make_unique<application::connection::ConnectionFsm>(*this,
                                                                session_id))
{}

BnetConnectionAdapter::BnetConnectionAdapter(
    domain::connection::IConnectionContext&  ctx,
    application::auth::LoginUserNls&         login_user_nls,
    std::uint32_t                            session_id) noexcept
    : ctx_(ctx)
    , fsm_(std::make_unique<application::connection::ConnectionFsm>(*this,
                                                                login_user_nls,
                                                                session_id))
{}

BnetConnectionAdapter::BnetConnectionAdapter(
    domain::connection::IConnectionContext&  ctx,
    application::auth::LoginUser&            login_user_ols,
    application::auth::LoginUserNls&         login_user_nls,
    std::uint32_t                            session_id) noexcept
    : ctx_(ctx)
    , fsm_(std::make_unique<application::connection::ConnectionFsm>(*this,
                                                                login_user_ols,
                                                                login_user_nls,
                                                                session_id))
{}

// ---------------------------------------------------------------------------
// Domain dispatch entry point
// ---------------------------------------------------------------------------

core::Status<> BnetConnectionAdapter::dispatch_to_domain(
    std::uint8_t packet_id,
    std::span<const std::byte> payload) {
    return fsm_->dispatch(packet_id, payload);
}

// ---------------------------------------------------------------------------
// IConnectionContext — forwarded to the injected ctx_
// ---------------------------------------------------------------------------

core::Status<> BnetConnectionAdapter::send_packet(
    std::uint8_t packet_id,
    std::span<const std::byte> payload) {
    return ctx_.send_packet(packet_id, payload);
}

void BnetConnectionAdapter::close() {
    ctx_.close();
}

std::string BnetConnectionAdapter::get_remote_address() const {
    return ctx_.get_remote_address();
}

std::uint32_t BnetConnectionAdapter::get_session_id() const {
    return ctx_.get_session_id();
}

void BnetConnectionAdapter::on_game_created(
    std::uint32_t game_id,
    const domain::connection::GameInfo& info) {
    ctx_.on_game_created(game_id, info);
}

void BnetConnectionAdapter::on_game_joined(
    std::uint32_t game_id,
    const domain::connection::GameInfo& info) {
    ctx_.on_game_joined(game_id, info);
}

void BnetConnectionAdapter::on_game_left(std::uint32_t game_id) {
    ctx_.on_game_left(game_id);
}

}  // namespace pvpgn::app::bnetd
