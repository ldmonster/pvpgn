// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wol_fsm.hpp
/// WOL (Westwood Online) chat protocol FSM.
///
/// WOL is an IRC-like text protocol used by Command & Conquer, Red Alert,
/// Tiberian Sun, and other Westwood/EA games. It is line-oriented (\r\n
/// terminated) and uses IRC-style commands with WOL-specific extensions.
///
/// Authentication flow:
///   Client → NICK <nickname>
///   Client → USER <username> <hostname> <servername> :<realname>
///   Client → PASS <password>
///   Server → :server 001 <nick> :Welcome to WOL
///
/// State chart:
///
///   ┌────────────┐  NICK   ┌───────────────┐  USER+PASS  ┌─────────────┐
///   │ Connecting │────────▶│ Authenticating │────────────▶│Authenticated│
///   └────────────┘         └───────────────┘             └──────┬──────┘
///                                                               │ JOIN
///                                                        ┌──────▼──────┐
///                                                        │  InChannel  │
///                                                        └─────────────┘
///
/// All states accept PING (→ PONG) and QUIT (→ Disconnecting).
///
/// ## LoginUser auth wiring
///
/// When constructed with a `LoginUser&` reference, `on_pass()` calls
/// `login_user_.execute(LoginRequest{...})` using OLS (old-style) auth.
/// On success the FSM transitions to Authenticated and sends 001 RPL_WELCOME.
/// On failure it sends 464 :Password incorrect and closes the connection.
///
/// When constructed without a `LoginUser` (legacy skeleton mode) the FSM
/// accepts any non-empty nick+user combination as before.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "application/auth/login_user.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "protocol/wol/wol_session_context.hpp"

// Forward-declare use-case types to avoid pulling in all their headers into
// every translation unit that includes wol_fsm.hpp.
namespace pvpgn::application::auth {
class LoginUser;
class CreateAccount;
class IWolCredentialStore;
}  // namespace pvpgn::application::auth

namespace pvpgn::application::chat {
class JoinChannel;
class ListChannels;
class PostMessage;
}  // namespace pvpgn::application::chat

namespace pvpgn::domain::identity {
class IAccountReader;
class ISessionRegistry;
}  // namespace pvpgn::domain::identity

namespace pvpgn::domain::connection {
class IMessageRouter;
}  // namespace pvpgn::domain::connection

namespace pvpgn::domain::chat {
class IChannelReader;
}  // namespace pvpgn::domain::chat

namespace pvpgn::application::game {
class IWolGameStore;
}  // namespace pvpgn::application::game

namespace pvpgn::application::social {
class AddFriend;
class RemoveFriend;
class ListFriends;
}  // namespace pvpgn::application::social

namespace pvpgn::protocol::wol {

/// Collaborators the native Westwood Online (APGAR/CVERS) login path needs.
/// All pointers are non-owning and must outlive the FSM. When a complete set
/// is supplied the FSM authenticates WOL clients the way the original server
/// does (auto-create on first login, verbatim APGAR compare thereafter); when
/// absent the FSM falls back to the legacy IRC NICK/USER/PASS path.
struct WolAuthDeps {
    application::auth::CreateAccount*        create_account   = nullptr;
    domain::identity::IAccountReader*       account_reader   = nullptr;
    application::auth::IWolCredentialStore*  wol_store        = nullptr;
    domain::identity::ISessionRegistry*     session_registry = nullptr;

    [[nodiscard]] bool complete() const noexcept {
        return create_account && account_reader && wol_store;
    }
};

/// WOL chat protocol FSM states.
enum class WolState : std::uint8_t {
    Connecting,      ///< Initial state — waiting for NICK
    Authenticating,  ///< Received NICK — waiting for USER / PASS
    Authenticated,   ///< Logged in — normal operation
    InChannel,       ///< Joined a channel
    InGame,          ///< In a game lobby
    Disconnecting,   ///< Graceful disconnect in progress
};

/// WOL chat protocol FSM.
///
/// Accumulates raw bytes into a line buffer, splits on \r\n, and
/// dispatches each complete line to the appropriate command handler.
class WolFsm {
public:
    /// Construct without auth use-case (skeleton / test mode).
    /// on_pass() accepts any non-empty nick+user combination.
    explicit WolFsm(std::shared_ptr<IWolSessionContext> ctx) noexcept
        : ctx_(std::move(ctx)), login_user_(nullptr) {}

