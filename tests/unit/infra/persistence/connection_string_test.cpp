// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "core/error.hpp"
#include "infra/persistence/connection_string.hpp"

namespace ip = pvpgn::infra::persistence;

TEST_CASE("parse_connection_string host + database (no port)",
          "[infra][persistence][connstring]") {
    auto r = ip::parse_connection_string("db.example.com/bnetd");
    REQUIRE(r);
    CHECK(r.value().host == "db.example.com");
    CHECK(r.value().port == 0);
    CHECK(r.value().database == "bnetd");
}

TEST_CASE("parse_connection_string host:port/database",
          "[infra][persistence][connstring]") {
    auto r = ip::parse_connection_string("127.0.0.1:3306/bnetd");
    REQUIRE(r);
    CHECK(r.value().host == "127.0.0.1");
    CHECK(r.value().port == 3306);
    CHECK(r.value().database == "bnetd");
}

TEST_CASE("parse_connection_string UNIX socket-style path keeps slashes in host",
          "[infra][persistence][connstring]") {
    auto r = ip::parse_connection_string("/var/run/mysqld.sock/bnetd");
    REQUIRE(r);
    CHECK(r.value().host == "/var/run/mysqld.sock");
    CHECK(r.value().port == 0);
    CHECK(r.value().database == "bnetd");
}

TEST_CASE("parse_connection_string boundary ports",
          "[infra][persistence][connstring]") {
    SECTION("port 1") {
        auto r = ip::parse_connection_string("h:1/d");
        REQUIRE(r);
        CHECK(r.value().port == 1);
    }
    SECTION("port 65535") {
        auto r = ip::parse_connection_string("h:65535/d");
        REQUIRE(r);
        CHECK(r.value().port == 65535);
    }
}

TEST_CASE("parse_connection_string rejects malformed input",
          "[infra][persistence][connstring]") {
    using SC = pvpgn::core::StatusCode;

    SECTION("empty") {
        auto r = ip::parse_connection_string("");
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == SC::InvalidArgument);
    }
    SECTION("no slash") {
        auto r = ip::parse_connection_string("host:3306");
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == SC::InvalidArgument);
    }
    SECTION("empty database") {
        auto r = ip::parse_connection_string("host:3306/");
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == SC::InvalidArgument);
    }
    SECTION("empty host") {
        auto r = ip::parse_connection_string("/bnetd");
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == SC::InvalidArgument);
    }
    SECTION("non-numeric port") {
        auto r = ip::parse_connection_string("host:abc/db");
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == SC::InvalidArgument);
    }
    SECTION("port out of range (0)") {
        auto r = ip::parse_connection_string("host:0/db");
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == SC::InvalidArgument);
    }
    SECTION("port out of range (65536)") {
        auto r = ip::parse_connection_string("host:65536/db");
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == SC::InvalidArgument);
    }
    SECTION("trailing colon") {
        auto r = ip::parse_connection_string("host:/db");
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == SC::InvalidArgument);
    }
}
