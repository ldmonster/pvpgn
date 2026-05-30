// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/crypto/bnet_srp3.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>

#include "infra/crypto/bnet_hash.hpp"

namespace pvpgn::v3::infra::crypto {

namespace {

// Wire bytes of the SRP-3 modulus (Warcraft III); same as legacy
// `bnetsrp3_N`.
constexpr std::array<std::uint8_t, 32> kModulusBytes{
    0xF8, 0xFF, 0x1A, 0x8B, 0x61, 0x99, 0x18, 0x03,
    0x21, 0x86, 0xB6, 0x8C, 0xA0, 0x92, 0xB5, 0x55,
    0x7E, 0x97, 0x6C, 0x78, 0xC7, 0x32, 0x12, 0xD9,
    0x12, 0x16, 0xF6, 0x65, 0x85, 0x23, 0xC7, 0x87};

constexpr std::array<std::uint8_t, 20> kIBytes{
    0xF8, 0x01, 0x8C, 0xF0, 0xA4, 0x25, 0xBA, 0x8B,
    0xEB, 0x89, 0x58, 0xB1, 0xAB, 0x6B, 0xF9, 0x0A,
    0xED, 0x97, 0x0E, 0x6C};

constexpr std::uint8_t kGenerator = 0x2F;

const BigUInt& modulus_singleton()
{
    // Legacy: `BigInt(bnetsrp3_N, 32)` uses the default ctor args
    // `(blockSize=1, bigEndian=true)` -- i.e. pure big-endian.
    static const BigUInt v =
        BigUInt::from_bytes_legacy(kModulusBytes, 1, true);
    return v;
}

const BigUInt& generator_singleton()
{
    static const BigUInt v{static_cast<std::uint32_t>(kGenerator)};
    return v;
}

const BigUInt& i_singleton()
{
    // Legacy: `BigInt(bnetsrp3_I, 32)` uses default ctor args
    // `(blockSize=1, bigEndian=true)` -- pure big-endian.
    static const BigUInt v =
        BigUInt::from_bytes_legacy(kIBytes, 1, true);
    return v;
}

// Toupper that matches legacy `safe_toupper` for ASCII inputs. For
// >0x7f bytes the legacy implementation returns the byte unchanged.
std::string ascii_upper(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x80u)
            out.push_back(static_cast<char>(
                std::toupper(static_cast<int>(c))));
        else
            out.push_back(static_cast<char>(c));
    }
    return out;
}

// Generate a 32-byte uniformly random BigUInt. Random bytes are
// drawn from `std::random_device`; the bytes are interpreted as a
// little-endian-block (SRP-3 wire) BigUInt so the value is < 2^256.
BigUInt random_32_byte_biguint()
{
    std::random_device rd;
    std::array<std::uint8_t, 32> bytes{};
    for (auto& b : bytes) {
        b = static_cast<std::uint8_t>(rd() & 0xffu);
    }
    return BigUInt::from_bytes_legacy(bytes, 4, false);
}

// SHA-1 over a contiguous byte buffer (as `std::byte` is what the
// hash API uses, this small helper hides the cast).
BnetDigest sha1_bytes(std::span<const std::uint8_t> data)
{
    auto bytes = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(data.data()), data.size()};
    return sha1(bytes);
}

BnetDigest sha1_le_bytes(std::span<const std::uint8_t> data)
{
    auto bytes = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(data.data()), data.size()};
    return sha1_le(bytes);
}

// Pack a BnetDigest (5 host-order uint32) into 20 wire bytes the
// same way as the legacy `t_hash` byte layout: each uint32 is
// stored in native byte order in memory. Match by reinterpreting.
std::array<std::uint8_t, 20> digest_to_bytes(const BnetDigest& d)
{
    std::array<std::uint8_t, 20> out{};
    std::memcpy(out.data(), d.data(), 20);
    return out;
}

}  // namespace

// ----- static accessors -------------------------------------------------

const BigUInt& BnetSrp3::N() { return modulus_singleton(); }
const BigUInt& BnetSrp3::g() { return generator_singleton(); }
const BigUInt& BnetSrp3::I() { return i_singleton(); }

std::array<std::uint8_t, 32> BnetSrp3::n_wire()
{
    std::array<std::uint8_t, 32> out{};
    // Pure big-endian, matching how N was constructed.
    modulus_singleton().to_bytes_legacy(out, 1, true);
    return out;
}

