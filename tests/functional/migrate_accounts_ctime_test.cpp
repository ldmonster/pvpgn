// SPDX-License-Identifier: GPL-2.0-or-later
//
// Interop regression for pvpgn-migrate account create-time handling.
//
// The migrator (src/app/pvpgn-migrate/migrate_accounts.cpp) reads a legacy
// account's create time from the key the ORIGINAL PvPGN actually writes:
// BNET\acct\ctime (src/bnetd/account.cpp:167). It used to read a non-existent
// BNET\acct\created key, so every migrated account got created_at = 0.
//
// This test pins the exact read path the migrator uses — parse_account_file +
// get_numeric_field over {"BNET","acct","ctime"} — against a legacy *.plain
// blob, asserting a non-zero create time is recovered and that the bogus
// "created" key yields nothing.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "infra/file/flat_db_reader.hpp"

TEST_CASE("migrate accounts: legacy ctime blob yields a non-zero create time",
          "[functional][migrate][accounts]") {
    // A minimal legacy account blob in the key=value form parse_account_file +
    // get_field operate on (single-backslash, unquoted keys). The original
    // PvPGN stores create time under BNET\acct\ctime and last-login time under
    // BNET\acct\lastlogin_time; in C++ source "\\" is a single backslash.
    const std::string blob =
        "BNET\\acct\\username=Trillian\n"
        "BNET\\acct\\userid=42\n"
        "BNET\\acct\\ctime=1234567890\n"
        "BNET\\acct\\lastlogin_time=1700000000\n";

    const auto kv = pvpgn::infra::file::parse_account_file(blob);

    // The key the migrator now reads — must resolve to the stored value.
    const auto created_at =
        pvpgn::infra::file::get_numeric_field(kv, {"BNET", "acct", "ctime"});
    CHECK(created_at == 1234567890);

    const auto last_login =
        pvpgn::infra::file::get_numeric_field(kv, {"BNET", "acct", "lastlogin_time"});
    CHECK(last_login == 1700000000);

    // The old, never-written key must resolve to the 0 default — this is the
    // exact bug that produced created_at = 0 for every migrated account.
    const auto bogus =
        pvpgn::infra::file::get_numeric_field(kv, {"BNET", "acct", "created"});
    CHECK(bogus == 0);
}
