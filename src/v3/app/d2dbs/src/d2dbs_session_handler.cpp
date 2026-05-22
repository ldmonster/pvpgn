// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/d2dbs/d2dbs_session_handler.hpp"

#include <string>

#include "core/result.hpp"
#include "domain/d2dbs/types.hpp"
#include "domain/d2dbs/use_cases.hpp"
#include "protocol/d2dbs/fsm.hpp"

namespace pvpgn::app::d2dbs {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

D2DBSSessionHandler::D2DBSSessionHandler(
    domain::d2dbs::ICharacterSaveRepository& save_repo,
    domain::d2dbs::ID2DBSLadderRepository&   ladder_repo,
    ID2DBSSessionEgress&                     egress) noexcept
    : save_repo_(save_repo)
    , ladder_repo_(ladder_repo)
    , egress_(egress)
{}

// ---------------------------------------------------------------------------
// Callback factory
// ---------------------------------------------------------------------------

protocol::d2dbs::D2DBSFsmCallbacks D2DBSSessionHandler::make_callbacks() {
    protocol::d2dbs::D2DBSFsmCallbacks cb;

    cb.on_char_save = [this](const protocol::d2dbs::D2DBSCharSaveData& req)
        -> core::Result<void, core::Error> {
        return handle_char_save(req);
    };

    cb.on_char_load = [this](const protocol::d2dbs::D2DBSCharLoadData& req)
        -> core::Result<void, core::Error> {
        return handle_char_load(req);
    };

    cb.on_char_ladder = [this](const protocol::d2dbs::D2DBSCharLadderData& req)
        -> core::Result<void, core::Error> {
        return handle_char_ladder(req);
    };

    cb.on_char_lock = [this](const protocol::d2dbs::D2DBSCharLockReq& req)
        -> core::Result<void, core::Error> {
        return handle_char_lock(req);
    };

    cb.on_echo_reply = [this](const protocol::d2dbs::D2DBSEchoReply& req)
        -> core::Result<void, core::Error> {
        return handle_echo_reply(req);
    };

    cb.on_disconnect = []() {
        // No-op: connection teardown is handled by the TCP layer.
    };

    return cb;
}

// ---------------------------------------------------------------------------
// Callback implementations
// ---------------------------------------------------------------------------

core::Result<void, core::Error> D2DBSSessionHandler::handle_char_save(
    const protocol::d2dbs::D2DBSCharSaveData& req)
{
    // Build domain save data from the wire request.
    domain::d2dbs::CharacterSaveData save_data;
    save_data.account_name = req.account_name;
    save_data.char_name    = req.char_name;
    save_data.realm_name   = req.realm_name;
    save_data.data         = req.data;
    save_data.timestamp    = 0;  // timestamp set by persistence layer

    domain::d2dbs::CharacterSaveUseCase uc{save_repo_};
    const bool ok = uc.execute(save_data);
    egress_.send_char_save_result(ok);
    return {};
}

core::Result<void, core::Error> D2DBSSessionHandler::handle_char_load(
    const protocol::d2dbs::D2DBSCharLoadData& req)
{
    domain::d2dbs::CharacterLoadUseCase uc{save_repo_};
    auto result = uc.execute(req.account_name, req.char_name);

    if (result.has_value()) {
        egress_.send_char_load_result(true, &result.value());
    } else {
        egress_.send_char_load_result(false, nullptr);
    }
    return {};
}

core::Result<void, core::Error> D2DBSSessionHandler::handle_char_ladder(
    const protocol::d2dbs::D2DBSCharLadderData& req)
{
    // Combine the 32-bit hi/lo experience fields into a single 64-bit value.
    const uint64_t experience =
        (static_cast<uint64_t>(req.charexphigh) << 32) |
        static_cast<uint64_t>(req.charexplow);

    domain::d2dbs::LadderUpdateEntry entry;
    entry.char_name    = req.char_name;
    entry.account_name = {};  // D2GS does not send account in UPDATE_LADDER
    entry.experience   = experience;
    entry.level        = static_cast<uint8_t>(req.charlevel);
    entry.char_class   = static_cast<uint8_t>(req.charclass);
    entry.flags        = static_cast<uint32_t>(req.charstatus);

    domain::d2dbs::LadderUpdateUseCase uc{ladder_repo_};
    const bool ok = uc.execute(entry);
    egress_.send_ladder_update_result(ok);
    return {};
}

core::Result<void, core::Error> D2DBSSessionHandler::handle_char_lock(
    const protocol::d2dbs::D2DBSCharLockReq& req)
{
    if (req.lockstatus != 0) {
        // Lock request — character entering a game.
        domain::d2dbs::CharacterLockUseCase uc{save_repo_};
        const bool ok = uc.execute(req.account_name, req.char_name);
        egress_.send_char_login_result(ok);
    } else {
        // Unlock request — character leaving a game.
        domain::d2dbs::CharacterUnlockUseCase uc{save_repo_};
        const bool ok = uc.execute(req.account_name, req.char_name);
        egress_.send_char_logout_result(ok);
    }
    return {};
}

core::Result<void, core::Error> D2DBSSessionHandler::handle_echo_reply(
    const protocol::d2dbs::D2DBSEchoReply& /*req*/)
{
    // Keepalive pong from D2GS — no domain action required.
    // The TCP layer resets its keepalive timer separately.
    return {};
}

} // namespace pvpgn::app::d2dbs
