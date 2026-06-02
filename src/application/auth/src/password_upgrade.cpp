// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/password_upgrade.hpp"

namespace pvpgn::application::auth {

PasswordVerification
PasswordUpgrade::verify(std::string_view stored_encoded,
                        std::string_view plaintext) const {
    PasswordVerification out;

    // 1. Constant-time verification (delegated to the hasher).
    if (!hasher_.verify(plaintext, stored_encoded)) {
        return out;  // verified == false; never persist on a mismatch.
    }
    out.verified = true;

    // 2. Transparent upgrade: if the stored hash was produced by a superseded
    //    algorithm or weaker-than-policy parameters, re-hash the (now-verified)
    //    plaintext so the caller can persist the stronger form.
    if (hasher_.needs_rehash(stored_encoded)) {
        out.upgraded_hash = hasher_.hash(plaintext);
    }

    return out;
}

}  // namespace pvpgn::application::auth
