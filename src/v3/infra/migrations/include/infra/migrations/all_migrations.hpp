// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file all_migrations.hpp
/// Provides access to all database migrations in order.

#include <span>

#include "infra/migrations/migration_runner.hpp"

namespace pvpgn::infra::migrations {

/// Returns all registered migrations in ascending version order.
/// Migrations are embedded from .sql files at compile time.
std::span<const Migration> get_all_migrations();

}  // namespace pvpgn::infra::migrations
