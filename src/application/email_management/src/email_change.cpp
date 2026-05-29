// SPDX-License-Identifier: GPL-2.0-or-later
//
// application/email_management/email_change.cpp -- R170.a implementation
//
// Pure-function port of the legacy bnetd `_client_setemailreply`
// (set-when-unset path) and `_client_changeemailreq` (replace path)
// validation logic. The dispatcher does no I/O -- the caller
// resolves `stored_email` and persists the result.
//
// Reference legacy code: src/bnetd/handle_bnet.cpp
//   - _client_setemailreply  (~line 7118)
//   - _client_changeemailreq (~line 7147)
//   - _client_getpasswordreq (~line 7197)

#include "application/email_management/email_change.hpp"
#include "application/email_management/password_recovery.hpp"

#include <cctype>
#include <cstddef>

namespace pvpgn::application::email_management {

namespace {

bool iequal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

// Minimal syntactic validation: non-empty, exactly one '@', with a
// non-empty local part and a domain part that contains at least
// one '.'. Intentionally permissive -- the legacy code accepted
// essentially anything non-empty.
bool looks_like_email(std::string_view s) noexcept {
    if (s.empty()) return false;
    const auto at = s.find('@');
    if (at == std::string_view::npos)              return false;
    if (at == 0 || at + 1 >= s.size())             return false;
    if (s.find('@', at + 1) != std::string_view::npos) return false;
    const auto domain = s.substr(at + 1);
    return domain.find('.') != std::string_view::npos;
}

}  // namespace

EmailChangeResponse dispatch_email_change(EmailChangeRequest const& req) {
    EmailChangeResponse resp{};

    // Set-when-unset path: stored email is empty.
    if (req.stored_email.empty()) {
        if (!req.allow_when_unset) {
            resp.status = EmailChangeStatus::kRejected;
            return resp;
        }
        // If the caller supplied a current_email_claim against an
        // unset stored email, treat as a protocol mismatch.
        if (!req.current_email_claim.empty()) {
            resp.status = EmailChangeStatus::kCurrentMismatch;
            return resp;
        }
        if (!looks_like_email(req.new_email)) {
            resp.status = EmailChangeStatus::kNewInvalid;
            return resp;
        }
        resp.status = EmailChangeStatus::kAccepted;
        resp.accepted_email.assign(req.new_email);
        return resp;
    }

    // Replace path: stored email is set; client must echo it back
    // (case-insensitive) before the new address is accepted.
    if (!iequal(req.stored_email, req.current_email_claim)) {
        resp.status = EmailChangeStatus::kCurrentMismatch;
        return resp;
    }
    if (!looks_like_email(req.new_email)) {
        resp.status = EmailChangeStatus::kNewInvalid;
        return resp;
    }
    resp.status = EmailChangeStatus::kAccepted;
    resp.accepted_email.assign(req.new_email);
    return resp;
}

PasswordRecoveryResponse dispatch_password_recovery(
    PasswordRecoveryRequest const& req) {
    PasswordRecoveryResponse resp{};

    if (!req.feature_enabled) {
        resp.status = PasswordRecoveryStatus::kDisabled;
        return resp;
    }
    // Indistinguishable failure modes: no stored email OR mismatch
    // both return kRejected so the protocol cannot be used to
    // enumerate accounts.
    if (req.stored_email.empty()) {
        resp.status = PasswordRecoveryStatus::kRejected;
        return resp;
    }
    if (!iequal(req.stored_email, req.claimed_email)) {
        resp.status = PasswordRecoveryStatus::kRejected;
        return resp;
    }
    resp.status = PasswordRecoveryStatus::kAccepted;
    // Token resolution remains caller-side; the dispatcher does
    // not touch account storage. Caller fills `token_to_deliver`
    // after seeing kAccepted.
    return resp;
}

}  // namespace pvpgn::application::email_management
