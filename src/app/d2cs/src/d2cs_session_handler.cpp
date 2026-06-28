// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/d2cs/d2cs_session_handler.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "infra/crypto/bnet_hash.hpp"
#include "domain/d2cs/types.hpp"
#include "domain/d2cs/use_cases.hpp"
#include "protocol/d2cs/fsm.hpp"

namespace pvpgn::app::d2cs {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

D2CSSessionHandler::D2CSSessionHandler(
    domain::d2cs::ICharacterRepository& char_repo,
    domain::d2cs::ILadderRepository&    ladder_repo,
    ID2CSSessionEgress&                 egress,
    std::string                         realm_key) noexcept
    : char_repo_(char_repo)
    , ladder_repo_(ladder_repo)
    , egress_(egress)
    , realm_key_(std::move(realm_key))
{}

// ---------------------------------------------------------------------------
// Callback factory
// ---------------------------------------------------------------------------

protocol::d2cs::D2CSFsmCallbacks D2CSSessionHandler::make_callbacks() {
    protocol::d2cs::D2CSFsmCallbacks cb;

    cb.on_login = [this](const protocol::d2cs::D2CSLoginRequest& req)
        -> core::Result<void, core::Error> {
        return handle_login(req);
    };

    cb.on_char_login = [this](const protocol::d2cs::D2CSCharLoginRequest& req)
        -> core::Result<void, core::Error> {
        return handle_char_login(req);
    };

    cb.on_create_char = [this](const protocol::d2cs::D2CSCreateCharRequest& req)
        -> core::Result<void, core::Error> {
        return handle_create_char(req);
    };

    cb.on_delete_char = [this](const protocol::d2cs::D2CSDeleteCharRequest& req)
        -> core::Result<void, core::Error> {
        return handle_delete_char(req);
    };

    cb.on_char_list = [this](const protocol::d2cs::D2CSCharListRequest& req)
        -> core::Result<void, core::Error> {
        return handle_char_list(req);
    };

    cb.on_char_list_110 = [this](const protocol::d2cs::D2CSCharListRequest& req)
        -> core::Result<void, core::Error> {
        return handle_char_list_110(req);
    };

    cb.on_ladder = [this](const protocol::d2cs::D2CSLadderRequest& req)
        -> core::Result<void, core::Error> {
        return handle_ladder(req);
    };

    cb.on_char_ladder = [this](const protocol::d2cs::D2CSCharLadderRequest& req)
        -> core::Result<void, core::Error> {
        return handle_char_ladder(req);
    };

    cb.on_create_game = [this](const protocol::d2cs::D2CSCreateGameRequest& req)
        -> core::Result<void, core::Error> {
        return handle_create_game(req);
    };

    cb.on_join_game = [this](const protocol::d2cs::D2CSJoinGameRequest& req)
        -> core::Result<void, core::Error> {
        return handle_join_game(req);
    };

    cb.on_game_list = [this](const protocol::d2cs::D2CSGameListRequest& req)
        -> core::Result<void, core::Error> {
        return handle_game_list(req);
    };

    cb.on_game_info = [this](const protocol::d2cs::D2CSGameInfoRequest& req)
        -> core::Result<void, core::Error> {
        return handle_game_info(req);
    };

    cb.on_motd = [this](const protocol::d2cs::D2CSMotdRequest& req)
        -> core::Result<void, core::Error> {
        return handle_motd(req);
    };

    cb.on_cancel_create_game = [this]()
        -> core::Result<void, core::Error> {
        return handle_cancel_create_game();
    };

    cb.on_convert_char = [this](const protocol::d2cs::D2CSConvertCharRequest& req)
        -> core::Result<void, core::Error> {
        return handle_convert_char(req);
    };

    cb.on_disconnect = [this]() {
        handle_disconnect();
    };

