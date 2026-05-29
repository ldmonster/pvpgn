// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file script_sandbox.hpp
/// Script sandbox port.
///
/// Controls resource limits applied to script execution (CPU instructions,
/// memory, wall-clock timeout). Implementations live in `infra/` and are
/// wired by the composition root alongside IScriptHost.

#include <cstdint>

namespace pvpgn::application::ports {

/// Resource limits for a single script call.
struct SandboxLimits {
    std::uint32_t max_instructions{0};  ///< CPU instruction limit per call (0 = unlimited)
    std::uint32_t max_memory_kb{0};     ///< Memory limit in KB (0 = unlimited)
    std::uint32_t timeout_ms{0};        ///< Wall-clock timeout per call in ms (0 = unlimited)
};

/// Port: script sandbox interface.
class IScriptSandbox {
public:
    virtual ~IScriptSandbox() = default;

    /// Apply resource limits to the script host for subsequent calls.
    virtual void set_limits(SandboxLimits limits) noexcept = 0;

    /// Get the currently active resource limits.
    virtual SandboxLimits limits() const noexcept = 0;

    /// Reset to unlimited (for trusted admin scripts).
    virtual void reset_limits() noexcept = 0;
};

} // namespace pvpgn::application::ports
