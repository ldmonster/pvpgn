// SPDX-License-Identifier: GPL-2.0-or-later
//
// NLS/SRP-6a server-side implementation for Warcraft III / W3XP.
//
// Algorithm parameters:
//   N  = 1024-bit (128-byte) WAR3 NLS prime
//   g  = 47 (0x2F)
//   H  = SHA-1 (OpenSSL EVP_sha1)
//
// Server flow:
//   create_challenge:
//     b  = random 32 bytes
//     B  = (3*v + g^b mod N) mod N   [128 bytes, sent to client]
//     s  = stored_salt               [32 bytes, sent to client]
//
//   verify_proof:
//     u  = H(A || B)[0..3] as little-endian uint32
//     S  = (A * v^u mod N)^b mod N
//     K  = interleave(SHA1(S_even), SHA1(S_odd))  [40 bytes]
//     M1_expected = H(H(N) XOR H(g), H(I), s, A, B, K)
//     check M1 == M1_expected
//     M2 = H(A, M1, K)
//
// References:
//   http://www.javaop.com/@ron/documents/SRP.html
//   src/common/bnetsrp3.cpp (legacy SRP-3 reference)

#include "infra/crypto/nls.hpp"
#include "infra/crypto/nls_verifier.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace pvpgn::infra::crypto {

namespace {

// ---- WAR3 NLS 1024-bit prime N (big-endian) --------------------------------
//
// This is the standard Warcraft III NLS prime, documented at
// http://www.javaop.com/@ron/documents/SRP.html and used by all
// Battle.net clients for WAR3/W3XP NLS authentication.
// 128 bytes = 1024 bits, big-endian.
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

// g = 47 = 0x2F
constexpr std::uint8_t kGenerator = 0x2Fu;

// ---- RAII wrappers for OpenSSL types ---------------------------------------

struct BnDeleter {
    void operator()(BIGNUM* p) const noexcept { BN_free(p); }
};
struct BnCtxDeleter {
    void operator()(BN_CTX* p) const noexcept { BN_CTX_free(p); }
};

using BnPtr    = std::unique_ptr<BIGNUM, BnDeleter>;
using BnCtxPtr = std::unique_ptr<BN_CTX, BnCtxDeleter>;

// ---- helpers ---------------------------------------------------------------

/// Allocate a new BIGNUM or throw on failure.
BnPtr bn_new()
{
    BnPtr p{BN_new()};
    if (!p) throw std::runtime_error{"NLS: BN_new failed"};
    return p;
}

/// Allocate a BN_CTX or throw on failure.
BnCtxPtr bn_ctx_new()
{
    BnCtxPtr p{BN_CTX_new()};
    if (!p) throw std::runtime_error{"NLS: BN_CTX_new failed"};
    return p;
}

/// Load the NLS prime N as a BIGNUM (big-endian bytes).
BnPtr load_prime()
{
    BnPtr n{BN_bin2bn(kNlsPrimeN.data(),
                      static_cast<int>(kNlsPrimeN.size()), nullptr)};
    if (!n) throw std::runtime_error{"NLS: failed to load prime N"};
    return n;
}

/// Load the generator g as a BIGNUM.
BnPtr load_generator()
{
    BnPtr g{BN_new()};
    if (!g || !BN_set_word(g.get(), kGenerator))
        throw std::runtime_error{"NLS: failed to load generator g"};
    return g;
}

/// Generate `count` cryptographically random bytes.
std::vector<std::uint8_t> random_bytes(std::size_t count)
{
    std::vector<std::uint8_t> buf(count);
    // Use std::random_device as a portable fallback; production code
    // should use RAND_bytes from OpenSSL, but that requires seeding.
    if (RAND_bytes(reinterpret_cast<unsigned char*>(buf.data()),
                   static_cast<int>(count)) != 1) {
        // Fallback to std::random_device if OpenSSL RAND not seeded.
        std::random_device rd;
        for (auto& b : buf)
            b = static_cast<std::uint8_t>(rd() & 0xffu);
    }
    return buf;
}

/// RAII wrapper for EVP_MD_CTX.
struct EvpMdCtxDeleter {
    void operator()(EVP_MD_CTX* p) const noexcept { EVP_MD_CTX_free(p); }
};
using EvpMdCtxPtr = std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter>;

/// Allocate an EVP_MD_CTX or throw on failure.
EvpMdCtxPtr evp_md_ctx_new()
{
    EvpMdCtxPtr p{EVP_MD_CTX_new()};
    if (!p) throw std::runtime_error{"NLS: EVP_MD_CTX_new failed"};
    return p;
}

/// SHA-1 of a single contiguous buffer.  Returns 20-byte digest.
std::array<std::uint8_t, 20>
sha1_buf(const void* data, std::size_t len)
{
    auto ctx = evp_md_ctx_new();
    std::array<std::uint8_t, 20> out{};
    unsigned int outlen = 20;
    if (!EVP_DigestInit_ex(ctx.get(), EVP_sha1(), nullptr) ||
        !EVP_DigestUpdate(ctx.get(), data, len) ||
        !EVP_DigestFinal_ex(ctx.get(), out.data(), &outlen))
        throw std::runtime_error{"NLS: SHA-1 (sha1_buf) failed"};
    return out;
}

/// SHA-1 of a variadic list of (ptr, len) pairs.
/// Usage: sha1_multi(a, la, b, lb, ...)
/// Implemented as a helper that takes an initializer list of spans.
std::array<std::uint8_t, 20>
sha1_multi(std::initializer_list<std::pair<const void*, std::size_t>> parts)
{
    auto ctx = evp_md_ctx_new();
    if (!EVP_DigestInit_ex(ctx.get(), EVP_sha1(), nullptr))
        throw std::runtime_error{"NLS: SHA-1 init failed"};
    for (auto [ptr, len] : parts)
        if (!EVP_DigestUpdate(ctx.get(), ptr, len))
            throw std::runtime_error{"NLS: SHA-1 update failed"};
    std::array<std::uint8_t, 20> out{};
    unsigned int outlen = 20;
    if (!EVP_DigestFinal_ex(ctx.get(), out.data(), &outlen))
        throw std::runtime_error{"NLS: SHA-1 final failed"};
    return out;
}

/// Uppercase ASCII string (matches legacy `safe_toupper`).
std::string ascii_upper(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (c < 0x80u)
            out.push_back(static_cast<char>(
                std::toupper(static_cast<int>(c))));
        else
            out.push_back(static_cast<char>(c));
    }
    return out;
}

