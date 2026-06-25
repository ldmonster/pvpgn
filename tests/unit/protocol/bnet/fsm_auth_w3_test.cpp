// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/protocol/bnet/fsm_auth_w3_test.cpp
//
// WarCraft III SRP-3 (NLS) login, driven through BnetFsm with the genuine
// LoginUserW3 / CreateAccount use-cases over in-memory repos + an in-memory
// SRP-3 credential store. The "client" side is BnetSrp3 itself — the
// parity-verified, legacy-bit-compatible Battle.net SRP-3 implementation — so a
// successful round-trip here means the bytes v3 puts on the wire interoperate
// with what a real WAR3/W3XP client (and the original server) produce.
//
// Flow exercised end to end:
//   AUTH_INFO(WAR3)            -> 0x50 seed, logon-type 2
//   CREATEACCOUNT2(salt,v,usr) -> account + SRP-3 creds stored
//   LOGINREQ_W3(A,usr)         -> salt + server public key B
//   LOGONPROOFREQ(M1)          -> M2, session attached, state LoggedIn
// The client recomputes M1 from B and verifies the server's M2.

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/create_account.hpp"
#include "application/auth/login_user_w3.hpp"
#include "application/auth/srp3_credential_store.hpp"
#include "core/clock.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "infra/crypto/big_uint.hpp"
#include "infra/crypto/bnet_srp3.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"
#include "infra/inmemory/session_registry.hpp"
#include "infra/inmemory/srp3_credential_store.hpp"
#include "protocol/bnet/fsm.hpp"
#include "protocol/bnet/messages.hpp"

#include "capturing_session_context.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::bnet;
using pvpgn::protocol::bnet::test::CapturingSessionContext;
using pvpgn::v3::infra::crypto::BigUInt;
using pvpgn::v3::infra::crypto::BnetSrp3;

namespace {

// Wire block-size conventions (identical to LoginUserW3 / the original server).
constexpr int kBlkSalt = 4, kBlkVerifier = 1, kBlkA = 1, kBlkB = 4, kBlkProof = 4;

struct W3Harness {
    infra::inmemory::InMemoryAccountRepository    accounts;
    infra::inmemory::InMemorySessionRegistry      sessions;
    infra::inmemory::InMemoryEventBus             bus;
    infra::inmemory::InMemoryIpBanRepository      ip_bans;
    infra::inmemory::InMemorySrp3CredentialStore  srp3;
    core::ManualClock                             clock{core::SystemTime{}};
    std::shared_ptr<CapturingSessionContext> ctx =
        std::make_shared<CapturingSessionContext>();

    BnetUseCaseContext make_ctx() {
        BnetUseCaseContext uc;
        uc.create_account = std::make_shared<application::auth::CreateAccount>(
            accounts, ip_bans, bus, clock);
        uc.login_user_w3 =
            std::make_shared<application::auth::LoginUserW3>(srp3);
        uc.srp3_store = std::shared_ptr<application::auth::ISrp3CredentialStore>(
            &srp3, [](application::auth::ISrp3CredentialStore*) noexcept {});
        uc.session_registry = std::shared_ptr<domain::identity::ISessionRegistry>(
            &sessions, [](domain::identity::ISessionRegistry*) noexcept {});
        return uc;
    }

    BnetFsm make_fsm() {
        return BnetFsm{ctx, make_ctx(), domain::SessionId{7}};
    }

    template <class T>
    const T* last_as() const {
        const T* found = nullptr;
        for (const auto& m : ctx->all_sent())
            if (const auto* p = std::get_if<T>(&m)) found = p;
        return found;
    }
};

template <std::size_t N>
std::array<std::uint8_t, N> to_wire(const BigUInt& v, int block) {
    std::array<std::uint8_t, N> out{};
    v.to_bytes_legacy(out, block, /*big_endian=*/false);
    return out;
}

// The server reads the verifier and client public key A with
// from_bytes_legacy(.,1,false), which is plain little-endian; so a real client
// serialises them little-endian. (Empirically the inverse of from_legacy(1).)
std::array<std::uint8_t, 32> le32(const BigUInt& v) {
    std::array<std::uint8_t, 32> out{};
    v.to_bytes(out, /*big_endian=*/false);
    return out;
}

}  // namespace

