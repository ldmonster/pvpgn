// SPDX-License-Identifier: GPL-2.0-or-later
//
// SRP-3 authentication as used by Battle.net (Warcraft III). Port
// of `src/common/bnetsrp3.{h,cpp}`. Bit-for-bit compatible with the
// legacy `pvpgn::BnetSRP3`; verified by parity tests.
//
// Differences from the legacy class:
//   * RAII / value semantics (no raw `new`/`xmalloc`).
//   * Random sources are injected via setters so the class is
//     deterministic in tests.
//   * `username`/`password` are owned `std::string`.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "infra/crypto/big_uint.hpp"

namespace pvpgn::v3::infra::crypto {

class BnetSrp3 {
public:
    // The 32-byte modulus, generator and constant `I = H(g) xor H(N)`
    // used by the Battle.net SRP-3 implementation. Exposed as static
    // accessors so callers / tests can compare against legacy.
    static const BigUInt& N();
    static const BigUInt& g();
    static const BigUInt& I();

    // Wire form (32-byte big-endian little-endian-block, i.e. the
    // legacy `getData(buf,32,4,false)` format) of N -- mostly for
    // tests.
    [[nodiscard]] static std::array<std::uint8_t, 32> n_wire();

    // Server-side construction: caller already knows the salt for
    // this account. A random `b` (server session private key) is
    // generated -- override with `set_server_private_key()` for
    // deterministic testing.
    BnetSrp3(std::string_view username, const BigUInt& salt);

    // Client-side construction: account creation / fresh login.
    // Random `a` (client session private key) and `s` (salt) are
    // generated -- override with `set_client_private_key()` and
    // `set_salt()` for determinism.
    BnetSrp3(std::string_view username, std::string_view password);

    // ----- public protocol API (matches legacy) -----
    [[nodiscard]] BigUInt verifier() const;
    [[nodiscard]] BigUInt salt() const;
    [[nodiscard]] BigUInt client_session_public_key() const;
    [[nodiscard]] BigUInt server_session_public_key(const BigUInt& v);
    [[nodiscard]] BigUInt hashed_client_secret(const BigUInt& B) const;
    [[nodiscard]] BigUInt hashed_server_secret(const BigUInt& A,
                                               const BigUInt& v);
    [[nodiscard]] BigUInt client_password_proof(const BigUInt& A,
                                                const BigUInt& B,
                                                const BigUInt& K) const;
    [[nodiscard]] BigUInt server_password_proof(const BigUInt& A,
                                                const BigUInt& M,
                                                const BigUInt& K) const;

    // ----- determinism hooks (only intended for tests) -----
    void set_salt(const BigUInt& s);
    void set_client_private_key(const BigUInt& a);
    void set_server_private_key(const BigUInt& b);

    // ----- accessors for tests -----
    [[nodiscard]] const std::string& username() const noexcept
    {
        return username_;
    }
    [[nodiscard]] bool has_password() const noexcept
    {
        return !password_.empty();
    }

private:
    // Static-initialisation-order-safe accessors backed by Meyers
    // singletons in the .cpp.
    [[nodiscard]] BigUInt client_private_key() const;
    [[nodiscard]] BigUInt scrambler(const BigUInt& B) const;
    [[nodiscard]] BigUInt client_secret(const BigUInt& B) const;
    [[nodiscard]] BigUInt server_secret(const BigUInt& A, const BigUInt& v);
    [[nodiscard]] BigUInt hash_secret(const BigUInt& secret) const;

    void refresh_raw_salt();

    std::string                  username_;
    std::string                  password_;  // empty on server side
    BigUInt                      a_;         // client session private key
    BigUInt                      b_;         // server session private key
    BigUInt                      s_;         // salt
    std::optional<BigUInt>       B_cache_;
    std::array<std::uint8_t, 32> raw_salt_{};
};

}  // namespace pvpgn::v3::infra::crypto
