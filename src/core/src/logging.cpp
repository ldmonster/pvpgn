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
    // A null argument *resets* the default to a NullLogger. The previous
    // `if (logger)` guard silently dropped resets, which left a dangling
    // default logger after its backing sink was destroyed (heap-use-after-free
    // caught by ASan in json_line_logger_composition_test). Always replace the
    // slot so `default_logger()` never outlives the sink a caller installed.
    global_slot() = logger ? std::move(logger)
                           : std::static_pointer_cast<ILogger>(
                                 std::make_shared<NullLogger>());
}

}  // namespace pvpgn::core
