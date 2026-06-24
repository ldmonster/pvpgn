// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridge for d2dbs charlock init/destroy.

#include "app/d2dbs/legacy_d2dbs_bridges/charlock_bridge.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

#include "core/logging.hpp"
#include "app/d2dbs/legacy_d2dbs_bridges/bridge_logger.hpp"

namespace pld = pvpgn::integration::legacy_d2dbs;

namespace {

// Render an unsigned into a fixed buffer; returns a string_view that
// remains valid for the buffer's lifetime.
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

extern "C" int pvpgn_v3_d2dbs_charlock_init(
    unsigned int tbllen, unsigned int maxgs) noexcept {
    std::array<char, 20> tbuf{};
    std::array<char, 20> mbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"tbllen", render_uint(tbuf, tbllen)},
        {"maxgs",  render_uint(mbuf, maxgs)},
    };
    pld::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2dbs_charlock_bridge",
        "charlock init observed",
        {fields[0], fields[1]});
    return 0;
}

extern "C" int pvpgn_v3_d2dbs_charlock_destroy(void) noexcept {
    pld::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2dbs_charlock_bridge",
        "charlock destroy observed",
        {});
    return 0;
}
