// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for the account
// management dispatch family: SID_CREATEACCOUNT*,
// SID_CHANGEPASSWORD, SID_SETEMAIL reply and
// SID_CHANGEEMAIL. Coalesced behind a single entry point keyed
// by `op`. Always returns 0; null conn = no log.

extern "C" int pvpgn_v3_account_dispatch(void* conn_ptr,
                                             char const* op) noexcept;
