// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_wire_types.hpp
/// Battle.net account-management wire constants: login (1, 2, W3),
/// logon-proof, password-change, account-creation (1, 2, W3),
/// change-password (legacy SC), change-game-port, UDPOK, email
/// management, UNKNOWN_2B. Mirrored from `src/common/bnet_protocol.h`.
/// Constants-only; structs deferred.

#include <cstdint>

namespace pvpgn::protocol::bnet::account {

// ---- Packet type codes -------------------------------------------------
inline constexpr std::uint16_t kClientCreateAcctReq1     = 0x2aff;
inline constexpr std::uint16_t kServerCreateAcctReply1   = 0x2aff;

inline constexpr std::uint16_t kClientUnknown2b          = 0x2bff;

inline constexpr std::uint16_t kClientLoginReq2          = 0x3aff;
inline constexpr std::uint16_t kServerLoginReply2        = 0x3aff;

inline constexpr std::uint16_t kClientLoginReqW3         = 0x53ff;
inline constexpr std::uint16_t kServerLoginReplyW3       = 0x53ff;

inline constexpr std::uint16_t kClientLogonProofReq      = 0x54ff;
inline constexpr std::uint16_t kServerLogonProofReply    = 0x54ff;

inline constexpr std::uint16_t kClientPassChangeReq      = 0x55ff;
inline constexpr std::uint16_t kServerPassChangeReply    = 0x55ff;

inline constexpr std::uint16_t kClientPassChangeProofReq = 0x56ff;
inline constexpr std::uint16_t kServerPassChangeProofReply = 0x56ff;

inline constexpr std::uint16_t kClientCreateAccountW3    = 0x52ff;
inline constexpr std::uint16_t kServerCreateAccountW3    = 0x52ff;

inline constexpr std::uint16_t kClientChangeGamePort     = 0x45ff;

inline constexpr std::uint16_t kClientCreateAcctReq2     = 0x3dff;
inline constexpr std::uint16_t kServerCreateAcctReply2   = 0x3dff;

inline constexpr std::uint16_t kClientUdpOk              = 0x14ff;

inline constexpr std::uint16_t kClientLoginReq1          = 0x29ff;
inline constexpr std::uint16_t kServerLoginReply1        = 0x29ff;

inline constexpr std::uint16_t kClientChangePassReq      = 0x31ff;
inline constexpr std::uint16_t kServerChangePassAck      = 0x31ff;

inline constexpr std::uint16_t kClientSetEmailReply      = 0x59ff;
inline constexpr std::uint16_t kServerSetEmailReq        = 0x59ff;
inline constexpr std::uint16_t kClientGetPasswordReq     = 0x5aff;
inline constexpr std::uint16_t kClientChangeEmailReq     = 0x5bff;

// ---- CreateAcctReply1 result codes ------------------------------------
inline constexpr std::uint32_t kCreateAcctReply1ResultOk = 0x00000001;
inline constexpr std::uint32_t kCreateAcctReply1ResultNo = 0x00000000;

// ---- LoginReply2 message codes ----------------------------------------
inline constexpr std::uint32_t kLoginReply2MessageSuccess  = 0x00000000;
inline constexpr std::uint32_t kLoginReply2MessageNonExist = 0x00000001;
inline constexpr std::uint32_t kLoginReply2MessageBadPass  = 0x00000002;
inline constexpr std::uint32_t kLoginReply2MessageLocked   = 0x00000006;

// ---- LoginReplyW3 message codes (note: BADACCT collides with ALREADY in legacy) -
inline constexpr std::uint32_t kLoginReplyW3MessageSuccess = 0x00000000;
inline constexpr std::uint32_t kLoginReplyW3MessageAlready = 0x00000001;
inline constexpr std::uint32_t kLoginReplyW3MessageBadAcct = 0x00000001;

// ---- LogonProofReply response codes -----------------------------------
inline constexpr std::uint32_t kLogonProofReplyResponseOk      = 0x00000000;
inline constexpr std::uint32_t kLogonProofReplyResponseBadPass = 0x00000002;
inline constexpr std::uint32_t kLogonProofReplyResponseEmail   = 0x0000000E;
inline constexpr std::uint32_t kLogonProofReplyResponseCustom  = 0x0000000F;

// ---- PassChangeReply message codes ------------------------------------
inline constexpr std::uint32_t kPassChangeReplyMessageAccept = 0x00000000;
inline constexpr std::uint32_t kPassChangeReplyMessageReject = 0x00000001;

// ---- PassChangeProofReply response codes ------------------------------
inline constexpr std::uint32_t kPassChangeProofReplyResponseOk      = 0x00000000;
inline constexpr std::uint32_t kPassChangeProofReplyResponseBadPass = 0x00000002;

// ---- CreateAccountW3 result codes -------------------------------------
inline constexpr std::uint32_t kCreateAccountW3ResultOk           = 0x00000000;
inline constexpr std::uint32_t kCreateAccountW3ResultExist        = 0x00000004;
inline constexpr std::uint32_t kCreateAccountW3ResultEmpty        = 0x00000007;
inline constexpr std::uint32_t kCreateAccountW3ResultInvalid      = 0x00000008;
inline constexpr std::uint32_t kCreateAccountW3ResultBanned       = 0x00000009;
inline constexpr std::uint32_t kCreateAccountW3ResultShort        = 0x0000000A;
inline constexpr std::uint32_t kCreateAccountW3ResultPunctuation  = 0x0000000B;
inline constexpr std::uint32_t kCreateAccountW3ResultPunctuation2 = 0x0000000C;

// ---- CreateAcctReply2 result codes ------------------------------------
inline constexpr std::uint32_t kCreateAcctReply2ResultOk                   = 0x00000000;
inline constexpr std::uint32_t kCreateAcctReply2ResultShort                = 0x00000001;
inline constexpr std::uint32_t kCreateAcctReply2ResultInvalid              = 0x00000002;
inline constexpr std::uint32_t kCreateAcctReply2ResultBanned               = 0x00000003;
inline constexpr std::uint32_t kCreateAcctReply2ResultExist                = 0x00000004;
inline constexpr std::uint32_t kCreateAcctReply2ResultLastCreateInProgress = 0x00000005;
inline constexpr std::uint32_t kCreateAcctReply2ResultAlphanum             = 0x00000006;
inline constexpr std::uint32_t kCreateAcctReply2ResultPunctuation          = 0x00000007;
inline constexpr std::uint32_t kCreateAcctReply2ResultPunctuation2         = 0x00000008;

} // namespace pvpgn::protocol::bnet::account