    /// Construct with a LoginUser use-case (production mode).
    /// on_pass() calls login_user_.execute() for OLS credential check.
    /// @param ctx        Session I/O context. Non-owning shared ownership.
    /// @param login_user OLS authentication use-case. Non-owning ref;
    ///                   must outlive the FSM.
    WolFsm(std::shared_ptr<IWolSessionContext> ctx,
           application::auth::LoginUser& login_user) noexcept
        : ctx_(std::move(ctx)), login_user_(&login_user) {}

    /// Construct with full chat use-cases (production mode).
    /// @param ctx          Session I/O context.
    /// @param login_user   OLS authentication use-case (non-owning, may be null).
    /// @param list_channels ListChannels use-case for LIST command.
    /// @param join_channel  JoinChannel use-case for JOIN command.
    /// @param post_message  PostMessage use-case for PRIVMSG command.
    WolFsm(std::shared_ptr<IWolSessionContext> ctx,
           application::auth::LoginUser* login_user,
           std::shared_ptr<application::chat::ListChannels> list_channels,
           std::shared_ptr<application::chat::JoinChannel>  join_channel,
           std::shared_ptr<application::chat::PostMessage>  post_message) noexcept
        : ctx_(std::move(ctx))
        , login_user_(login_user)
        , list_channels_(std::move(list_channels))
        , join_channel_(std::move(join_channel))
        , post_message_(std::move(post_message)) {}

    /// Construct with the native WOL auth collaborators (production mode). The
    /// FSM authenticates via the Westwood CVERS/VERCHK/APGAR/NICK/USER flow,
    /// mirroring the original server. Optionally also wires the chat use-cases.
    WolFsm(std::shared_ptr<IWolSessionContext> ctx,
           WolAuthDeps auth,
           std::shared_ptr<application::chat::ListChannels> list_channels = nullptr,
           std::shared_ptr<application::chat::JoinChannel>  join_channel = nullptr,
           std::shared_ptr<application::chat::PostMessage>  post_message = nullptr) noexcept
        : ctx_(std::move(ctx))
        , login_user_(nullptr)
        , list_channels_(std::move(list_channels))
        , join_channel_(std::move(join_channel))
        , post_message_(std::move(post_message))
        , auth_(auth) {}

    /// Assign the cross-session identity for this connection. Must be called
    /// before authentication so the session registry attaches the account to
    /// the right SessionId and the message router can deliver to it. @p router
    /// and @p channel_reader are non-owning and must outlive the FSM (both may
    /// be null in test mode, in which case channel messages are accepted but
    /// not relayed). @p channel_reader resolves a channel's current members for
    /// GAMEOPT/STARTG-style broadcasts.
    void set_routing(domain::SessionId session_id,
                     domain::connection::IMessageRouter* router,
                     domain::chat::IChannelReader* channel_reader = nullptr) noexcept {
        session_id_     = session_id;
        message_router_ = router;
        channel_reader_ = channel_reader;
    }

    /// Wire the WOL game-channel registry so JOINGAME can create/find games.
    /// Non-owning; null in test/stub mode (JOINGAME then falls back to a local
    /// channel join without game tracking).
    void set_game_store(application::game::IWolGameStore* store) noexcept {
        wol_game_store_ = store;
    }

    /// Wire the social use-cases backing the WOL buddy commands (GETBUDDY /
    /// ADDBUDDY / DELBUDDY), reusing the same friend-list store as BNCS. All
    /// non-owning; null in test/stub mode (buddy commands then no-op).
    void set_social(application::social::AddFriend* add,
                    application::social::RemoveFriend* remove,
                    application::social::ListFriends* list) noexcept {
        add_friend_    = add;
        remove_friend_ = remove;
        list_friends_  = list;
    }

    /// Feed raw bytes from the TCP stream into the FSM.
    /// Returns ok() on success; error causes the session to close.
    core::Status<> on_bytes(std::span<const std::byte> bytes);

    /// Called when the connection is closing (peer close or error).
    void on_close();

    /// Current FSM state (for testing / introspection).
    WolState state() const noexcept { return state_; }

    /// Current nickname (empty until NICK received).
    std::string_view nick() const noexcept { return nick_; }

    /// Current channel (empty until JOIN).
    std::string_view channel() const noexcept { return channel_; }

private:
    // -----------------------------------------------------------------------
    // Line processing
    // -----------------------------------------------------------------------

