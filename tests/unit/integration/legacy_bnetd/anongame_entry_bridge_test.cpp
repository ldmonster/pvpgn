// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the observation-only anongame entry bridge.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/anongame_entry_bridge.hpp"
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

TEST_CASE("anongame entry bridge null-conn is no-op",
          "[integration][legacy_bnetd][anongame_entry_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_anongame_entry(nullptr, "search") == 0);
    REQUIRE(sink.records.empty());
}

TEST_CASE("anongame entry bridge logs kind",
          "[integration][legacy_bnetd][anongame_entry_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_anongame_entry(&marker, "search") == 0);
    REQUIRE(::pvpgn_v3_anongame_entry(&marker, "join")   == 0);
    REQUIRE(sink.records.size() == 2u);
    REQUIRE(sink.records[0].module  == "v3_anongame_entry_bridge");
    REQUIRE(sink.records[0].message == "anongame entry observed");
    REQUIRE(field_value(sink.records[0], "kind") == "search");
    REQUIRE(field_value(sink.records[1], "kind") == "join");
}

TEST_CASE("anongame entry bridge null kind -> ?",
          "[integration][legacy_bnetd][anongame_entry_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_anongame_entry(&marker, nullptr) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "kind") == "?");
}
