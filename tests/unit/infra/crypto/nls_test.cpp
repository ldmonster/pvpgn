// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for NLS/SRP-6a crypto (Warcraft III / W3XP authentication).
//
// Tests:
//   1. Verifier creation with known username/password
//   2. Full server challenge/verify round-trip (simulated client)
//   3. Wrong password fails verification
//   4. Tampered M1 fails verification

#include <catch2/catch_test_macros.hpp>

#include "infra/crypto/nls.hpp"
#include "infra/crypto/nls_verifier.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace nls = pvpgn::infra::crypto;

// ---- helpers ---------------------------------------------------------------

/// Convert a std::array<std::byte,N> to std::span<const std::byte>.
template <std::size_t N>
static std::span<const std::byte> as_view(const std::array<std::byte, N>& a)
{
    return {a.data(), a.size()};
}

// ---- Simulated NLS client --------------------------------------------------
//
// Implements the client side of NLS/SRP-6a so we can do a full round-trip
// test without a real Warcraft III client.
//
// Client flow:
//   a  = random 32 bytes mod N
//   A  = g^a mod N                    (128 bytes, sent to server)
//   x  = H(s || H(U:P))
//   u  = H(A || B)[0..3] as LE uint32
//   S  = (B - 3*v)^(a + u*x) mod N   where v = g^x mod N
//   K  = interleave(SHA1(S_even), SHA1(S_odd))
//   M1 = H(H(N) XOR H(g), H(I), s, A, B, K)
//   M2_expected = H(A, M1, K)

namespace {

// WAR3 NLS 1024-bit prime (same as in nls.cpp)
constexpr std::array<std::uint8_t, 128> kNlsPrimeN = {
    0xF4, 0x88, 0xFD, 0x58, 0x4E, 0x49, 0xDB, 0xCD,
    0x20, 0xB4, 0x9D, 0xE4, 0x91, 0x07, 0x36, 0x6B,
    0x33, 0x6C, 0x38, 0x0D, 0x45, 0x1D, 0x0F, 0x7C,
    0x88, 0xB3, 0x1C, 0x7C, 0x5B, 0x2D, 0x8E, 0xF6,
    0xF3, 0xC9, 0x23, 0xC0, 0x43, 0xF0, 0xA5, 0x5B,
    0x18, 0x8D, 0x8E, 0xBB, 0x55, 0x8C, 0xB8, 0x5D,
    0x38, 0xD3, 0x34, 0xFD, 0x7C, 0x17, 0x57, 0x43,
    0xA3, 0x1D, 0x18, 0x6C, 0xDE, 0x33, 0x21, 0x2C,
    0xB5, 0x2A, 0xFF, 0x3C, 0xE1, 0xB1, 0x29, 0x40,
    0x18, 0x11, 0x8D, 0x7C, 0x84, 0xA7, 0x0A, 0x72,
    0xD6, 0x86, 0xC4, 0x03, 0x19, 0xC8, 0x07, 0x29,
    0x7A, 0xCA, 0x95, 0x0C, 0xD9, 0x96, 0x9F, 0xAB,
    0xD0, 0x0A, 0x50, 0x9B, 0x02, 0x46, 0xD3, 0x08,
    0x3D, 0x66, 0xA4, 0x5D, 0x41, 0x9F, 0x9C, 0x7C,
    0xBD, 0x89, 0x4B, 0x22, 0x19, 0x26, 0xBA, 0xAB,
    0xA2, 0x5E, 0xC3, 0x55, 0xE9, 0x2F, 0x78, 0xC7
};

constexpr std::uint8_t kGenerator = 0x2Fu;

struct BnDeleter { void operator()(BIGNUM* p) const noexcept { BN_free(p); } };
struct BnCtxDeleter { void operator()(BN_CTX* p) const noexcept { BN_CTX_free(p); } };
using BnPtr    = std::unique_ptr<BIGNUM, BnDeleter>;
using BnCtxPtr = std::unique_ptr<BN_CTX, BnCtxDeleter>;

BnPtr bn_new_checked()
{
    BnPtr p{BN_new()};
    REQUIRE(p != nullptr);
    return p;
}

BnCtxPtr bn_ctx_new_checked()
{
    BnCtxPtr p{BN_CTX_new()};
    REQUIRE(p != nullptr);
    return p;
}

std::string ascii_upper(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char raw : s) {
        const unsigned char c = static_cast<unsigned char>(raw);
        out.push_back(c < 0x80u ? static_cast<char>(std::toupper(c))
                                 : static_cast<char>(c));
    }
    return out;
}

