// SPDX-License-Identifier: GPL-2.0-or-later
// Auto-split from codec.cpp by scripts/dev/split_codec.py
// See plans/15-large-file-decomposition-detail.md §8 for rationale.
#include "codec_internal.h"

namespace pvpgn::protocol::bnet {
namespace detail {

core::Result<CreateAccount2Request> decode_createaccount2_request(const Packet& pkt) {
    Reader r{pkt.payload};
    CreateAccount2Request m;
    {
        auto v = r.read_bytes(m.salt.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.salt.size(); ++i)
            m.salt[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    {
        auto v = r.read_bytes(m.password_verifier.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.password_verifier.size(); ++i)
            m.password_verifier[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    RD_STR(m.account_name);
    return m;
}

core::Result<CreateAccount2Reply> decode_createaccount2_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    CreateAccount2Reply m;
    RD_U32(m.result);
    return m;
}

// --- SID_LOGINREQ_W3 / SID_LOGINREPLY_W3 (0x53) -------------------------

core::Result<LoginW3Request> decode_loginw3_request(const Packet& pkt) {
    Reader r{pkt.payload};
    LoginW3Request m;
    {
        auto v = r.read_bytes(m.client_public_key.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.client_public_key.size(); ++i)
            m.client_public_key[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    RD_STR(m.account_name);
    return m;
}

core::Result<LoginW3Reply> decode_loginw3_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    LoginW3Reply m;
    RD_U32(m.message);
    {
        auto v = r.read_bytes(m.salt.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.salt.size(); ++i)
            m.salt[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    {
        auto v = r.read_bytes(m.server_public_key.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.server_public_key.size(); ++i)
            m.server_public_key[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    return m;
}

// --- SID_LOGONPROOFREQ / SID_LOGONPROOFREPLY (0x54) ---------------------

core::Result<LogonProofW3Request> decode_logonproof_w3_request(const Packet& pkt) {
    Reader r{pkt.payload};
    LogonProofW3Request m;
    auto v = r.read_bytes(m.client_password_proof.size());
    if (!v) return core::fail(v.error());
    for (std::size_t i = 0; i < m.client_password_proof.size(); ++i)
        m.client_password_proof[i] = static_cast<std::uint8_t>(v.value()[i]);
    return m;
}

core::Result<LogonProofW3Reply> decode_logonproof_w3_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    LogonProofW3Reply m;
    RD_U32(m.response);
    auto v = r.read_bytes(m.server_password_proof.size());
    if (!v) return core::fail(v.error());
    for (std::size_t i = 0; i < m.server_password_proof.size(); ++i)
        m.server_password_proof[i] = static_cast<std::uint8_t>(v.value()[i]);
    if (!r.empty()) {
        RD_STR(m.message);
    }
    return m;
}

// --- SID_PASSCHANGEREQ / SID_PASSCHANGEREPLY (0x55) ---------------------

core::Result<PassChangeRequest> decode_passchange_request(const Packet& pkt) {
    Reader r{pkt.payload};
    PassChangeRequest m;
    auto v = r.read_bytes(m.client_public_key.size());
    if (!v) return core::fail(v.error());
    for (std::size_t i = 0; i < m.client_public_key.size(); ++i)
        m.client_public_key[i] = static_cast<std::uint8_t>(v.value()[i]);
    RD_STR(m.account_name);
    return m;
}

core::Result<PassChangeReply> decode_passchange_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    PassChangeReply m;
    RD_U32(m.message);
    {
        auto v = r.read_bytes(m.salt.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.salt.size(); ++i)
            m.salt[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    {
        auto v = r.read_bytes(m.server_public_key.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.server_public_key.size(); ++i)
            m.server_public_key[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    return m;
}

// --- SID_PASSCHANGEPROOFREQ / SID_PASSCHANGEPROOFREPLY (0x56) ----------

core::Result<PassChangeProofRequest> decode_passchange_proof_request(const Packet& pkt) {
    Reader r{pkt.payload};
    PassChangeProofRequest m;
    {
        auto v = r.read_bytes(m.client_password_proof.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.client_password_proof.size(); ++i)
            m.client_password_proof[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    {
        auto v = r.read_bytes(m.salt.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.salt.size(); ++i)
            m.salt[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    {
        auto v = r.read_bytes(m.password_verifier.size());
        if (!v) return core::fail(v.error());
        for (std::size_t i = 0; i < m.password_verifier.size(); ++i)
            m.password_verifier[i] = static_cast<std::uint8_t>(v.value()[i]);
    }
    return m;
}

core::Result<PassChangeProofReply> decode_passchange_proof_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    PassChangeProofReply m;
    RD_U32(m.response);
    auto v = r.read_bytes(m.server_password_proof.size());
    if (!v) return core::fail(v.error());
    for (std::size_t i = 0; i < m.server_password_proof.size(); ++i)
        m.server_password_proof[i] = static_cast<std::uint8_t>(v.value()[i]);
    return m;
}

// --- SID_CLANINFO (0x82) ---------------------------------------------------

core::Status<> read_strings(Reader& r, std::vector<std::string>& out,
                            std::uint32_t count) {
    constexpr std::uint32_t kLimit = 256u;  // defensive
    if (count > kLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: userdata count exceeds limit"});
    }
    out.clear();
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        out.emplace_back(s.value());
    }
    return core::ok();
}

core::Result<UserDataReadRequest> decode_userdata_read_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    UserDataReadRequest m;
    std::uint32_t name_count = 0;
    std::uint32_t key_count  = 0;
    RD_U32(name_count);
    RD_U32(key_count);
    RD_U32(m.request_id);
    auto s = read_strings(r, m.names, name_count);
    if (!s) return core::fail(s.error());
    s = read_strings(r, m.keys, key_count);
    if (!s) return core::fail(s.error());
    return m;
}

core::Result<UserDataReadReply> decode_userdata_read_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    UserDataReadReply m;
    RD_U32(m.name_count);
    RD_U32(m.key_count);
    RD_U32(m.request_id);
    // name_count * key_count must fit in u32 and stay below a defensive cap.
    constexpr std::uint64_t kCellLimit = 4096u;
    const std::uint64_t cells =
        static_cast<std::uint64_t>(m.name_count) *
        static_cast<std::uint64_t>(m.key_count);
    if (cells > kCellLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: userdata read reply cell count exceeds limit"});
    }
    auto s = read_strings(r, m.values, static_cast<std::uint32_t>(cells));
    if (!s) return core::fail(s.error());
    return m;
}

core::Result<UserDataWriteRequest> decode_userdata_write_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    UserDataWriteRequest m;
    std::uint32_t name_count = 0;
    std::uint32_t key_count  = 0;
    RD_U32(name_count);
    RD_U32(key_count);
    auto s = read_strings(r, m.names, name_count);
    if (!s) return core::fail(s.error());
    s = read_strings(r, m.keys, key_count);
    if (!s) return core::fail(s.error());
    constexpr std::uint64_t kCellLimit = 4096u;
    const std::uint64_t cells =
        static_cast<std::uint64_t>(name_count) *
        static_cast<std::uint64_t>(key_count);
    if (cells > kCellLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: userdata write request cell count exceeds limit"});
    }
    s = read_strings(r, m.values, static_cast<std::uint32_t>(cells));
    if (!s) return core::fail(s.error());
    return m;
}

// --- SID_CLAN_CREATE family (0x70..0x7C) ----------------------------------


} // namespace detail

core::Status<> encode(Writer& w, const CreateAccount2Request& m) {
    w.begin_bnet_packet(kSidCreateAccount2);
    for (auto b : m.salt)              w.write_le<std::uint8_t>(b);
    for (auto b : m.password_verifier) w.write_le<std::uint8_t>(b);
    w.write_cstring(m.account_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const CreateAccount2Reply& m) {
    w.begin_bnet_packet(kSidCreateAccount2);
    w.write_le<std::uint32_t>(m.result);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LoginW3Request& m) {
    w.begin_bnet_packet(kSidLoginW3);
    for (auto b : m.client_public_key) w.write_le<std::uint8_t>(b);
    w.write_cstring(m.account_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LoginW3Reply& m) {
    w.begin_bnet_packet(kSidLoginW3);
    w.write_le<std::uint32_t>(m.message);
    for (auto b : m.salt)              w.write_le<std::uint8_t>(b);
    for (auto b : m.server_public_key) w.write_le<std::uint8_t>(b);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LogonProofW3Request& m) {
    w.begin_bnet_packet(kSidLogonProofW3);
    for (auto b : m.client_password_proof) w.write_le<std::uint8_t>(b);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LogonProofW3Reply& m) {
    w.begin_bnet_packet(kSidLogonProofW3);
    w.write_le<std::uint32_t>(m.response);
    for (auto b : m.server_password_proof) w.write_le<std::uint8_t>(b);
    if (!m.message.empty()) {
        w.write_cstring(m.message);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const PassChangeRequest& m) {
    w.begin_bnet_packet(kSidPassChange);
    for (auto b : m.client_public_key) w.write_le<std::uint8_t>(b);
    w.write_cstring(m.account_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const PassChangeReply& m) {
    w.begin_bnet_packet(kSidPassChange);
    w.write_le<std::uint32_t>(m.message);
    for (auto b : m.salt)              w.write_le<std::uint8_t>(b);
    for (auto b : m.server_public_key) w.write_le<std::uint8_t>(b);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const PassChangeProofRequest& m) {
    w.begin_bnet_packet(kSidPassChangeProof);
    for (auto b : m.client_password_proof) w.write_le<std::uint8_t>(b);
    for (auto b : m.salt)                  w.write_le<std::uint8_t>(b);
    for (auto b : m.password_verifier)     w.write_le<std::uint8_t>(b);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const PassChangeProofReply& m) {
    w.begin_bnet_packet(kSidPassChangeProof);
    w.write_le<std::uint32_t>(m.response);
    for (auto b : m.server_password_proof) w.write_le<std::uint8_t>(b);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const UserDataReadRequest& m) {
    w.begin_bnet_packet(kSidReadUserData);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.names.size()));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.keys.size()));
    w.write_le<std::uint32_t>(m.request_id);
    for (const auto& s : m.names) w.write_cstring(s);
    for (const auto& s : m.keys)  w.write_cstring(s);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const UserDataReadReply& m) {
    w.begin_bnet_packet(kSidReadUserData);
    w.write_le<std::uint32_t>(m.name_count);
    w.write_le<std::uint32_t>(m.key_count);
    w.write_le<std::uint32_t>(m.request_id);
    for (const auto& s : m.values) w.write_cstring(s);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const UserDataWriteRequest& m) {
    w.begin_bnet_packet(kSidWriteUserData);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.names.size()));
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.keys.size()));
    for (const auto& s : m.names)  w.write_cstring(s);
    for (const auto& s : m.keys)   w.write_cstring(s);
    for (const auto& s : m.values) w.write_cstring(s);
    return w.finalize_bnet_packet();
}


} // namespace pvpgn::protocol::bnet