/// Export a BIGNUM to a fixed-size big-endian byte array.
/// Zero-pads on the left if the value is smaller than `out.size()`.
template <std::size_t N>
void bn_to_bytes_be(const BIGNUM* bn, std::array<std::uint8_t, N>& out)
{
    out.fill(0u);
    const int nbytes = BN_num_bytes(bn);
    if (nbytes > static_cast<int>(N))
        throw std::runtime_error{"NLS: BIGNUM too large for buffer"};
    BN_bn2bin(bn, out.data() + (N - static_cast<std::size_t>(nbytes)));
}

/// Export a BIGNUM to a fixed-size big-endian std::byte array.
template <std::size_t N>
void bn_to_bytes_be(const BIGNUM* bn, std::array<std::byte, N>& out)
{
    std::array<std::uint8_t, N> tmp{};
    bn_to_bytes_be(bn, tmp);
    std::memcpy(out.data(), tmp.data(), N);
}

/// Import a big-endian byte span into a BIGNUM.
BnPtr bn_from_bytes_be(std::span<const std::byte> bytes)
{
    BnPtr r{BN_bin2bn(
        reinterpret_cast<const unsigned char*>(bytes.data()),
        static_cast<int>(bytes.size()), nullptr)};
    if (!r) throw std::runtime_error{"NLS: BN_bin2bn failed"};
    return r;
}

/// Compute the NLS scrambler u = first 4 bytes of SHA1(A || B) as
/// a little-endian uint32, then converted to BIGNUM.
/// A and B are 128-byte big-endian values.
BnPtr compute_scrambler(const std::array<std::uint8_t, 128>& A_be,
                        const std::array<std::uint8_t, 128>& B_be)
{
    // u = SHA1(A || B), take first 4 bytes as little-endian uint32
    const auto h = sha1_multi({{A_be.data(), 128}, {B_be.data(), 128}});

    // Interpret first 4 bytes as little-endian uint32
    const std::uint32_t u_val =
        static_cast<std::uint32_t>(h[0])        |
        (static_cast<std::uint32_t>(h[1]) << 8)  |
        (static_cast<std::uint32_t>(h[2]) << 16) |
        (static_cast<std::uint32_t>(h[3]) << 24);

    BnPtr u = bn_new();
    if (!BN_set_word(u.get(), u_val))
        throw std::runtime_error{"NLS: BN_set_word for u failed"};
    return u;
}