std::array<std::uint8_t, 20>
sha1_buf(const void* data, std::size_t len)
{
    std::array<std::uint8_t, 20> out{};
    unsigned int outlen = 20;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    REQUIRE(ctx != nullptr);
    REQUIRE(EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr));
    REQUIRE(EVP_DigestUpdate(ctx, data, len));
    REQUIRE(EVP_DigestFinal_ex(ctx, out.data(), &outlen));
    EVP_MD_CTX_free(ctx);
    return out;
}

std::array<std::uint8_t, 20>
sha1_multi(std::initializer_list<std::pair<const void*, std::size_t>> parts)
{
    std::array<std::uint8_t, 20> out{};
    unsigned int outlen = 20;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    REQUIRE(ctx != nullptr);
    REQUIRE(EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr));
    for (auto [ptr, len] : parts)
        REQUIRE(EVP_DigestUpdate(ctx, ptr, len));
    REQUIRE(EVP_DigestFinal_ex(ctx, out.data(), &outlen));
    EVP_MD_CTX_free(ctx);
    return out;
}

/// Compute the 40-byte interleaved session key K from 128-byte S (big-endian).
std::array<std::byte, 40>
compute_session_key(const std::array<std::uint8_t, 128>& S_be)
{
    std::array<std::uint8_t, 64> even_bytes{};
    std::array<std::uint8_t, 64> odd_bytes{};
    for (std::size_t i = 0; i < 64; ++i) {
        even_bytes[i] = S_be[i * 2];
        odd_bytes[i]  = S_be[i * 2 + 1];
    }
    const auto h_even = sha1_buf(even_bytes.data(), 64);
    const auto h_odd  = sha1_buf(odd_bytes.data(), 64);

    std::array<std::byte, 40> K{};
    for (std::size_t i = 0; i < 20; ++i) {
        K[i * 2]     = static_cast<std::byte>(h_even[i]);
        K[i * 2 + 1] = static_cast<std::byte>(h_odd[i]);
    }
    return K;
}

/// Compute M1 = H(H(N) XOR H(g), H(I), s, A, B, K)
std::array<std::uint8_t, 20>
compute_m1(std::string_view                       username,
           const std::array<std::uint8_t, 128>&   A_be,
           const std::array<std::uint8_t, 128>&   B_be,
           const std::array<std::byte, 32>&        salt,
           const std::array<std::byte, 40>&        K)
{
    const auto h_N = sha1_buf(kNlsPrimeN.data(), kNlsPrimeN.size());
    const std::uint8_t g_byte = kGenerator;
    const auto h_g = sha1_buf(&g_byte, 1);

    std::array<std::uint8_t, 20> hN_xor_hg{};
    for (std::size_t i = 0; i < 20; ++i)
        hN_xor_hg[i] = h_N[i] ^ h_g[i];

    const std::string I = ascii_upper(username);
    const auto h_I = sha1_buf(I.data(), I.size());

    return sha1_multi({
        {hN_xor_hg.data(), 20},
        {h_I.data(),        20},
        {salt.data(),       32},
        {A_be.data(),      128},
        {B_be.data(),      128},
        {K.data(),          40}
    });
}

