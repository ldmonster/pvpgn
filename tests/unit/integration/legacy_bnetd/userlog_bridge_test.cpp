// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the userlog observation bridge.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/userlog_bridge.hpp"

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

TEST_CASE("userlog init bridge logs",
          "[integration][legacy_bnetd][userlog_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_userlog_init() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_userlog_bridge");
    REQUIRE(r.message == "userlog init observed");
    REQUIRE(r.fields.empty());
}

TEST_CASE("userlog append bridge logs user and text",
          "[integration][legacy_bnetd][userlog_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_userlog_append("alice", "/kick bob") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_userlog_bridge");
    REQUIRE(r.message == "userlog append observed");
    REQUIRE(field_value(r, "user") == "alice");
    REQUIRE(field_value(r, "text") == "/kick bob");
}

TEST_CASE("userlog append bridge null params",
          "[integration][legacy_bnetd][userlog_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_userlog_append(nullptr, nullptr) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "user").empty());
    REQUIRE(field_value(sink.records[0], "text").empty());
}