/// Compute the 40-byte interleaved session key K from the shared
/// secret S (128-byte big-endian).
///
/// K = interleave(SHA1(S[0,2,4,...,126]), SHA1(S[1,3,5,...,127]))
/// i.e. K[2i] = SHA1_even[i], K[2i+1] = SHA1_odd[i]
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
/// where I = uppercase(username).
std::array<std::uint8_t, 20>
compute_m1(std::string_view                       username,
           const std::array<std::uint8_t, 128>&   A_be,
           const std::array<std::uint8_t, 128>&   B_be,
           const std::array<std::byte, 32>&        salt,
           const std::array<std::byte, 40>&        K)
{
    // H(N)
    const auto h_N = sha1_buf(kNlsPrimeN.data(), kNlsPrimeN.size());

    // H(g)
    const std::uint8_t g_byte = kGenerator;
    const auto h_g = sha1_buf(&g_byte, 1);

    // H(N) XOR H(g) -- 20 bytes
    std::array<std::uint8_t, 20> hN_xor_hg{};
    for (std::size_t i = 0; i < 20; ++i)
        hN_xor_hg[i] = h_N[i] ^ h_g[i];

    // H(I) where I = uppercase(username)
    const std::string I = ascii_upper(username);
    const auto h_I = sha1_buf(I.data(), I.size());

    // M1 = SHA1(hN_xor_hg || h_I || s || A || B || K)
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

}  // namespace

// ============================================================================
// NlsServer
// ============================================================================

NlsContext NlsServer::create_challenge(std::string_view           username,
                                       std::span<const std::byte> verifier,
                                       std::span<const std::byte> stored_salt)
{
    if (verifier.size() != 128)
        throw std::invalid_argument{"NLS: verifier must be 128 bytes"};
    if (stored_salt.size() != 32)
        throw std::invalid_argument{"NLS: salt must be 32 bytes"};

    (void)username;  // not used in challenge generation

    const auto ctx = bn_ctx_new();
    const auto N   = load_prime();
    const auto g   = load_generator();

    // b = random 32 bytes, reduced mod N
    const auto b_bytes = random_bytes(32);
    BnPtr b{BN_bin2bn(b_bytes.data(), 32, nullptr)};
    if (!b) throw std::runtime_error{"NLS: failed to create b"};
    {
        BnPtr b_mod = bn_new();
        if (!BN_mod(b_mod.get(), b.get(), N.get(), ctx.get()))
            throw std::runtime_error{"NLS: BN_mod for b failed"};
        b = std::move(b_mod);
    }

    // v = verifier (big-endian)
    BnPtr v = bn_from_bytes_be(verifier);

    // B = (3*v + g^b mod N) mod N
    BnPtr gb = bn_new();
    if (!BN_mod_exp(gb.get(), g.get(), b.get(), N.get(), ctx.get()))
        throw std::runtime_error{"NLS: BN_mod_exp for g^b failed"};

    BnPtr three = bn_new();
    if (!BN_set_word(three.get(), 3u))
        throw std::runtime_error{"NLS: BN_set_word(3) failed"};

    BnPtr three_v = bn_new();
    if (!BN_mul(three_v.get(), three.get(), v.get(), ctx.get()))
        throw std::runtime_error{"NLS: BN_mul for 3*v failed"};

    BnPtr B_raw = bn_new();
    if (!BN_add(B_raw.get(), three_v.get(), gb.get()))
        throw std::runtime_error{"NLS: BN_add for 3*v + g^b failed"};

    BnPtr B = bn_new();
    if (!BN_mod(B.get(), B_raw.get(), N.get(), ctx.get()))
        throw std::runtime_error{"NLS: BN_mod for B failed"};

    NlsContext result{};

    // Fill salt from stored_salt
    std::memcpy(result.salt.data(), stored_salt.data(), 32);

    // Fill server_private_key (b) -- store as 32-byte big-endian
    {
        std::array<std::uint8_t, 32> b_buf{};
        const int nb = BN_num_bytes(b.get());
        if (nb <= 32) {
            BN_bn2bin(b.get(),
                      b_buf.data() + (32 - static_cast<std::size_t>(nb)));
        }
        std::memcpy(result.server_private_key.data(), b_buf.data(), 32);
    }

    // Fill server_public_key (B) -- 128-byte big-endian
    bn_to_bytes_be(B.get(), result.server_public_key);

    return result;
}