/// Compute M2 = H(A, M1, K)
std::array<std::byte, 20>
compute_m2(const std::array<std::uint8_t, 128>& A_be,
           const std::array<std::uint8_t, 20>&  M1,
           const std::array<std::byte, 40>&      K)
{
    const auto h = sha1_multi({
        {A_be.data(), 128},
        {M1.data(),    20},
        {K.data(),     40}
    });
    std::array<std::byte, 20> m2{};
    std::memcpy(m2.data(), h.data(), 20);
    return m2;
}

/// Simulated NLS client.  Returns (A, M1, M2_expected).
struct ClientResult {
    std::array<std::byte, 128> A{};
    std::array<std::byte, 20>  M1{};
    std::array<std::byte, 20>  M2_expected{};
};

ClientResult simulate_client(std::string_view           username,
                              std::string_view           password,
                              const std::array<std::byte, 32>&  salt,
                              const std::array<std::byte, 128>& server_B)
{
    const auto bn_ctx = bn_ctx_new_checked();

    // Load N and g
    BnPtr N{BN_bin2bn(kNlsPrimeN.data(), 128, nullptr)};
    REQUIRE(N != nullptr);
    BnPtr g = bn_new_checked();
    REQUIRE(BN_set_word(g.get(), kGenerator));

    // Generate random a mod N
    std::array<std::uint8_t, 32> a_bytes{};
    REQUIRE(RAND_bytes(a_bytes.data(), 32) == 1);
    BnPtr a{BN_bin2bn(a_bytes.data(), 32, nullptr)};
    REQUIRE(a != nullptr);
    {
        BnPtr a_mod = bn_new_checked();
        REQUIRE(BN_mod(a_mod.get(), a.get(), N.get(), bn_ctx.get()));
        a = std::move(a_mod);
    }

    // A = g^a mod N
    BnPtr A_bn = bn_new_checked();
    REQUIRE(BN_mod_exp(A_bn.get(), g.get(), a.get(), N.get(), bn_ctx.get()));

    // Export A as 128-byte big-endian
    std::array<std::uint8_t, 128> A_be{};
    {
        const int na = BN_num_bytes(A_bn.get());
        REQUIRE(na <= 128);
        BN_bn2bin(A_bn.get(), A_be.data() + (128 - static_cast<std::size_t>(na)));
    }

    // Export B as 128-byte big-endian
    std::array<std::uint8_t, 128> B_be{};
    std::memcpy(B_be.data(), server_B.data(), 128);

    // x = H(s || H(U:P))
    const std::string U = ascii_upper(username);
    const std::string P = ascii_upper(password);
    std::string userpass;
    userpass.reserve(U.size() + 1 + P.size());
    userpass.append(U);
    userpass.push_back(':');
    userpass.append(P);
    const auto h_up = sha1_buf(userpass.data(), userpass.size());

    std::array<std::uint8_t, 52> xbuf{};
    std::memcpy(xbuf.data(), salt.data(), 32);
    std::memcpy(xbuf.data() + 32, h_up.data(), 20);
    const auto x_raw = sha1_buf(xbuf.data(), 52);

    BnPtr x{BN_bin2bn(x_raw.data(), 20, nullptr)};
    REQUIRE(x != nullptr);

    // u = H(A || B)[0..3] as LE uint32
    const auto u_hash = sha1_multi({{A_be.data(), 128}, {B_be.data(), 128}});
    const std::uint32_t u_val =
        static_cast<std::uint32_t>(u_hash[0])        |
        (static_cast<std::uint32_t>(u_hash[1]) << 8)  |
        (static_cast<std::uint32_t>(u_hash[2]) << 16) |
        (static_cast<std::uint32_t>(u_hash[3]) << 24);
    BnPtr u = bn_new_checked();
    REQUIRE(BN_set_word(u.get(), u_val));

    // v = g^x mod N
    BnPtr v = bn_new_checked();
    REQUIRE(BN_mod_exp(v.get(), g.get(), x.get(), N.get(), bn_ctx.get()));

    // B_bn from wire
    BnPtr B_bn{BN_bin2bn(B_be.data(), 128, nullptr)};
    REQUIRE(B_bn != nullptr);

    // S = (B - 3*v)^(a + u*x) mod N
    // First: 3*v mod N
    BnPtr three = bn_new_checked();
    REQUIRE(BN_set_word(three.get(), 3u));
    BnPtr three_v = bn_new_checked();
    REQUIRE(BN_mod_mul(three_v.get(), three.get(), v.get(), N.get(), bn_ctx.get()));

    // base = (B - 3*v) mod N
    BnPtr base = bn_new_checked();
    REQUIRE(BN_mod_sub(base.get(), B_bn.get(), three_v.get(), N.get(), bn_ctx.get()));

    // exp = a + u*x
    BnPtr ux = bn_new_checked();
    REQUIRE(BN_mul(ux.get(), u.get(), x.get(), bn_ctx.get()));
    BnPtr exp_val = bn_new_checked();
    REQUIRE(BN_add(exp_val.get(), a.get(), ux.get()));

    // S = base^exp mod N
    BnPtr S = bn_new_checked();
    REQUIRE(BN_mod_exp(S.get(), base.get(), exp_val.get(), N.get(), bn_ctx.get()));

    // Export S as 128-byte big-endian
    std::array<std::uint8_t, 128> S_be{};
    {
        const int ns = BN_num_bytes(S.get());
        REQUIRE(ns <= 128);
        BN_bn2bin(S.get(), S_be.data() + (128 - static_cast<std::size_t>(ns)));
    }

    // K = interleaved session key
    const auto K = compute_session_key(S_be);

    // M1 = H(H(N) XOR H(g), H(I), s, A, B, K)
    const auto M1_raw = compute_m1(username, A_be, B_be, salt, K);

    // M2_expected = H(A, M1, K)
    const auto M2_raw = compute_m2(A_be, M1_raw, K);

    ClientResult result{};
    std::memcpy(result.A.data(), A_be.data(), 128);
    std::memcpy(result.M1.data(), M1_raw.data(), 20);
    result.M2_expected = M2_raw;
    return result;
}

}  // namespace

