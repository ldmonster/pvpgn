// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/logging.hpp"

#include <atomic>
#include <memory>

namespace pvpgn::core {

namespace {

std::shared_ptr<ILogger>& global_slot() {
    // Default: NullLogger so library users opt-in to noise.
    static std::shared_ptr<ILogger> instance = std::make_shared<NullLogger>();
    return instance;
}

}  // namespace

ILogger& default_logger() noexcept {
    return *global_slot();
}

void set_default_logger(std::shared_ptr<ILogger> logger) noexcept {
    if (logger) global_slot() = std::move(logger);
}

}  // namespace pvpgn::core