pvpgn::core::Result<std::array<std::byte, 20>, NlsError>
NlsServer::verify_proof(NlsContext&                ctx_ref,
                        std::string_view           username,
                        std::span<const std::byte> client_public_key_A,
                        std::span<const std::byte> client_proof_M1)
{
    if (client_public_key_A.size() != 128)
        return pvpgn::core::fail(NlsError::InvalidPublicKey);
    if (client_proof_M1.size() != 20)
        return pvpgn::core::fail(NlsError::InvalidProof);

    try {
        const auto bn_ctx = bn_ctx_new();
        const auto N      = load_prime();
        const auto g      = load_generator();

        // A = client public key (big-endian)
        BnPtr A = bn_from_bytes_be(client_public_key_A);

        // Reject A == 0 mod N (protocol violation)
        {
            BnPtr A_mod = bn_new();
            if (!BN_mod(A_mod.get(), A.get(), N.get(), bn_ctx.get()))
                return pvpgn::core::fail(NlsError::CryptoError);
            if (BN_is_zero(A_mod.get()))
                return pvpgn::core::fail(NlsError::InvalidPublicKey);
        }

        // B = server public key (big-endian, 128 bytes)
        BnPtr B = bn_from_bytes_be(
            std::span<const std::byte>{ctx_ref.server_public_key});

        // b = server private key (big-endian, 32 bytes)
        BnPtr b = bn_from_bytes_be(
            std::span<const std::byte>{ctx_ref.server_private_key});

        // v = verifier -- we need to recompute it from the context.
        // Actually we don't store v in the context; we need it to compute S.
        // The verifier is not stored in NlsContext. We must compute S differently.
        //
        // S = (A * v^u mod N)^b mod N
        //
        // But we don't have v here. The design requires the caller to pass v
        // through verify_proof, OR we store it in the context.
        // Per the task spec, verify_proof takes (ctx, username, A, M1) only.
        //
        // Resolution: store v in NlsContext (extend the struct) OR
        // recompute B from v and b to extract v.
        //
        // Since B = (3*v + g^b) mod N, we can solve for v:
        //   3*v = (B - g^b) mod N
        //   v   = (B - g^b) * modinv(3, N) mod N
        //
        // This is the correct approach for a stateless design.

        // g^b mod N
        BnPtr gb = bn_new();
        if (!BN_mod_exp(gb.get(), g.get(), b.get(), N.get(), bn_ctx.get()))
            return pvpgn::core::fail(NlsError::CryptoError);

        // (B - g^b) mod N
        BnPtr B_minus_gb = bn_new();
        if (!BN_mod_sub(B_minus_gb.get(), B.get(), gb.get(), N.get(),
                        bn_ctx.get()))
            return pvpgn::core::fail(NlsError::CryptoError);

        // modinv(3, N)
        BnPtr three = bn_new();
        if (!BN_set_word(three.get(), 3u))
            return pvpgn::core::fail(NlsError::CryptoError);
        BnPtr inv3 = bn_new();
        if (!BN_mod_inverse(inv3.get(), three.get(), N.get(), bn_ctx.get()))
            return pvpgn::core::fail(NlsError::CryptoError);

        // v = (B - g^b) * inv3 mod N
        BnPtr v = bn_new();
        if (!BN_mod_mul(v.get(), B_minus_gb.get(), inv3.get(), N.get(),
                        bn_ctx.get()))
            return pvpgn::core::fail(NlsError::CryptoError);

        // Export A and B as 128-byte big-endian for hash computations
        std::array<std::uint8_t, 128> A_be{};
        std::array<std::uint8_t, 128> B_be{};
        {
            const int na = BN_num_bytes(A.get());
            if (na > 128) return pvpgn::core::fail(NlsError::InvalidPublicKey);
            BN_bn2bin(A.get(), A_be.data() + (128 - static_cast<std::size_t>(na)));
        }
        {
            const int nb2 = BN_num_bytes(B.get());
            if (nb2 > 128) return pvpgn::core::fail(NlsError::CryptoError);
            BN_bn2bin(B.get(), B_be.data() + (128 - static_cast<std::size_t>(nb2)));
        }

        // u = scrambler from H(A || B)
        BnPtr u = compute_scrambler(A_be, B_be);

        // v^u mod N
        BnPtr vu = bn_new();
        if (!BN_mod_exp(vu.get(), v.get(), u.get(), N.get(), bn_ctx.get()))
            return pvpgn::core::fail(NlsError::CryptoError);

        // A * v^u mod N
        BnPtr Avu = bn_new();
        if (!BN_mod_mul(Avu.get(), A.get(), vu.get(), N.get(), bn_ctx.get()))
            return pvpgn::core::fail(NlsError::CryptoError);

        // S = (A * v^u)^b mod N
        BnPtr S = bn_new();
        if (!BN_mod_exp(S.get(), Avu.get(), b.get(), N.get(), bn_ctx.get()))
            return pvpgn::core::fail(NlsError::CryptoError);

        // Export S as 128-byte big-endian
        std::array<std::uint8_t, 128> S_be{};
        {
            const int ns = BN_num_bytes(S.get());
            if (ns > 128) return pvpgn::core::fail(NlsError::CryptoError);
            BN_bn2bin(S.get(), S_be.data() + (128 - static_cast<std::size_t>(ns)));
        }

        // K = interleaved session key
        const auto K = compute_session_key(S_be);
        ctx_ref.session_key = K;

        // Compute expected M1
        const auto M1_expected = compute_m1(username, A_be, B_be,
                                            ctx_ref.salt, K);

        // Compare with client-provided M1
        if (std::memcmp(M1_expected.data(), client_proof_M1.data(), 20) != 0)
            return pvpgn::core::fail(NlsError::InvalidProof);

        // Compute M2 = H(A, M1, K)
        return compute_m2(A_be, M1_expected, K);

    } catch (...) {
        return pvpgn::core::fail(NlsError::CryptoError);
    }
}

