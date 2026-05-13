// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file fsm.hpp
/// Per-session Battle.net protocol FSM.
///
/// **What this class does**
///   * Holds the wire-level state (`BnetState`) so the codec stays pure.
///   * Validates that an incoming message is legal in the current state
///     (rejects out-of-order traffic before any use-case fires).
///   * Sends the canonical immediate replies that the protocol demands
///     (PING echo, LOGON acks, ENTERCHAT echo). Real domain mutations
///     are delegated to `ISessionContext` in Phase 5; for now the FSM
///     only performs the wire dance and records what it saw.
///
/// **What this class does NOT do**
///   * No I/O — `send()` goes through `ISessionContext`.
///   * No clock — `Ping` reflects the client cookie verbatim.
///   * No domain access — no `AccountId`, no `Channel` here.
///
/// State chart:
///
///       ┌──────────┐   AUTH_INFO   ┌────────────────────┐
///       │   Init   │──────────────▶│  AuthInfoReceived  │
///       └──────────┘               └─────────┬──────────┘
///                                            │ LOGONRESPONSE2 (ok)
///                                            ▼
///                                  ┌────────────────────┐
///                                  │     LoggedIn       │
///                                  └─────────┬──────────┘
///                                            │ ENTERCHAT
///                                            ▼
///                                  ┌────────────────────┐
///                                  │      InChat        │
///                                  └────────────────────┘
///
/// `Ping` and `Null` are legal in every non-`Closing` state.

#include <cstdint>

#include "core/result.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/bnet/session_context.hpp"

namespace pvpgn::protocol::bnet {

enum class BnetState : std::uint8_t {
    Init,
    AuthInfoReceived,
    LoggedIn,
    InChat,
    Closing,
};

class BnetFsm {
public:
    explicit BnetFsm(ISessionContext& ctx) noexcept : ctx_(&ctx) {}

    BnetState state() const noexcept { return state_; }

    /// Drive the FSM with one decoded inbound message. Returns
    /// `InvalidArgument` if the message is illegal in the current
    /// state (the caller should close the session).
    core::Status<> handle(const ClientMessage& msg);

    // Visitor handlers — public so a custom dispatcher can call them.
    core::Status<> on(const Null&);
    core::Status<> on(const Ping&);
    core::Status<> on(const AuthInfo&);
    core::Status<> on(const LogonResponse2&);
    core::Status<> on(const JoinChannel&);
    core::Status<> on(const EnterChatRequest&);
    core::Status<> on(const ChatCommand&);

private:
    core::Status<> reject(const char* reason);

    ISessionContext* ctx_;
    BnetState        state_ = BnetState::Init;
};

}  // namespace pvpgn::protocol::bnet
