// SPDX-License-Identifier: GPL-2.0-or-later
// R245: batch of bnetd small-module lifecycle / dispatch
// observation bridges (helpfile, autoupdate, output, support, mail).

#include "integration/legacy_bnetd/bnetd_lifecycle_bridges.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

namespace {

constexpr std::string_view kNull = "<null>";

std::string_view safe_str(const char* s) noexcept {
    if (!s) return kNull;
    return std::string_view{s};
}

std::string_view render_int(std::array<char, 24>& buf, int v) noexcept {
    auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (res.ec != std::errc{}) return std::string_view{};
    return std::string_view{buf.data(),
        static_cast<std::size_t>(res.ptr - buf.data())};
}

}  // namespace

// ---- helpfile -----------------------------------------------------

extern "C" int pvpgn_v3_bnetd_helpfile_init_try(
    const char* filename) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"filename", safe_str(filename)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_helpfile_bridge",
        "helpfile init observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_helpfile_unload_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_helpfile_bridge",
        "helpfile unload observed",
        {});
    return 0;
}

// ---- autoupdate ---------------------------------------------------

extern "C" int pvpgn_v3_bnetd_autoupdate_load_try(
    const char* filename) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"filename", safe_str(filename)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_autoupdate_bridge",
        "autoupdate load observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_autoupdate_unload_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_autoupdate_bridge",
        "autoupdate unload observed",
        {});
    return 0;
}

// ---- output -------------------------------------------------------

extern "C" int pvpgn_v3_bnetd_output_init_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_output_bridge",
        "output init observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_output_write_to_file_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_output_bridge",
        "output write_to_file observed",
        {});
    return 0;
}

// ---- support ------------------------------------------------------

extern "C" int pvpgn_v3_bnetd_support_check_files_try(
    const char* supportfile) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"supportfile", safe_str(supportfile)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_support_bridge",
        "support check_files observed",
        {fields[0]});
    return 0;
}

// ---- mail ---------------------------------------------------------

extern "C" int pvpgn_v3_bnetd_mail_handle_command_try(
    int sd,
    const char* text) noexcept {
    std::array<char, 24> sdbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd",   render_int(sdbuf, sd)},
        {"text", safe_str(text)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_mail_bridge",
        "mail handle_command observed",
        {fields[0], fields[1]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_mail_check_try(int sd) noexcept {
    std::array<char, 24> sdbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd", render_int(sdbuf, sd)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_bnetd_mail_bridge",
        "mail check observed",
        {fields[0]});
    return 0;
}
