// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_INTEGRATION_LEGACY_BNETD_AD_DISPATCH_BRIDGE_HPP
#define PVPGN_INTEGRATION_LEGACY_BNETD_AD_DISPATCH_BRIDGE_HPP

extern "C" int pvpgn_v3_ad_dispatch_try(void* conn_ptr,
                                        char const* op) noexcept;

#endif
