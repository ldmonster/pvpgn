// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// DEPRECATED: This header has been merged into bnetd_lifecycle_bridges.hpp
// as part of the Plan 06 lifecycle bridge collapse (quick wins, 2026-05-30).
// Include the unified header instead.
//
// This shim is kept so that any out-of-tree code that included the r247
// header directly continues to compile without modification.
// It will be deleted once the legacy bnetd target is retired.
//
// See: plans/16-strangler-completion-detail.md §5 "Can be done now"

#include "integration/legacy_bnetd/bnetd_lifecycle_bridges.hpp"