TEST_CASE("fsm W3: SRP-3 create + login round-trips against a BnetSrp3 client",
          "[protocol][bnet][auth][w3][srp]") {
    W3Harness h;
    auto fsm = h.make_fsm();

    const std::string user = "w3user";
    const std::string pass = "hunter2pass";

    // --- client-side registration artefacts (what a real WAR3 client sends) --
    // A fixed 32-byte salt; the client derives its verifier from it.
    std::array<std::uint8_t, 32> salt_wire{};
    for (std::size_t i = 0; i < salt_wire.size(); ++i)
        salt_wire[i] = static_cast<std::uint8_t>(0x11 * (i + 1));
    const BigUInt salt = BigUInt::from_bytes_legacy(salt_wire, kBlkSalt, false);

    BnetSrp3 client{user, pass};
    client.set_salt(salt);
    const BigUInt v = client.verifier();
    const BigUInt A = client.client_session_public_key();
    const auto verifier_wire = le32(v);
    const auto A_wire        = le32(A);

    // --- AUTH_INFO (WAR3) → logon-type 2 seed -------------------------------
    AuthInfo ai;
    ai.game_id = domain::tags::kWarcraft3.packed_be();
    REQUIRE(fsm.handle(ClientMessage{ai}).has_value());
    REQUIRE(h.last_as<AuthInfoReply>() != nullptr);
    CHECK(h.last_as<AuthInfoReply>()->logontype == 2u);

    // --- CREATEACCOUNT2 (0x52): store salt + verifier -----------------------
    CreateAccount2Request create;
    create.salt              = salt_wire;
    create.password_verifier = verifier_wire;
    create.account_name      = user;
    REQUIRE(fsm.handle(ClientMessage{create}).has_value());
    REQUIRE(h.last_as<CreateAccount2Reply>() != nullptr);
    CHECK(h.last_as<CreateAccount2Reply>()->result == kCreateAccount2ResultOk);

    // --- LOGINREQ_W3 (0x53): send A, receive salt + B -----------------------
    LoginW3Request login;
    login.client_public_key = A_wire;
    login.account_name      = user;
    REQUIRE(fsm.handle(ClientMessage{login}).has_value());
    const auto* reply = h.last_as<LoginW3Reply>();
    REQUIRE(reply != nullptr);
    CHECK(reply->message == kLoginW3MessageSuccess);
    CHECK(reply->salt == salt_wire);  // salt echoed verbatim

    // Client reconstructs B and computes its proof M1 + expected server M2.
    const BigUInt B = BigUInt::from_bytes(reply->server_public_key, /*big_endian=*/false);
    const BigUInt K = client.hashed_client_secret(B);
    const BigUInt M1 = client.client_password_proof(A, B, K);
    const BigUInt M2_expected = client.server_password_proof(A, M1, K);

    // --- LOGONPROOFREQ (0x54): send M1, receive M2 --------------------------
    LogonProofW3Request proof;
    proof.client_password_proof = to_wire<20>(M1, kBlkProof);
    REQUIRE(fsm.handle(ClientMessage{proof}).has_value());
    const auto* preply = h.last_as<LogonProofW3Reply>();
    REQUIRE(preply != nullptr);
    CHECK(preply->response == kLogonProofW3ResponseOk);

    // Server's M2 must match the client's independently-computed M2.
    CHECK(preply->server_password_proof == to_wire<20>(M2_expected, kBlkProof));

    // Login completed: state advanced and the session is attached.
    CHECK(fsm.state() == BnetState::LoggedIn);
    CHECK(h.sessions.account_for(domain::SessionId{7}).has_value());
}

TEST_CASE("fsm W3: wrong password proof is rejected with BadPass",
          "[protocol][bnet][auth][w3][srp]") {
    W3Harness h;
    auto fsm = h.make_fsm();
    const std::string user = "w3user";

    std::array<std::uint8_t, 32> salt_wire{};
    salt_wire.fill(0x42);
    const BigUInt salt = BigUInt::from_bytes_legacy(salt_wire, kBlkSalt, false);
    BnetSrp3 client{user, "rightpass"};
    client.set_salt(salt);

    AuthInfo ai;
    ai.game_id = domain::tags::kWarcraft3.packed_be();
    REQUIRE(fsm.handle(ClientMessage{ai}).has_value());

    CreateAccount2Request create;
    create.salt              = salt_wire;
    create.password_verifier = le32(client.verifier());
    create.account_name      = user;
    REQUIRE(fsm.handle(ClientMessage{create}).has_value());

    LoginW3Request login;
    login.client_public_key = le32(client.client_session_public_key());
    login.account_name      = user;
    REQUIRE(fsm.handle(ClientMessage{login}).has_value());

    // Send garbage proof.
    LogonProofW3Request proof;
    proof.client_password_proof.fill(0xAB);
    REQUIRE(fsm.handle(ClientMessage{proof}).has_value());
    const auto* preply = h.last_as<LogonProofW3Reply>();
    REQUIRE(preply != nullptr);
    CHECK(preply->response == kLogonProofW3ResponseBadPass);
    CHECK(fsm.state() != BnetState::LoggedIn);
}

TEST_CASE("fsm W3: login for an unknown account fails",
          "[protocol][bnet][auth][w3][srp]") {
    W3Harness h;
    auto fsm = h.make_fsm();

    AuthInfo ai;
    ai.game_id = domain::tags::kWar3Xp.packed_be();
    REQUIRE(fsm.handle(ClientMessage{ai}).has_value());

    LoginW3Request login;
    login.client_public_key.fill(0x01);
    login.account_name = "ghost";
    REQUIRE(fsm.handle(ClientMessage{login}).has_value());
    const auto* reply = h.last_as<LoginW3Reply>();
    REQUIRE(reply != nullptr);
    CHECK(reply->message == kLoginW3MessageFailure);
}
