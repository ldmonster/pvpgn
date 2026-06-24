// SPDX-License-Identifier: GPL-2.0-or-later
// main/server_setup.hpp — ServerConfig builder and BnetUseCaseContext factory.
//
// Separates the "build configuration from CLI args" and "build use-case
// context from service" concerns from the composition root in main().
#pragma once

#include "app/bnetd/server_config.hpp"
#include "application/auth/login_user_nls.hpp"
#include "protocol/bnet/use_case_context.hpp"
#include "services/bnetd/bnetd_service.hpp"

#include "main/cli_args.hpp"

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// Build ServerConfig from CLI args (TOML loading deferred).
// ---------------------------------------------------------------------------

/// Apply CLI overrides on top of the default-constructed ServerConfig.
ServerConfig build_config(const CliArgs& args);

// ---------------------------------------------------------------------------
// Null NLS credential store
// ---------------------------------------------------------------------------

/// Null NLS credential store — used when no persistent credential backend is
/// available.  Returns std::nullopt for every lookup so that WAR3/W3XP NLS
/// auth always fails gracefully (the FSM falls back to OLS or rejects).
struct NullNlsCredentialStore final
    : public application::auth::INlsCredentialStore {
    [[nodiscard]] std::optional<application::auth::NlsCredentials>
    find(std::string_view /*username*/) override {
        return std::nullopt;
    }
};

// ---------------------------------------------------------------------------
// Build BnetUseCaseContext from BnetdService
// ---------------------------------------------------------------------------

/// Build a BnetUseCaseContext populated with real chat use-cases from
/// BnetdService.  The service owns the use-cases; the context holds
/// non-owning shared_ptrs (no-op deleters) so the FSM can call them.
protocol::bnet::BnetUseCaseContext
build_use_cases(services::bnetd::BnetdService& svc);

} // namespace pvpgn::app::bnetd
