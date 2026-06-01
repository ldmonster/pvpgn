// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file shadow_unit_of_work_factory.hpp
/// Factory for creating ShadowUnitOfWork instances.
///
/// Wraps two IUnitOfWorkFactory instances (primary and secondary). Each call
/// to create() produces a ShadowUnitOfWork that mirrors writes to both
/// backends when the shadow feature flag is enabled.

#include <memory>

#include "application/persistence/unit_of_work_factory.hpp"

namespace pvpgn::infra::shadow {

/// Factory for creating ShadowUnitOfWork instances.
///
/// When @p enabled is false, create() returns a UnitOfWork backed only by
/// the primary factory (pass-through mode). When @p enabled is true, each
/// created UnitOfWork mirrors writes to both primary and secondary.
///
/// Lifetime: both @p primary and @p secondary must outlive this factory.
class ShadowUnitOfWorkFactory final
    : public application::ports::IUnitOfWorkFactory {
public:
    /// @param primary   The authoritative backend factory.
    /// @param secondary The mirror backend factory.
    /// @param enabled   Feature flag — when false, behaves as a pass-through to primary.
    ShadowUnitOfWorkFactory(application::ports::IUnitOfWorkFactory& primary,
                            application::ports::IUnitOfWorkFactory& secondary,
                            bool enabled);

    /// Create a new ShadowUnitOfWork (or a primary-only UnitOfWork when disabled).
    [[nodiscard]] std::unique_ptr<application::ports::IUnitOfWork> create() override;

private:
    application::ports::IUnitOfWorkFactory& primary_;
    application::ports::IUnitOfWorkFactory& secondary_;
    bool enabled_;
};

}  // namespace pvpgn::infra::shadow