    return cb;
}

// ---------------------------------------------------------------------------
// Callback implementations
// ---------------------------------------------------------------------------

core::Result<void, core::Error> D2CSSessionHandler::handle_login(
    const protocol::d2cs::D2CSLoginRequest& req)
{
    // Store the account name for subsequent callbacks in this session.
    account_name_ = req.account_name;

    // Real auth: when a realm key is configured, the LOGINREQ secret_hash must
    // equal the keyed token blizzard_hash(key ‖ account ‖ sessionnum ‖ seqno) —
    // the token a realm-join issues to a client that authenticated via bnetd.
    // A forged or tampered hash is rejected with InvalidPassword, mirroring the
    // original d2cs<->bnetd ACCOUNTLOGINREQ validation. With no key configured
    // (unit tests construct the handler without one) the check is skipped.
    if (!realm_key_.empty()) {
        std::vector<std::byte> buf;
        const auto put_str = [&](std::string_view s) {
            for (char ch : s)
                buf.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
        };
        const auto put_u32 = [&](std::uint32_t v) {
            for (int i = 0; i < 4; ++i)
                buf.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFFu));
        };
        put_str(realm_key_);
        put_str(req.account_name);
        put_u32(req.sessionnum);
        put_u32(req.seqno);
        const auto token = pvpgn::v3::infra::crypto::blizzard_hash(buf);
        if (token != req.secret_hash) {
            egress_.send_realm_logon_result(
                domain::d2cs::RealmLogonResult::InvalidPassword);
            return {};
        }
    }

    egress_.send_realm_logon_result(domain::d2cs::RealmLogonResult::Success);
    return {};
}

core::Result<void, core::Error> D2CSSessionHandler::handle_char_login(
    const protocol::d2cs::D2CSCharLoginRequest& req)
{
    // CHARLOGINREQ carries only the char name on the wire; the account is the
    // session account established at LOGINREQ.
    domain::d2cs::CharacterSelectUseCase uc{char_repo_};
    auto result = uc.execute(account_name_, req.char_name);

    if (result.has_value()) {
        egress_.send_char_select_result(true, &result.value());
    } else {
        egress_.send_char_select_result(false, nullptr);
    }
    return {};
}

core::Result<void, core::Error> D2CSSessionHandler::handle_create_char(
    const protocol::d2cs::D2CSCreateCharRequest& req)
{
    // Build a CharacterInfo from the wire request.
    // CREATECHARREQ carries chclass and status as 16-bit fields (the low byte
    // is the meaningful class/status value for the classes/flags we model).
    domain::d2cs::CharacterInfo info;
    info.name    = req.char_name;
    info.class_  = static_cast<domain::d2cs::CharacterClass>(
                       static_cast<uint8_t>(req.char_class));
    info.flags   = static_cast<domain::d2cs::CharacterFlags>(
                       static_cast<uint8_t>(req.char_status));
    info.level   = 1;
    info.experience = 0;
    info.last_played = 0;

    domain::d2cs::CharacterCreateUseCase uc{char_repo_};
    egress_.send_char_create_result(uc.execute(account_name_, info));
    return {};
}

core::Result<void, core::Error> D2CSSessionHandler::handle_delete_char(
    const protocol::d2cs::D2CSDeleteCharRequest& req)
{
    domain::d2cs::CharacterDeleteUseCase uc{char_repo_};
    const bool ok = uc.execute(account_name_, req.char_name);
    egress_.send_char_delete_result(ok);
    return {};
}

core::Result<void, core::Error> D2CSSessionHandler::handle_char_list(
    const protocol::d2cs::D2CSCharListRequest& /*req*/)
{
    domain::d2cs::CharacterListUseCase uc{char_repo_};
    auto result = uc.execute(account_name_);

    if (result.has_value()) {
        egress_.send_char_list(result.value());
    } else {
        // Repository error — send empty list with failure flag.
        egress_.send_char_list_result(false);
    }
    return {};
}

