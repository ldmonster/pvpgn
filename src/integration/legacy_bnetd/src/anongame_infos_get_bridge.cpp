// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridge for anongame_infos runtime data getters.

#include "integration/legacy_bnetd/anongame_infos_get_bridge.hpp"

#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

namespace {
std::string_view to_sv(char const* s) noexcept {
    return s ? std::string_view{s} : std::string_view{};
}
}  // namespace

extern "C" int pvpgn_v3_anongame_infos_get_try(char const* kind,
                                               char const* arg0,
                                               char const* arg1,
                                               char const* arg2) noexcept {
    std::string_view k = to_sv(kind);
    if (k.empty()) k = std::string_view{"?"};

    const pvpgn::core::ILogger::Field fields[] = {
        {"kind", k},
        {"arg0", to_sv(arg0)},
        {"arg1", to_sv(arg1)},
        {"arg2", to_sv(arg2)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_anongame_infos_get_bridge",
        "anongame_infos get observed",
        {fields[0], fields[1], fields[2], fields[3]});

    return 0;
}
