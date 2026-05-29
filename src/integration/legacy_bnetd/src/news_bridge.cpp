// SPDX-License-Identifier: GPL-2.0-or-later
//
// R230(2): observation bridge for news lifecycle.

#include "integration/legacy_bnetd/news_bridge.hpp"

#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

extern "C" int pvpgn_v3_news_load_try(char const* filename) noexcept {
    std::string_view name = (filename != nullptr)
                                ? std::string_view{filename}
                                : std::string_view{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"file", name},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_news_bridge",
        "news load observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_news_unload_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_news_bridge",
        "news unload observed",
        {});
    return 0;
}
