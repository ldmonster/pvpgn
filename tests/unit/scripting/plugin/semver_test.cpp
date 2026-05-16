// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include "scripting/plugin/semver.hpp"

namespace pvpgn::scripting::plugin::test {

TEST(SemVerTest, ParseBasicVersion) {
    auto v = SemVer::parse("1.2.3");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->major, 1u);
    EXPECT_EQ(v->minor, 2u);
    EXPECT_EQ(v->patch, 3u);
    EXPECT_TRUE(v->prerelease.empty());
}

TEST(SemVerTest, ParseWithPrerelease) {
    auto v = SemVer::parse("2.0.0-alpha.1");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->major, 2u);
    EXPECT_EQ(v->prerelease, "alpha.1");
}

TEST(SemVerTest, ParseWithBuildMeta) {
    auto v = SemVer::parse("1.0.0+build.123");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->build_meta, "build.123");
}

TEST(SemVerTest, ParseWithPrereleaseAndBuild) {
    auto v = SemVer::parse("1.0.0-beta+build.42");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->prerelease, "beta");
    EXPECT_EQ(v->build_meta, "build.42");
}

TEST(SemVerTest, ParseInvalidReturnsError) {
    EXPECT_FALSE(SemVer::parse("not-a-version").has_value());
    EXPECT_FALSE(SemVer::parse("1.2").has_value());
    EXPECT_FALSE(SemVer::parse("").has_value());
}

TEST(SemVerTest, ComparisonOrdering) {
    auto v100 = SemVer::parse("1.0.0").value();
    auto v110 = SemVer::parse("1.1.0").value();
    auto v200 = SemVer::parse("2.0.0").value();
    auto v100pre = SemVer::parse("1.0.0-alpha").value();

    EXPECT_LT(v100, v110);
    EXPECT_LT(v110, v200);
    EXPECT_LT(v100pre, v100); // pre-release < release
    EXPECT_EQ(v100, SemVer::parse("1.0.0").value());
}

TEST(SemVerTest, BuildMetadataIgnoredInComparison) {
    auto v1 = SemVer::parse("1.0.0+build1").value();
    auto v2 = SemVer::parse("1.0.0+build2").value();
    EXPECT_EQ(v1, v2); // Build metadata ignored
}

TEST(SemVerTest, PrereleaseComparison) {
    auto v_alpha = SemVer::parse("1.0.0-alpha").value();
    auto v_alpha1 = SemVer::parse("1.0.0-alpha.1").value();
    auto v_alpha_beta = SemVer::parse("1.0.0-alpha.beta").value();
    auto v_beta = SemVer::parse("1.0.0-beta").value();
    auto v_beta2 = SemVer::parse("1.0.0-beta.2").value();
    auto v_beta11 = SemVer::parse("1.0.0-beta.11").value();
    auto v_rc1 = SemVer::parse("1.0.0-rc.1").value();
    auto v_release = SemVer::parse("1.0.0").value();

    EXPECT_LT(v_alpha, v_alpha1);
    EXPECT_LT(v_alpha1, v_alpha_beta);
    EXPECT_LT(v_alpha_beta, v_beta);
    EXPECT_LT(v_beta, v_beta2);
    EXPECT_LT(v_beta2, v_beta11);
    EXPECT_LT(v_beta11, v_rc1);
    EXPECT_LT(v_rc1, v_release);
}

TEST(SemVerTest, ToStringRoundTrip) {
    std::string ver = "1.2.3-beta.1+build.42";
    auto v = SemVer::parse(ver).value();
    EXPECT_EQ(v.to_string(), ver);
}

TEST(SemVerTest, ToStringWithoutPrerelease) {
    auto v = SemVer::parse("2.3.4").value();
    EXPECT_EQ(v.to_string(), "2.3.4");
}

TEST(VersionReqTest, ParseExactMatch) {
    auto req = VersionReq::parse("=1.2.3");
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->op, VersionReq::Op::Eq);
    EXPECT_EQ(req->version.major, 1u);
}

TEST(VersionReqTest, ParseGte) {
    auto req = VersionReq::parse(">=1.2.3");
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->op, VersionReq::Op::Ge);
}

TEST(VersionReqTest, ParseCaret) {
    auto req = VersionReq::parse("^1.2.3");
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->op, VersionReq::Op::Caret);
}

TEST(VersionReqTest, ParseTilde) {
    auto req = VersionReq::parse("~1.2.3");
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->op, VersionReq::Op::Tilde);
}

TEST(VersionReqTest, CaretRequirement) {
    auto v = SemVer::parse("1.5.0").value();
    EXPECT_TRUE(v.satisfies("^1.0.0"));  // >=1.0.0 <2.0.0
    EXPECT_FALSE(v.satisfies("^2.0.0")); // >=2.0.0 <3.0.0
}

TEST(VersionReqTest, CaretZeroMajor) {
    auto v = SemVer::parse("0.2.5").value();
    EXPECT_TRUE(v.satisfies("^0.2.0"));  // >=0.2.0 <0.3.0
    EXPECT_FALSE(v.satisfies("^0.3.0")); // >=0.3.0 <0.4.0
}

TEST(VersionReqTest, CaretZeroMinor) {
    auto v = SemVer::parse("0.0.5").value();
    EXPECT_TRUE(v.satisfies("^0.0.5"));  // >=0.0.5 <0.0.6
    EXPECT_FALSE(v.satisfies("^0.0.6")); // >=0.0.6 <0.0.7
}

TEST(VersionReqTest, TildeRequirement) {
    auto v = SemVer::parse("1.2.5").value();
    EXPECT_TRUE(v.satisfies("~1.2.0"));  // >=1.2.0 <1.3.0
    EXPECT_FALSE(v.satisfies("~1.3.0")); // >=1.3.0 <1.4.0
}

TEST(VersionReqTest, GteRequirement) {
    auto v = SemVer::parse("2.1.0").value();
    EXPECT_TRUE(v.satisfies(">=2.0.0"));
    EXPECT_FALSE(v.satisfies(">=3.0.0"));
}

TEST(VersionReqTest, LteRequirement) {
    auto v = SemVer::parse("1.5.0").value();
    EXPECT_TRUE(v.satisfies("<=2.0.0"));
    EXPECT_FALSE(v.satisfies("<=1.0.0"));
}

TEST(VersionReqTest, NotEqualRequirement) {
    auto v = SemVer::parse("1.5.0").value();
    EXPECT_TRUE(v.satisfies("!=1.0.0"));
    EXPECT_FALSE(v.satisfies("!=1.5.0"));
}

TEST(VersionReqTest, GreaterThanRequirement) {
    auto v = SemVer::parse("2.0.0").value();
    EXPECT_TRUE(v.satisfies(">1.9.9"));
    EXPECT_FALSE(v.satisfies(">2.0.0"));
}

TEST(VersionReqTest, LessThanRequirement) {
    auto v = SemVer::parse("1.9.9").value();
    EXPECT_TRUE(v.satisfies("<2.0.0"));
    EXPECT_FALSE(v.satisfies("<1.9.9"));
}

} // namespace pvpgn::scripting::plugin::test
