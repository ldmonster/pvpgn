// SPDX-License-Identifier: GPL-2.0-or-later
//
// Linked-variant implementation of `LegacyAccountRepository`
// (Batch 28a). Reads the legacy `t_account` list and rehydrates
// `identity::Account` aggregates.
//
// `passhash1` in the legacy attribute store is a 40-character lower-
// case hex string. We hex-decode in place to 20 bytes and feed
// `BNHash::from_bytes`. If the attribute is missing or malformed
// we surface `core::StatusCode::Internal` so the caller can decide
// whether to fall back to the legacy path.

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

#include "integration/legacy_bnetd/legacy_account_repository.hpp"
#include "integration/legacy_bnetd/legacy_account_hex.hpp"

#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/ban.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

#include "common/setup_before.h"
#include "bnetd/account.h"
#include "bnetd/account_wrap.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

namespace {

using detail::hex_decode_passhash;
using detail::hex_encode_passhash;

core::Result<domain::identity::Account>
rehydrate_from_legacy(::pvpgn::bnetd::t_account* a) {
    if (a == nullptr) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "account missing"});
    }
    char const* name_c = ::pvpgn::bnetd::account_get_name(a);
    char const* pass_c = ::pvpgn::bnetd::account_get_pass(a);
    unsigned int uid   = ::pvpgn::bnetd::account_get_uid(a);
    if (name_c == nullptr || pass_c == nullptr || uid == 0) {
        return core::fail(core::Error{
            core::StatusCode::Internal,
            "legacy account missing name/pass/uid"});
    }
    auto name = domain::UserName::parse(std::string_view{name_c});
    if (!name) return core::fail(name.error());

    auto bytes = hex_decode_passhash(std::string_view{pass_c});
    if (!bytes) {
        return core::fail(core::Error{
            core::StatusCode::Internal,
            "legacy passhash1 not 40 hex chars"});
    }
    domain::BNHash hash{*bytes};

    const bool must_change =
        (::pvpgn::bnetd::account_get_auth_changepass(a) == 0);
    // 30c: surface the legacy `auth\lock` boolean into the
    // aggregate. Reason / lockby / locktime stay legacy-only -- the
    // aggregate's `locked` flag is intentionally a coarse signal
    // ("can this user log in?") that the v3 use cases gate on.
    const bool locked =
        (::pvpgn::bnetd::account_get_auth_lock(a) != 0);

    // 31c: hydrate the optional `Ban` value from the legacy
    // (lockreason, lockby, locktime) tuple. We surface a ban
    // whenever any of the three carries data, regardless of the
    // `locked` boolean -- the legacy admin commands sometimes set
    // a ban without flipping the boolean (e.g. mute vs lock). The
    // ban's `issuer` is `AccountId{0}` because `lockby` is a
    // *name* in the legacy store and resolving it would cost an
    // extra `accountlist_find_account` per hydration. The reason
    // string is rich enough for current use-cases; the lockby
    // name is appended in parentheses so the information is not
    // lost.
    std::optional<domain::Ban> ban_opt;
    {
        char const* lr   = ::pvpgn::bnetd::account_get_auth_lockreason(a);
        char const* lby  = ::pvpgn::bnetd::account_get_auth_lockby(a);
        const unsigned int lt = ::pvpgn::bnetd::account_get_auth_locktime(a);
        const bool has_reason = (lr  != nullptr && lr[0]  != '\0');
        const bool has_by     = (lby != nullptr && lby[0] != '\0');
        if (has_reason || has_by || lt != 0) {
            domain::Ban b{};
            b.scope  = domain::BanScope::Account;
            b.issuer = domain::AccountId{};
            if (has_reason) {
                b.reason = lr;
                if (has_by) {
                    b.reason += " (by ";
                    b.reason += lby;
                    b.reason += ")";
                }
            } else if (has_by) {
                b.reason = std::string{"(by "} + lby + ")";
            }
            // `issued_at` is not stored separately by the legacy
            // path; leave it default (epoch). `expires_at` is the
            // unix-seconds value when non-zero, else nullopt
            // (permanent).
            if (lt != 0) {
                b.expires_at = core::SystemTime{
                    std::chrono::seconds{static_cast<std::int64_t>(lt)}};
            }
            ban_opt = std::move(b);
        }
    }

    // 31c: locale hydration is intentionally left at the default
    // value. The legacy `WOL\acct\locale` numeric attribute belongs
    // to the Westwood Online subsystem and does not map cleanly
    // onto the BNet 4-char `gamelang` that `domain::Locale`
    // represents. The actual per-session `gamelang` lives on
    // `t_connection`, not on `t_account`, and is propagated by the
    // protocol layer when v3 takes over session establishment.

    domain::identity::CommandGroupMask groups{};
    auto account = domain::identity::Account::rehydrate(
        domain::AccountId{uid}, name.value(), hash,
        domain::Locale{}, groups,
        std::move(ban_opt),
        /*locked=*/locked,
        /*must_change_password=*/must_change);
    return account;
}

}  // namespace