core::Result<void, core::Error> D2CSSessionHandler::handle_char_list_110(
    const protocol::d2cs::D2CSCharListRequest& req)
{
    // 1.10+ variant uses the same domain logic; the wire encoding difference
    // is handled by the egress implementation.
    return handle_char_list(req);
}

core::Result<void, core::Error> D2CSSessionHandler::handle_ladder(
    const protocol::d2cs::D2CSLadderRequest& req)
{
    // Default page size matches legacy d2cs behaviour (20 entries per page).
    static constexpr uint32_t kPageSize = 20;

    const auto type = static_cast<domain::d2cs::LadderType>(req.ladder_type);
    domain::d2cs::LadderQueryUseCase uc{ladder_repo_};
    auto result = uc.execute(type,
                             static_cast<uint32_t>(req.start_pos),
                             kPageSize);

    if (result.has_value()) {
        egress_.send_ladder(result.value());
    } else {
        egress_.send_ladder({});
    }
    return {};
}

core::Result<void, core::Error> D2CSSessionHandler::handle_char_ladder(
    const protocol::d2cs::D2CSCharLadderRequest& req)
{
    // Determine ladder type from the hardcore/expansion flags.
    // Mirrors the legacy d2cs logic in handle_d2cs.cpp.
    domain::d2cs::LadderType type;
    if (req.expansion != 0 && req.hardcore != 0) {
        type = domain::d2cs::LadderType::ExpansionHardcore;
    } else if (req.expansion != 0) {
        type = domain::d2cs::LadderType::Expansion;
    } else if (req.hardcore != 0) {
        type = domain::d2cs::LadderType::Hardcore;
    } else {
        type = domain::d2cs::LadderType::Standard;
    }

    // Look up the single character's ladder entry.
    auto entry = ladder_repo_.get_character_ladder_entry(req.char_name, type);
    if (entry.has_value()) {
        egress_.send_ladder({entry.value()});
    } else {
        egress_.send_ladder({});
    }
    return {};
}

// ---------------------------------------------------------------------------
// No-op stubs
// ---------------------------------------------------------------------------

core::Result<void, core::Error> D2CSSessionHandler::handle_create_game(
    const protocol::d2cs::D2CSCreateGameRequest& /*req*/)
{
    // Stub — CreateGame use case wired in a later round.
    return {};
}

core::Result<void, core::Error> D2CSSessionHandler::handle_join_game(
    const protocol::d2cs::D2CSJoinGameRequest& /*req*/)
{
    // Stub — JoinGame use case wired in a later round.
    return {};
}

core::Result<void, core::Error> D2CSSessionHandler::handle_game_list(
    const protocol::d2cs::D2CSGameListRequest& /*req*/)
{
    // Stub — GetGameList use case wired in a later round.
    return {};
}

core::Result<void, core::Error> D2CSSessionHandler::handle_game_info(
    const protocol::d2cs::D2CSGameInfoRequest& /*req*/)
{
    // Stub — GetGameInfo use case wired in a later round.
    return {};
}

core::Result<void, core::Error> D2CSSessionHandler::handle_motd(
    const protocol::d2cs::D2CSMotdRequest& /*req*/)
{
    // The original on_client_motdreq ALWAYS replies SERVER_MOTDREPLY with
    // prefs_get_motd(); a no-op stub hung the client. v3 has no per-realm motd
    // config yet, so it sends the conventional d2cs default text.
    egress_.send_motd("No Message Of The Day Set");
    return {};
}

core::Result<void, core::Error> D2CSSessionHandler::handle_cancel_create_game()
{
    // Stub — CancelCreateGame use case wired in a later round.
    return {};
}

core::Result<void, core::Error> D2CSSessionHandler::handle_convert_char(
    const protocol::d2cs::D2CSConvertCharRequest& /*req*/)
{
    // Stub — ConvertCharacter use case wired in a later round.
    return {};
}

void D2CSSessionHandler::handle_disconnect()
{
    // No-op — session cleanup handled by the composition root.
}

} // namespace pvpgn::app::d2cs