// ============================================================================
// Test cases
// ============================================================================

TEST_CASE("NlsVerifier: create_verifier produces non-zero salt and verifier",
          "[infra][crypto][nls]")
{
    const auto [salt, verifier] =
        nls::NlsVerifier::create_verifier("testuser", "testpass");

    // Salt must be 32 bytes (non-zero with overwhelming probability)
    bool salt_nonzero = false;
    for (const auto b : salt)
        if (b != std::byte{0}) { salt_nonzero = true; break; }
    REQUIRE(salt_nonzero);

    // Verifier must be 128 bytes (non-zero)
    bool verifier_nonzero = false;
    for (const auto b : verifier)
        if (b != std::byte{0}) { verifier_nonzero = true; break; }
    REQUIRE(verifier_nonzero);
}

TEST_CASE("NlsVerifier: same credentials produce same verifier with same salt",
          "[infra][crypto][nls]")
{
    // Create a fixed salt
    std::array<std::byte, 32> fixed_salt{};
    for (std::size_t i = 0; i < 32; ++i)
        fixed_salt[i] = static_cast<std::byte>(i + 1);

    const auto x1 = nls::NlsVerifier::hash_password("USER", "PASS", fixed_salt);
    const auto x2 = nls::NlsVerifier::hash_password("USER", "PASS", fixed_salt);
    REQUIRE(x1 == x2);
}

TEST_CASE("NlsVerifier: hash_password is case-insensitive for username/password",
          "[infra][crypto][nls]")
{
    std::array<std::byte, 32> salt{};
    for (std::size_t i = 0; i < 32; ++i)
        salt[i] = static_cast<std::byte>(i);

    const auto x_lower = nls::NlsVerifier::hash_password("user", "pass", salt);
    const auto x_upper = nls::NlsVerifier::hash_password("USER", "PASS", salt);
    const auto x_mixed = nls::NlsVerifier::hash_password("User", "Pass", salt);

    REQUIRE(x_lower == x_upper);
    REQUIRE(x_lower == x_mixed);
}