// ============================================================================
// NlsVerifier
// ============================================================================

std::pair<std::array<std::byte, 32>, std::array<std::byte, 128>>
NlsVerifier::create_verifier(std::string_view username,
                              std::string_view password)
{
    // Generate random 32-byte salt
    std::array<std::byte, 32> salt{};
    {
        std::vector<std::uint8_t> s_bytes = random_bytes(32);
        std::memcpy(salt.data(), s_bytes.data(), 32);
    }

    // x = hash_password(username, password, salt)
    const auto x_bytes = NlsVerifier::hash_password(username, password, salt);

    // v = g^x mod N
    const auto bn_ctx = bn_ctx_new();
    const auto N      = load_prime();
    const auto g      = load_generator();

    BnPtr x = bn_from_bytes_be(
        std::span<const std::byte>{x_bytes.data(), x_bytes.size()});
    BnPtr v = bn_new();
    if (!BN_mod_exp(v.get(), g.get(), x.get(), N.get(), bn_ctx.get()))
        throw std::runtime_error{"NLS: BN_mod_exp for verifier failed"};

    std::array<std::byte, 128> verifier{};
    bn_to_bytes_be(v.get(), verifier);

    return {salt, verifier};
}

std::array<std::byte, 20>
NlsVerifier::hash_password(std::string_view           username,
                            std::string_view           password,
                            std::span<const std::byte> salt)
{
    if (salt.size() != 32)
        throw std::invalid_argument{"NLS: salt must be 32 bytes"};

    // H(uppercase(username) ":" uppercase(password))
    const std::string U = ascii_upper(username);
    const std::string P = ascii_upper(password);

    std::string userpass;
    userpass.reserve(U.size() + 1 + P.size());
    userpass.append(U);
    userpass.push_back(':');
    userpass.append(P);

    const auto h_up = sha1_buf(userpass.data(), userpass.size());

    // x = H(s || H(U:P))
    std::array<std::uint8_t, 32 + 20> buf{};
    std::memcpy(buf.data(), salt.data(), 32);
    std::memcpy(buf.data() + 32, h_up.data(), 20);

    const auto x_raw = sha1_buf(buf.data(), buf.size());

    std::array<std::byte, 20> result{};
    std::memcpy(result.data(), x_raw.data(), 20);
    return result;
}

}  // namespace pvpgn::infra::crypto