// ----- construction -----------------------------------------------------

BnetSrp3::BnetSrp3(std::string_view username, const BigUInt& salt)
    : username_{ascii_upper(username)},
      password_{},
      a_{std::uint32_t{0u}},
      b_{random_32_byte_biguint() % modulus_singleton()},
      s_{salt}
{
    refresh_raw_salt();
}

BnetSrp3::BnetSrp3(std::string_view username, std::string_view password)
    : username_{ascii_upper(username)},
      password_{ascii_upper(password)},
      a_{random_32_byte_biguint() % modulus_singleton()},
      b_{std::uint32_t{0u}},
      s_{random_32_byte_biguint()}
{
    if (password_.empty()) {
        throw std::invalid_argument{"BnetSrp3: empty password"};
    }
    refresh_raw_salt();
}

void BnetSrp3::set_salt(const BigUInt& s)
{
    s_ = s;
    B_cache_.reset();
    refresh_raw_salt();
}

void BnetSrp3::set_client_private_key(const BigUInt& a)
{
    a_ = a;
    B_cache_.reset();
}

void BnetSrp3::set_server_private_key(const BigUInt& b)
{
    b_ = b;
    B_cache_.reset();
}

void BnetSrp3::refresh_raw_salt()
{
    // legacy: `s.getData(raw_salt, 32)` -- default blockSize=1,
    // bigEndian=true (pure BE).
    s_.to_bytes_legacy(raw_salt_, 1, true);
}

// ----- private helpers --------------------------------------------------

BigUInt BnetSrp3::client_private_key() const
{
    // x = H( s || H(username:password) )
    // First inner hash: little-endian SHA-1 of "USERNAME:PASSWORD"
    std::string userpass;
    userpass.reserve(username_.size() + 1 + password_.size());
    userpass.append(username_);
    userpass.push_back(':');
    userpass.append(password_);
    const auto userpass_hash = sha1_le_bytes(std::span<const std::uint8_t>{
        reinterpret_cast<const std::uint8_t*>(userpass.data()),
        userpass.size()});
    const auto userpass_bytes = digest_to_bytes(userpass_hash);

    std::array<std::uint8_t, 32 + 20> private_value{};
    std::memcpy(private_value.data(), raw_salt_.data(), 32);
    std::memcpy(private_value.data() + 32, userpass_bytes.data(), 20);

    const auto private_hash = sha1_le_bytes(private_value);
    const auto private_bytes = digest_to_bytes(private_hash);

    // Legacy returns BigInt(private_value_hash, 20, 1, false)
    return BigUInt::from_bytes_legacy(private_bytes, 1, false);
}

BigUInt BnetSrp3::scrambler(const BigUInt& B) const
{
    // scrambler = first 32 bits of SHA1(raw_B), interpreted as a
    // host-byte-order uint32 (legacy `*(uint32_t*)hash`).
    std::array<std::uint8_t, 32> raw_B{};
    B.to_bytes_legacy(raw_B, 4, false);
    const auto h = sha1_bytes(raw_B);
    // Legacy reads the first 4 bytes of `t_hash` as uint32. The
    // `t_hash` is `uint32[5]`, so the first uint32 in memory IS
    // h[0] in native byte order. BigInt(uint32_t) just stores the
    // value. We replicate that exactly:
    return BigUInt{static_cast<std::uint32_t>(h[0])};
}

BigUInt BnetSrp3::client_secret(const BigUInt& B) const
{
    const BigUInt x = client_private_key();
    const BigUInt u = scrambler(B);
    const BigUInt& N = modulus_singleton();
    const BigUInt& g = generator_singleton();
    // ((N + B - g^x) ^ (x*u + a)) mod N
    const BigUInt gx   = g.pow_mod(x, N);
    const BigUInt base = (N + B - gx) % N;
    const BigUInt exp  = (x * u) + a_;
    return base.pow_mod(exp, N);
}

BigUInt BnetSrp3::server_secret(const BigUInt& A, const BigUInt& v)
{
    const BigUInt B = server_session_public_key(v);
    const BigUInt u = scrambler(B);
    const BigUInt& N = modulus_singleton();
    const BigUInt base = (A * v.pow_mod(u, N)) % N;
    return base.pow_mod(b_, N);
}