    /// Process all complete \r\n-terminated lines in line_buf_.
    core::Status<> process_lines();

    /// Dispatch one complete line (without the trailing \r\n).
    core::Status<> dispatch_line(std::string_view line);

    // -----------------------------------------------------------------------
    // Command handlers
    // -----------------------------------------------------------------------

    /// NICK <nickname>
    core::Status<> on_nick(std::string_view params);

    /// USER <username> <hostname> <servername> :<realname>
    core::Status<> on_user(std::string_view params);

    /// PASS <password>
    core::Status<> on_pass(std::string_view params);

    /// CVERS <oldvernum> <SKU> — sets the WOL client SKU/version.
    core::Status<> on_cvers(std::string_view params);

    /// VERCHK <SKU> <version> — version check; replies 379 NONREQ.
    core::Status<> on_verchk(std::string_view params);

    /// APGAR <token> — stores the opaque Westwood password token.
    core::Status<> on_apgar(std::string_view params);

    /// PING <token>
    core::Status<> on_ping(std::string_view params);

    /// QUIT [:<reason>]
    core::Status<> on_quit(std::string_view params);

    /// LIST
    core::Status<> on_list(std::string_view params);

    /// JOIN #<channel>
    core::Status<> on_join(std::string_view params);

    /// PART #<channel>
    core::Status<> on_part(std::string_view params);

    /// PRIVMSG <target> :<message>
    core::Status<> on_privmsg(std::string_view params);

    /// GAMEOPT <target> :<gameOptions> — relay opaque game-option text to the
    /// current channel's members (target "#...") or whisper it to a single nick.
    /// Mirrors the original `_handle_gameopt_command` (channel-talk / whisper).
    core::Status<> on_gameopt(std::string_view params);

    /// JOINGAME — create a game-channel (>= 7 params) or join an existing one
    /// (2-3 params). Tracks the game in the WOL game store, joins the backing
    /// chat channel, and emits the WOLv1 JOINGAME acknowledgement (create acks
    /// the host; join acks every channel member). Mirrors the original
    /// `_handle_joingame_command`.
    core::Status<> on_joingame(std::string_view params);

    /// FINDUSER / FINDUSEREX <nick> — report whether a user is online (and thus
    /// findable; findme defaults on) and which channel they are in. Replies
    /// 388 (FINDUSER) / 398 (FINDUSEREX): "0 :<channel>" when found, "1 :" when
    /// not. @p ex selects FINDUSEREX (398, ",0" suffix). Mirrors the original
    /// `_handle_finduser_command` / `_handle_finduserex_command`.
    core::Status<> on_finduser(std::string_view params, bool ex);

    /// SETCODEPAGE <cp> — store this session's codepage; reply 329 <cp>.
    core::Status<> on_setcodepage(std::string_view params);
    /// GETCODEPAGE <nick...> — reply 328 "<nick>`<cp>`..." (own codepage for
    /// this session's nick, 0 for others — v3 has no cross-session codepage map).
    core::Status<> on_getcodepage(std::string_view params);
    /// SETLOCALE <locale> — store this session's locale; reply 310 <locale>.
    core::Status<> on_setlocale(std::string_view params);
    /// GETLOCALE <nick...> — reply 309 "<nick>`<locale>`...".
    core::Status<> on_getlocale(std::string_view params);
    /// GETINSIDER <nick> — reply 399 "<nick>`0" (461 with no param).
    core::Status<> on_getinsider(std::string_view params);

    /// GETBUDDY — reply 333 with the backtick-terminated buddy (friend) list.
    core::Status<> on_getbuddy();

    /// ADDBUDDY <name> — add a buddy (friend); reply 334 <name>, or 401 if the
    /// named account does not exist. Mirrors `_handle_addbuddy_command`.
    core::Status<> on_addbuddy(std::string_view params);

    /// DELBUDDY <name> — remove a buddy; reply 335 <name> (echoes the name
    /// regardless, like the original). Mirrors `_handle_delbuddy_command`.
    core::Status<> on_delbuddy(std::string_view params);

    /// Route a fully-formed IRC line (CRLF appended here) to each recipient
    /// SessionId via the message router. Best-effort; null router → no-op.
    void route_irc_line(const std::string& line,
                        const std::vector<domain::SessionId>& recipients);

    /// Resolve the current channel's member SessionIds (optionally excluding
    /// this session) via the channel reader + session registry. Empty when the
    /// collaborators are absent or the channel is unknown.
    std::vector<domain::SessionId>
    current_channel_member_sessions(bool exclude_self) const;

