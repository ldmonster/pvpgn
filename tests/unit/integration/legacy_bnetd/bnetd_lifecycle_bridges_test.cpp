// SPDX-License-Identifier: GPL-2.0-or-later
// R245 -- unit tests for the bnetd small-module lifecycle batch
// (helpfile, autoupdate, output, support, mail).

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bnetd_lifecycle_bridges.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct CapturedLog {
    pvpgn::core::LogLevel level{};
    std::string module;
    std::string message;
    std::vector<std::pair<std::string, std::string>> fields;
};

class RecordingLogger : public pvpgn::core::ILogger {
public:
    void log(pvpgn::core::LogLevel level,
             std::string_view module,
             std::string_view message) noexcept override {
        records.push_back(CapturedLog{
            level, std::string{module}, std::string{message}, {}});
    }
    pvpgn::core::LogLevel level() const noexcept override {
        return pvpgn::core::LogLevel::Trace;
    }
    void set_level(pvpgn::core::LogLevel) noexcept override {}
    void log_kv(pvpgn::core::LogLevel level,
                std::string_view module,
                std::string_view message,
                std::span<const Field> fields) noexcept override {
        CapturedLog c;
        c.level   = level;
        c.module  = std::string{module};
        c.message = std::string{message};
        for (const auto& f : fields) {
            c.fields.emplace_back(std::string{f.key},
                                  std::string{f.value});
        }
        records.push_back(std::move(c));
    }
    std::vector<CapturedLog> records;
};

std::string field_value(const CapturedLog& r,
                        std::string_view key) {
    for (const auto& kv : r.fields) {
        if (kv.first == key) return kv.second;
    }
    return {};
}

}  // namespace

// ---- helpfile -----------------------------------------------------

TEST_CASE("helpfile init bridge logs filename at Info",
          "[integration][legacy_bnetd][helpfile_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_helpfile_init_try("/etc/pvpgn/bnhelp.conf") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.module  == "v3_bnetd_helpfile_bridge");
    REQUIRE(r.message == "helpfile init observed");
    REQUIRE(std::string{field_value(r, "filename")}
            == "/etc/pvpgn/bnhelp.conf");
}

TEST_CASE("helpfile init tolerates null filename",
          "[integration][legacy_bnetd][helpfile_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_helpfile_init_try(nullptr) == 0);
    REQUIRE(std::string{field_value(sink.records.front(), "filename")}
            == "<null>");
}

TEST_CASE("helpfile unload bridge logs at Info",
          "[integration][legacy_bnetd][helpfile_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_helpfile_unload_try() == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(sink.records.front().message == "helpfile unload observed");
}

// ---- autoupdate ---------------------------------------------------

TEST_CASE("autoupdate load bridge logs filename at Info",
          "[integration][legacy_bnetd][autoupdate_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_autoupdate_load_try("/etc/pvpgn/autoupdate.conf") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level  == pvpgn::core::LogLevel::Info);
    REQUIRE(r.module == "v3_bnetd_autoupdate_bridge");
    REQUIRE(std::string{field_value(r, "filename")}
            == "/etc/pvpgn/autoupdate.conf");
}

TEST_CASE("autoupdate unload bridge logs at Info",
          "[integration][legacy_bnetd][autoupdate_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_autoupdate_unload_try() == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(sink.records.front().message == "autoupdate unload observed");
}

// ---- output -------------------------------------------------------

TEST_CASE("output init bridge logs at Debug",
          "[integration][legacy_bnetd][output_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_output_init_try() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.module  == "v3_bnetd_output_bridge");
    REQUIRE(r.message == "output init observed");
}

TEST_CASE("output write_to_file bridge logs at Debug",
          "[integration][legacy_bnetd][output_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_output_write_to_file_try() == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(sink.records.front().message == "output write_to_file observed");
}

// ---- support ------------------------------------------------------

TEST_CASE("support check_files bridge logs supportfile",
          "[integration][legacy_bnetd][support_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_support_check_files_try("/etc/pvpgn/supportfile.conf") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.module  == "v3_bnetd_support_bridge");
    REQUIRE(r.message == "support check_files observed");
    REQUIRE(std::string{field_value(r, "supportfile")}
            == "/etc/pvpgn/supportfile.conf");
}

// ---- mail ---------------------------------------------------------

TEST_CASE("mail handle_command bridge logs sd and text at Debug",
          "[integration][legacy_bnetd][mail_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_mail_handle_command_try(5, "send alice hello") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.module  == "v3_bnetd_mail_bridge");
    REQUIRE(r.message == "mail handle_command observed");
    REQUIRE(std::string{field_value(r, "sd")}   == "5");
    REQUIRE(std::string{field_value(r, "text")} == "send alice hello");
}

TEST_CASE("mail handle_command bridge null-safe",
          "[integration][legacy_bnetd][mail_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_mail_handle_command_try(-1, nullptr) == 0);
    const auto& r = sink.records.front();
    REQUIRE(std::string{field_value(r, "sd")}   == "-1");
    REQUIRE(std::string{field_value(r, "text")} == "<null>");
}

TEST_CASE("mail check bridge logs sd at Trace",
          "[integration][legacy_bnetd][mail_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_mail_check_try(11) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Trace);
    REQUIRE(r.module  == "v3_bnetd_mail_bridge");
    REQUIRE(r.message == "mail check observed");
    REQUIRE(std::string{field_value(r, "sd")} == "11");
}
