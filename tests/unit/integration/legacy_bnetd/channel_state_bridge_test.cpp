// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the observation-only channel-state bridges:
// `pvpgn_v3_joinchannel_try` and `pvpgn_v3_leavechannel_try`.
//
// These bridges have no side-effect besides structured logging;
// they always return 0 so legacy retains full state-machine
// ownership. The tests therefore exercise:
//   - null-conn handling (no log emitted, return 0)
//   - return-value contract (always 0 even on the happy path)
//   - log capture via `BridgeLoggerOverride` to confirm the
//     structured field set matches the spec.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/channel_state_bridge.hpp"

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

}  // namespace

TEST_CASE("joinchannel bridge null-conn is no-op",
          "[integration][legacy_bnetd][channel_state_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_joinchannel_try(nullptr, "Test", 0u) == 0);
    REQUIRE(sink.records.empty());
}

TEST_CASE("leavechannel bridge null-conn is no-op",
          "[integration][legacy_bnetd][channel_state_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_leavechannel_try(nullptr) == 0);
    REQUIRE(sink.records.empty());
}

TEST_CASE("joinchannel bridge logs structured fields",
          "[integration][legacy_bnetd][channel_state_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_joinchannel_try(&marker, "Public Chat", 0u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module == "v3_channel_state_bridge");
    REQUIRE(r.message == "joinchannel intent observed");
    REQUIRE(r.fields.size() == 3u);
    REQUIRE(r.fields[0].first == "channel");
    REQUIRE(r.fields[0].second == "Public Chat");
    REQUIRE(r.fields[1].first == "flag");
    REQUIRE(r.fields[1].second == "NORMAL");
    REQUIRE(r.fields[2].first == "raw");
    REQUIRE(r.fields[2].second == "0");
}

TEST_CASE("joinchannel bridge flag names",
          "[integration][legacy_bnetd][channel_state_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_joinchannel_try(&marker, "X", 1u) == 0);
    REQUIRE(::pvpgn_v3_joinchannel_try(&marker, "X", 2u) == 0);
    REQUIRE(::pvpgn_v3_joinchannel_try(&marker, "X", 99u) == 0);
    REQUIRE(sink.records.size() == 3u);
    REQUIRE(sink.records[0].fields[1].second == "GENERIC");
    REQUIRE(sink.records[1].fields[1].second == "CREATE");
    REQUIRE(sink.records[2].fields[1].second == "?");
    REQUIRE(sink.records[2].fields[2].second == "99");
}

TEST_CASE("joinchannel bridge null channel name -> empty",
          "[integration][legacy_bnetd][channel_state_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_joinchannel_try(&marker, nullptr, 0u) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(sink.records[0].fields[0].second.empty());
}

TEST_CASE("leavechannel bridge logs no fields",
          "[integration][legacy_bnetd][channel_state_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_leavechannel_try(&marker) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(sink.records[0].module == "v3_channel_state_bridge");
    REQUIRE(sink.records[0].message == "leavechannel intent observed");
    REQUIRE(sink.records[0].fields.empty());
}