TEST_CASE("NlsServer: full round-trip with correct password succeeds",
          "[infra][crypto][nls]")
{
    constexpr std::string_view username = "TESTUSER";
    constexpr std::string_view password = "TESTPASS";

    // 1. Create verifier (account registration)
    const auto [salt, verifier] =
        nls::NlsVerifier::create_verifier(username, password);

    // 2. Server generates challenge
    nls::NlsContext ctx = nls::NlsServer::create_challenge(
        username, as_view(verifier), as_view(salt));

    // Verify context fields are populated
    bool B_nonzero = false;
    for (const auto b : ctx.server_public_key)
        if (b != std::byte{0}) { B_nonzero = true; break; }
    REQUIRE(B_nonzero);

    // 3. Simulate client computing A and M1
    const auto client = simulate_client(username, password, salt,
                                        ctx.server_public_key);

    // 4. Server verifies M1 and returns M2
    const auto result = nls::NlsServer::verify_proof(
        ctx, username, as_view(client.A), as_view(client.M1));

    REQUIRE(result.has_value());

    // 5. Client verifies M2
    REQUIRE(result.value() == client.M2_expected);
}

TEST_CASE("NlsServer: wrong password fails verification",
          "[infra][crypto][nls]")
{
    constexpr std::string_view username = "TESTUSER";
    constexpr std::string_view correct_password = "CORRECTPASS";
    constexpr std::string_view wrong_password   = "WRONGPASS";

    // Create verifier with correct password
    const auto [salt, verifier] =
        nls::NlsVerifier::create_verifier(username, correct_password);

    // Server generates challenge
    nls::NlsContext ctx = nls::NlsServer::create_challenge(
        username, as_view(verifier), as_view(salt));

    // Client uses wrong password
    const auto client = simulate_client(username, wrong_password, salt,
                                        ctx.server_public_key);

    // Server should reject
    const auto result = nls::NlsServer::verify_proof(
        ctx, username, as_view(client.A), as_view(client.M1));

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == nls::NlsError::InvalidProof);
}

TEST_CASE("NlsServer: tampered M1 fails verification",
          "[infra][crypto][nls]")
{
    constexpr std::string_view username = "TESTUSER";
    constexpr std::string_view password = "TESTPASS";

    const auto [salt, verifier] =
        nls::NlsVerifier::create_verifier(username, password);

    nls::NlsContext ctx = nls::NlsServer::create_challenge(
        username, as_view(verifier), as_view(salt));

    auto client = simulate_client(username, password, salt,
                                  ctx.server_public_key);

    // Tamper with M1
    client.M1[0] ^= std::byte{0xFFu};

    const auto result = nls::NlsServer::verify_proof(
        ctx, username, as_view(client.A), as_view(client.M1));

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == nls::NlsError::InvalidProof);
}

TEST_CASE("NlsServer: zero client public key A is rejected",
          "[infra][crypto][nls]")
{
    constexpr std::string_view username = "TESTUSER";
    constexpr std::string_view password = "TESTPASS";

    const auto [salt, verifier] =
        nls::NlsVerifier::create_verifier(username, password);

    nls::NlsContext ctx = nls::NlsServer::create_challenge(
        username, as_view(verifier), as_view(salt));

    // A = 0 (all zeros) is a protocol violation
    std::array<std::byte, 128> zero_A{};
    std::array<std::byte, 20>  dummy_M1{};

    const auto result = nls::NlsServer::verify_proof(
        ctx, username,
        std::span<const std::byte>{zero_A},
        std::span<const std::byte>{dummy_M1});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == nls::NlsError::InvalidPublicKey);
}