    /// Attempt authentication with the current nick_/user_/pass_ credentials.
    /// Called from on_pass() and on_user() once all three are available.
    /// @param close_on_bad_credentials  When true, close the connection on any
    ///   auth failure (used when PASS arrived before NICK/USER — the client
    ///   cannot retry in that ordering).  When false, only close on
    ///   UnknownUser; InvalidCredentials keeps the connection open for retry.
    /// Returns ok() in all cases (auth failure is communicated via IRC numerics).
    core::Status<> try_authenticate(bool close_on_bad_credentials);

    /// Native Westwood Online authentication: auto-create the account on first
    /// login (storing the APGAR token), or verbatim-compare the stored token on
    /// subsequent logins, then send the welcome/MOTD. Mirrors the original
    /// `handle_wol_authenticate`. Used when `auth_.complete()` and the client
    /// supplied an APGAR token. Returns ok() always (failure → IRC numeric).
    core::Status<> try_wol_authenticate();

    /// Send the post-auth welcome sequence (001, 002) and MOTD (375 … 376).
    core::Status<> send_welcome_and_motd();

    // -----------------------------------------------------------------------
    // Reply helpers
    // -----------------------------------------------------------------------

    /// Send ":server_name <code> <target> :<text>\r\n"
    core::Status<> send_numeric(int code,
                                std::string_view target,
                                std::string_view text);

    /// Send a raw line (appends \r\n).
    core::Status<> send_raw(std::string_view line);

    /// Send ":<server> <code> <nick> <params>" — the irc_send_cmd framing, with
    /// @p params copied verbatim (no injected ':'). Used by WOL replies whose
    /// payload carries its own structure (codepage/locale/insider/buddy lists).
    core::Status<> send_raw_cmd(int code, std::string_view params);

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------

    std::shared_ptr<IWolSessionContext> ctx_;

    /// Non-owning pointer to the OLS auth use-case.
    /// Null when constructed without auth (skeleton / test mode).
    application::auth::LoginUser* login_user_;

    /// Chat use-cases. Null when not wired (stub mode).
    std::shared_ptr<application::chat::ListChannels> list_channels_;
    std::shared_ptr<application::chat::JoinChannel>  join_channel_;
    std::shared_ptr<application::chat::PostMessage>  post_message_;

    /// Native WOL auth collaborators (empty in legacy/skeleton mode).
    WolAuthDeps auth_{};

    WolState    state_   = WolState::Connecting;
    std::string nick_;
    std::string user_;
    std::string realname_;
    std::string pass_;
    std::string channel_;

    /// Westwood APGAR password token (from the APGAR command); empty until set.
    std::string apgar_;
    /// WOL SKU from CVERS/VERCHK (0 until set); used in the VERCHK reply.
    int         wol_sku_ = 0;

    /// This session's codepage / locale (SETCODEPAGE / SETLOCALE; 0 until set).
    int         codepage_ = 0;
    int         locale_   = 0;

    /// Account ID resolved after successful login (0 until authenticated).
    domain::AccountId account_id_{0};

    /// Channel ID of the currently joined channel (0 until JOIN succeeds).
    domain::ChannelId channel_id_{0};

    /// Session ID for this connection (used in use-case calls).
    domain::SessionId session_id_{};

    /// Cross-session message router for relaying channel chat to other members'
    /// connections. Non-owning; null in test/stub mode (messages still post to
    /// the channel but are not delivered to peers).
    domain::connection::IMessageRouter* message_router_ = nullptr;

    /// Channel reader for resolving a channel's current members (GAMEOPT/STARTG
    /// broadcasts). Non-owning; null in test/stub mode.
    domain::chat::IChannelReader* channel_reader_ = nullptr;

    /// WOL game-channel registry (JOINGAME create/join). Non-owning; null in
    /// test/stub mode.
    application::game::IWolGameStore* wol_game_store_ = nullptr;

    /// Social use-cases for the WOL buddy commands (shared with BNCS friends).
    /// Non-owning; null in test/stub mode.
    application::social::AddFriend*    add_friend_    = nullptr;
    application::social::RemoveFriend* remove_friend_ = nullptr;
    application::social::ListFriends*  list_friends_  = nullptr;

    /// Accumulation buffer for partial lines.
    std::string line_buf_;
};

}  // namespace pvpgn::protocol::wol