BigUInt BnetSrp3::hash_secret(const BigUInt& secret) const
{
    std::array<std::uint8_t, 32> raw_secret{};
    secret.to_bytes_legacy(raw_secret, 4, false);

    std::array<std::uint8_t, 16> odd{};
    std::array<std::uint8_t, 16> even{};
    for (std::size_t i = 0; i < 16; ++i) {
        odd[i]  = raw_secret[i * 2];
        even[i] = raw_secret[i * 2 + 1];
    }
    const auto odd_h_bytes  = digest_to_bytes(sha1_le_bytes(odd));
    const auto even_h_bytes = digest_to_bytes(sha1_le_bytes(even));

    std::array<std::uint8_t, 40> hashed{};
    for (std::size_t i = 0; i < 20; ++i) {
        hashed[i * 2]     = odd_h_bytes[i];
        hashed[i * 2 + 1] = even_h_bytes[i];
    }
    return BigUInt::from_bytes_legacy(hashed, 1, false);
}

// ----- public API -------------------------------------------------------

BigUInt BnetSrp3::verifier() const
{
    return generator_singleton().pow_mod(client_private_key(),
                                         modulus_singleton());
}

BigUInt BnetSrp3::salt() const { return s_; }

BigUInt BnetSrp3::client_session_public_key() const
{
    return generator_singleton().pow_mod(a_, modulus_singleton());
}

BigUInt BnetSrp3::server_session_public_key(const BigUInt& v)
{
    if (!B_cache_.has_value()) {
        const BigUInt& N = modulus_singleton();
        const BigUInt& g = generator_singleton();
        B_cache_ = (v + g.pow_mod(b_, N)) % N;
    }
    return *B_cache_;
}

BigUInt BnetSrp3::hashed_client_secret(const BigUInt& B) const
{
    return hash_secret(client_secret(B));
}

BigUInt BnetSrp3::hashed_server_secret(const BigUInt& A, const BigUInt& v)
{
    return hash_secret(server_secret(A, v));
}

BigUInt BnetSrp3::client_password_proof(const BigUInt& A, const BigUInt& B,
                                       const BigUInt& K) const
{
    std::array<std::uint8_t, 176> proof_data{};
    const auto username_h = digest_to_bytes(sha1_le_bytes(
        std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(username_.data()),
            username_.size()}));

    // Layout matches legacy:
    //   [0..20)   = I              (20 bytes, blockSize=4)
    //   [20..40)  = H(username)    (20 bytes raw little-endian SHA-1)
    //   [40..72)  = s              (32 bytes, default blockSize=1)
    //   [72..104) = A              (32 bytes, blockSize=4)
    //   [104..136)= B              (32 bytes, blockSize=4)
    //   [136..176)= K              (40 bytes, blockSize=4)
    i_singleton().to_bytes_legacy(
        std::span<std::uint8_t>{proof_data.data() + 0, 20}, 4, false);
    std::memcpy(proof_data.data() + 20, username_h.data(), 20);
    // Legacy: `s.getData(&proofData[40], 32)` -- defaults
    // (blockSize=1, bigEndian=true).
    s_.to_bytes_legacy(
        std::span<std::uint8_t>{proof_data.data() + 40, 32}, 1, true);
    A.to_bytes_legacy(
        std::span<std::uint8_t>{proof_data.data() + 72, 32}, 4, false);
    B.to_bytes_legacy(
        std::span<std::uint8_t>{proof_data.data() + 104, 32}, 4, false);
    K.to_bytes_legacy(
        std::span<std::uint8_t>{proof_data.data() + 136, 40}, 4, false);

    const auto proof_h_bytes =
        digest_to_bytes(sha1_le_bytes(proof_data));
    return BigUInt::from_bytes_legacy(proof_h_bytes, 1, false);
}

BigUInt BnetSrp3::server_password_proof(const BigUInt& A, const BigUInt& M,
                                       const BigUInt& K) const
{
    std::array<std::uint8_t, 92> proof_data{};
    A.to_bytes_legacy(
        std::span<std::uint8_t>{proof_data.data() + 0, 32}, 4, false);
    M.to_bytes_legacy(
        std::span<std::uint8_t>{proof_data.data() + 32, 20}, 4, false);
    K.to_bytes_legacy(
        std::span<std::uint8_t>{proof_data.data() + 52, 40}, 4, false);

    const auto proof_h_bytes =
        digest_to_bytes(sha1_le_bytes(proof_data));
    return BigUInt::from_bytes_legacy(proof_h_bytes, 1, false);
}

}  // namespace pvpgn::v3::infra::crypto
