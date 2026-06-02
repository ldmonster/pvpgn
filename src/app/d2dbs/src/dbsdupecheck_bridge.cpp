// SPDX-License-Identifier: GPL-2.0-or-later
//
// R232(1): observation bridge for d2dbs dbsdupecheck per-call.

#include "app/d2dbs/legacy_d2dbs_bridges/dbsdupecheck_bridge.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

#include "core/logging.hpp"
#include "app/d2dbs/legacy_d2dbs_bridges/bridge_logger.hpp"

namespace pld = pvpgn::integration::legacy_d2dbs;

namespace {

std::string_view render_uint(std::array<char, 20>& buf,
                              unsigned int v) noexcept {
    auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (res.ec != std::errc{}) {
        return std::string_view{};
    }
    return std::string_view{buf.data(),
        static_cast<std::size_t>(res.ptr - buf.data())};
}

}  // namespace

extern "C" int pvpgn_v3_d2dbs_dupecheck(
    char const* data, unsigned int datalen) noexcept {
    std::array<char, 20> dbuf{};
    const char* data_state = (data != nullptr) ? "present" : "null";
    const pvpgn::core::ILogger::Field fields[] = {
        {"datalen", render_uint(dbuf, datalen)},
        {"data",    std::string_view{data_state}},
    };
    pld::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_d2dbs_dupecheck_bridge",
        "dupecheck call observed",
        {fields[0], fields[1]});
    return 0;
}
