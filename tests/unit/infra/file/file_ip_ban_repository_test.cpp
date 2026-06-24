// SPDX-License-Identifier: GPL-2.0-or-later

/// @file file_ip_ban_repository_test.cpp
/// Catch2 tests for FileIpBanRepository — verifies the bnban.conf loader
/// round-trips every legacy ban syntax (exact, trailing wildcard,
/// middle-octet wildcard, inclusive range, CIDR prefix, dotted netmask)
/// plus the optional trailing endtime field, instead of silently dropping
/// wildcard/range/CIDR lines (the original bug).

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "domain/shared/ip_address.hpp"
#include "infra/file/ip_ban_repository.hpp"

namespace pvpgn::infra::file {

namespace {

class TempBanFile {
public:
    explicit TempBanFile(const std::string& contents) {
        path_ = std::filesystem::temp_directory_path() /
                ("pvpgn_bnban_test_" +
                 std::to_string(std::chrono::steady_clock::now()
                                    .time_since_epoch()
                                    .count()) +
                 ".conf");
        std::ofstream out{path_};
        out << contents;
    }
    ~TempBanFile() noexcept {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
    std::string str() const { return path_.string(); }

private:
    std::filesystem::path path_;
};

bool banned(const FileIpBanRepository& repo, const char* ip) {
    auto r = repo.is_banned(domain::IpAddress::parse(ip).value());
    REQUIRE(r);
    return r.value();
}

}  // namespace

TEST_CASE("FileIpBanRepository: loads all five bnban.conf syntaxes",
          "[infra][file][ipban]") {
    TempBanFile f(
        "# comment line\n"
        "\n"
        "1.2.3.4\n"             // exact
        "5.6.7.*\n"            // trailing wildcard
        "8.9.*.4\n"           // middle-octet wildcard
        "20.0.0.10-20.0.0.40\n"  // inclusive range
        "30.0.0.0/24\n"       // CIDR prefix
        "40.0.0.0/255.255.0.0\n"  // dotted netmask (/16)
    );

    FileIpBanRepository repo{f.str()};

    // exact
    REQUIRE(banned(repo, "1.2.3.4"));
    REQUIRE_FALSE(banned(repo, "1.2.3.5"));

    // trailing wildcard 5.6.7.*
    REQUIRE(banned(repo, "5.6.7.0"));
    REQUIRE(banned(repo, "5.6.7.200"));
    REQUIRE_FALSE(banned(repo, "5.6.8.0"));

    // middle-octet wildcard 8.9.*.4
    REQUIRE(banned(repo, "8.9.99.4"));
    REQUIRE_FALSE(banned(repo, "8.9.99.5"));

    // inclusive range 20.0.0.10-20.0.0.40
    REQUIRE(banned(repo, "20.0.0.20"));
    REQUIRE_FALSE(banned(repo, "20.0.0.41"));
    REQUIRE_FALSE(banned(repo, "20.0.0.9"));

    // CIDR /24
    REQUIRE(banned(repo, "30.0.0.123"));
    REQUIRE_FALSE(banned(repo, "30.0.1.123"));

    // dotted netmask /16
    REQUIRE(banned(repo, "40.0.5.5"));
    REQUIRE_FALSE(banned(repo, "40.1.5.5"));
}

TEST_CASE("FileIpBanRepository: trailing endtime field is parsed (not permanent)",
          "[infra][file][ipban]") {
    // Past epoch endtime -> already expired -> not banned.
    // Far-future endtime -> still active -> banned.
    const auto far_future =
        std::chrono::duration_cast<std::chrono::seconds>(
            (std::chrono::system_clock::now() + std::chrono::hours(24 * 365))
                .time_since_epoch())
            .count();

    TempBanFile f(
        "1.1.1.1 100\n"  // endtime epoch 100s -> long expired
        "2.2.2.2 " + std::to_string(far_future) + "\n");

    FileIpBanRepository repo{f.str()};

    REQUIRE_FALSE(banned(repo, "1.1.1.1"));  // expired endtime honoured
    REQUIRE(banned(repo, "2.2.2.2"));        // future endtime still active
}

TEST_CASE("FileIpBanRepository: malformed line is skipped, valid lines survive",
          "[infra][file][ipban]") {
    TempBanFile f(
        "this-is-not-an-ip\n"
        "9.9.9.9\n");

    FileIpBanRepository repo{f.str()};
    REQUIRE(banned(repo, "9.9.9.9"));
}

TEST_CASE("FileIpBanRepository: missing file loads empty (no crash)",
          "[infra][file][ipban]") {
    FileIpBanRepository repo{"/nonexistent/path/bnban.conf"};
    REQUIRE_FALSE(banned(repo, "1.2.3.4"));
}

}  // namespace pvpgn::infra::file
