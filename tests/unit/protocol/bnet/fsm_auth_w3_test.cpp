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

TEST_CASE("fsm W3: version >= 0x0D with no e-mail gets RESPONSE_EMAIL on proof",
          "[protocol][bnet][auth][w3][srp]") {
    // Parity with the original _client_loginproofw3: a successful proof from a
    // client at version id >= 0x0D whose account has no e-mail on file returns
    // RESPONSE_EMAIL (0x0E) rather than RESPONSE_OK — still a successful login
    // (M2 is returned), but the client is prompted to register an e-mail. W3
    // accounts in v3 are always e-mail-less, so the version id alone decides it.
    W3Harness h;
    auto fsm = h.make_fsm();

    const std::string user = "w3mailer";
    const std::string pass = "needsemail1";

    std::array<std::uint8_t, 32> salt_wire{};
    for (std::size_t i = 0; i < salt_wire.size(); ++i)
        salt_wire[i] = static_cast<std::uint8_t>(0x07 * (i + 3));
    const BigUInt salt = BigUInt::from_bytes_legacy(salt_wire, kBlkSalt, false);

    BnetSrp3 client{user, pass};
    client.set_salt(salt);

    // AUTH_INFO with a WarCraft III version id >= 0x0D (real WC3 builds are 13+).
    AuthInfo ai;
    ai.game_id    = domain::tags::kWarcraft3.packed_be();
    ai.version_id = 0x1Au;
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
    const auto* reply = h.last_as<LoginW3Reply>();
    REQUIRE(reply != nullptr);

    const BigUInt A = client.client_session_public_key();
    const BigUInt B = BigUInt::from_bytes(reply->server_public_key, /*big_endian=*/false);
    const BigUInt K = client.hashed_client_secret(B);
    const BigUInt M1 = client.client_password_proof(A, B, K);
    const BigUInt M2_expected = client.server_password_proof(A, M1, K);

    LogonProofW3Request proof;
    proof.client_password_proof = to_wire<20>(M1, kBlkProof);
    REQUIRE(fsm.handle(ClientMessage{proof}).has_value());
    const auto* preply = h.last_as<LogonProofW3Reply>();
    REQUIRE(preply != nullptr);
    // Login still succeeds (M2 is returned, session attached) but the code asks
    // the client to register an e-mail.
    CHECK(preply->response == kLogonProofW3ResponseEmail);
    CHECK(preply->server_password_proof == to_wire<20>(M2_expected, kBlkProof));
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

TEST_CASE("fsm W3: NLS password change proves old, stores new, new pass logs in",
          "[protocol][bnet][auth][w3][srp][passchange]") {
    W3Harness h;
    auto fsm = h.make_fsm();
    const std::string user = "w3pc";
    const std::string oldp = "oldpass1", newp = "newpass2";

    std::array<std::uint8_t, 32> salt_wire{};
    for (std::size_t i = 0; i < salt_wire.size(); ++i)
        salt_wire[i] = static_cast<std::uint8_t>(0x05 * (i + 1));
    const BigUInt salt = BigUInt::from_bytes_legacy(salt_wire, kBlkSalt, false);

    // AUTH_INFO + create the account with the OLD password.
    AuthInfo ai; ai.game_id = domain::tags::kWarcraft3.packed_be();
    REQUIRE(fsm.handle(ClientMessage{ai}).has_value());
    BnetSrp3 oldc{user, oldp}; oldc.set_salt(salt);
    CreateAccount2Request create;
    create.salt = salt_wire;
    create.password_verifier = le32(oldc.verifier());
    create.account_name = user;
    REQUIRE(fsm.handle(ClientMessage{create}).has_value());

    // 0x55: challenge proves the OLD password.
    const BigUInt A = oldc.client_session_public_key();
    PassChangeRequest pc;
    pc.client_public_key = le32(A);
    pc.account_name = user;
    REQUIRE(fsm.handle(ClientMessage{pc}).has_value());
    const auto* preply = h.last_as<PassChangeReply>();
    REQUIRE(preply != nullptr);
    CHECK(preply->message == kPassChangeMessageAccept);

    const BigUInt B = BigUInt::from_bytes(preply->server_public_key, false);
    const BigUInt K = oldc.hashed_client_secret(B);
    const BigUInt M1 = oldc.client_password_proof(A, B, K);
    const BigUInt M2_expected = oldc.server_password_proof(A, M1, K);

    // New salt+verifier derived from the NEW password.
    std::array<std::uint8_t, 32> nsalt_wire{};
    for (std::size_t i = 0; i < nsalt_wire.size(); ++i)
        nsalt_wire[i] = static_cast<std::uint8_t>(0x09 * (i + 2));
    const BigUInt nsalt = BigUInt::from_bytes_legacy(nsalt_wire, kBlkSalt, false);
    BnetSrp3 newc{user, newp}; newc.set_salt(nsalt);

    // 0x56: proof + new salt + new verifier → Ok, M2 matches.
    PassChangeProofRequest pp;
    pp.client_password_proof = to_wire<20>(M1, kBlkProof);
    pp.salt = nsalt_wire;
    pp.password_verifier = le32(newc.verifier());
    REQUIRE(fsm.handle(ClientMessage{pp}).has_value());
    const auto* pproof = h.last_as<PassChangeProofReply>();
    REQUIRE(pproof != nullptr);
    CHECK(pproof->response == kPassChangeProofResponseOk);
    CHECK(pproof->server_password_proof == to_wire<20>(M2_expected, kBlkProof));

    // The NEW password must now log in (proves the verifier was replaced).
    LoginW3Request login;
    login.client_public_key = le32(newc.client_session_public_key());
    login.account_name = user;
    REQUIRE(fsm.handle(ClientMessage{login}).has_value());
    const auto* lr = h.last_as<LoginW3Reply>();
    REQUIRE(lr != nullptr);
    CHECK(lr->message == kLoginW3MessageSuccess);
    const BigUInt nA = newc.client_session_public_key();
    const BigUInt nB = BigUInt::from_bytes(lr->server_public_key, false);
    const BigUInt nK = newc.hashed_client_secret(nB);
    const BigUInt nM1 = newc.client_password_proof(nA, nB, nK);
    LogonProofW3Request lp;
    lp.client_password_proof = to_wire<20>(nM1, kBlkProof);
    REQUIRE(fsm.handle(ClientMessage{lp}).has_value());
    const auto* lpr = h.last_as<LogonProofW3Reply>();
    REQUIRE(lpr != nullptr);
    CHECK((lpr->response == kLogonProofW3ResponseOk ||
           lpr->response == kLogonProofW3ResponseEmail));
}

TEST_CASE("fsm W3: NLS password change with wrong old proof is rejected",
          "[protocol][bnet][auth][w3][srp][passchange]") {
    W3Harness h;
    auto fsm = h.make_fsm();
    const std::string user = "w3pcbad";

    std::array<std::uint8_t, 32> salt_wire{}; salt_wire.fill(0x33);
    const BigUInt salt = BigUInt::from_bytes_legacy(salt_wire, kBlkSalt, false);
    AuthInfo ai; ai.game_id = domain::tags::kWarcraft3.packed_be();
    REQUIRE(fsm.handle(ClientMessage{ai}).has_value());
    BnetSrp3 oldc{user, "realpass"}; oldc.set_salt(salt);
    CreateAccount2Request create;
    create.salt = salt_wire;
    create.password_verifier = le32(oldc.verifier());
    create.account_name = user;
    REQUIRE(fsm.handle(ClientMessage{create}).has_value());

    PassChangeRequest pc;
    pc.client_public_key = le32(oldc.client_session_public_key());
    pc.account_name = user;
    REQUIRE(fsm.handle(ClientMessage{pc}).has_value());
    REQUIRE(h.last_as<PassChangeReply>() != nullptr);

    // Garbage proof → BadPass.
    PassChangeProofRequest pp;
    pp.client_password_proof.fill(0xCD);
    pp.salt.fill(0x77);
    pp.password_verifier.fill(0x88);
    REQUIRE(fsm.handle(ClientMessage{pp}).has_value());
    const auto* pproof = h.last_as<PassChangeProofReply>();
    REQUIRE(pproof != nullptr);
    CHECK(pproof->response == kPassChangeProofResponseBadPass);
}
