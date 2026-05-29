// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file serverqueue_bridge.hpp
/// R236(2) -- observation-only strangler-fig bridges for the legacy
/// server-queue lifecycle in `src/d2cs/serverqueue.cpp`:
///
///   * `sqlist_create()`  -- builds the s2s server-queue at startup.
///   * `sqlist_destroy()` -- tears the s2s server-queue down at
///                           shutdown.
///
/// Contract: always return 0 -- legacy MUST fall through and run the
/// real body.

extern "C" int pvpgn_v3_d2cs_sqlist_create_try(void)  noexcept;
extern "C" int pvpgn_v3_d2cs_sqlist_destroy_try(void) noexcept;