core::Result<domain::identity::Account>
LegacyAccountRepository::find_by_id(domain::AccountId id) const {
    auto* a = ::pvpgn::bnetd::accountlist_find_account_by_uid(id.value());
    if (a == nullptr) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "account not found"});
    }
    return rehydrate_from_legacy(a);
}

core::Result<domain::identity::Account>
LegacyAccountRepository::find_by_name(const domain::UserName& name) const {
    std::string buf{name.display()};
    auto* a = ::pvpgn::bnetd::accountlist_find_account(buf.c_str());
    if (a == nullptr) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "account not found"});
    }
    return rehydrate_from_legacy(a);
}

core::Status<>
LegacyAccountRepository::save(const domain::identity::Account& account) {
    // 29c + 30c: write-back the fields the v3 use-cases mutate:
    //   * passhash1 -- via `account_set_pass` (legacy formats the
    //     hex string; mirrors `_client_changepassreq`'s call).
    //   * BNET\auth\changepass flag -- "1" cleared, "0" must change.
    //   * auth\lock boolean -- coarse "can log in" gate (30c).
    // Other attributes (email, command groups, ban detail, locale)
    // are still managed exclusively by the legacy code path; the
    // aggregate's view of them is read-only here.
    std::string name_buf{account.name().display()};
    auto* a = ::pvpgn::bnetd::accountlist_find_account(name_buf.c_str());
    if (a == nullptr) {
        return core::fail(core::Error{
            core::StatusCode::NotFound,
            "legacy account vanished mid-save"});
    }
    const std::string hex = hex_encode_passhash(account.password_hash1());
    if (::pvpgn::bnetd::account_set_pass(a, hex.c_str()) < 0) {
        return core::fail(core::Error{
            core::StatusCode::Internal,
            "account_set_pass failed"});
    }
    // `auth\changepass` is the legacy boolean: 1 == allowed (i.e.
    // *no* forced rotation pending), 0 == must change. Mirror the
    // semantics from `account_get_auth_changepass`.
    char const* flag = account.must_change_password() ? "0" : "1";
    if (::pvpgn::bnetd::account_set_strattr(
            a, "BNET\\auth\\changepass", flag) < 0) {
        return core::fail(core::Error{
            core::StatusCode::Internal,
            "account_set_strattr(changepass) failed"});
    }
    // 30c: round-trip the coarse `locked` boolean. The
    // lockreason/lockby/locktime fields remain owned by the legacy
    // path because v3 does not model administrator metadata yet.
    if (::pvpgn::bnetd::account_set_auth_lock(
            a, account.is_locked() ? 1 : 0) < 0) {
        return core::fail(core::Error{
            core::StatusCode::Internal,
            "account_set_auth_lock failed"});
    }
    // 31c: round-trip the optional `Ban` value to the legacy
    // (lockreason, lockby, locktime) tuple. We *do not* clear
    // these fields when the aggregate has no ban -- doing so
    // would wipe legacy-side admin state that v3 never observed
    // (e.g. mute metadata stored in the same attributes). Writes
    // only happen when the aggregate carries a ban.
    if (account.ban().has_value()) {
        const auto& b = account.ban().value();
        if (::pvpgn::bnetd::account_set_auth_lockreason(
                a, b.reason.c_str()) < 0) {
            return core::fail(core::Error{
                core::StatusCode::Internal,
                "account_set_auth_lockreason failed"});
        }
        unsigned int lt = 0;
        if (b.expires_at.has_value()) {
            const auto secs = std::chrono::duration_cast<
                std::chrono::seconds>(
                b.expires_at->time_since_epoch()).count();
            if (secs > 0) {
                lt = static_cast<unsigned int>(secs);
            }
        }
        if (::pvpgn::bnetd::account_set_auth_locktime(a, lt) < 0) {
            return core::fail(core::Error{
                core::StatusCode::Internal,
                "account_set_auth_locktime failed"});
        }
        // `lockby` (the legacy admin username) is not derivable
        // from `Ban::issuer` (an AccountId) without a reverse
        // lookup we don't want in the save hot-path. Leave the
        // legacy field untouched.
    }
    // Locale (`WOL\acct\locale`) is intentionally NOT written
    // back: see the rehydrate-side note for why the WOL numeric
    // locale does not match `domain::Locale`'s BNet `gamelang`.
    return core::ok();
}

core::Status<>
LegacyAccountRepository::remove(domain::AccountId) {
    return core::fail(core::Error{
        core::StatusCode::Internal,
        "LegacyAccountRepository::remove not implemented"});
}

void LegacyAccountRepository::forEach(
    std::function<bool(const domain::identity::Account&)>) const {
    // No cheap legacy enumeration; no-op stub.
}

std::size_t LegacyAccountRepository::size() const noexcept {
    // No cheap legacy count; return 0 as stub.
    return 0;
}

}  // namespace pvpgn::integration::legacy_bnetd
