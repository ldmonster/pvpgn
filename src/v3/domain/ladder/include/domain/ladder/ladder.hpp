// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ladder.hpp
/// `ladder::LadderCalculator` — pure stateless domain service that
/// turns a `MatchReport` into a ladder delta.
///
/// Today's `ladder_calc.cpp` is being lifted here piecewise; this
/// first cut implements the simple "Elo-style + W3 K-factor 32"
/// algorithm sufficient for tests. Per-client-tag strategies plug
/// in via the `LadderRules` parameter.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/match_report.hpp"

namespace pvpgn::domain::ladder {

struct LadderEntry {
    AccountId    account;
    std::int32_t rating       = 1500;
    std::uint32_t wins        = 0;
    std::uint32_t losses      = 0;
    std::uint32_t disconnects = 0;
};

struct LadderDelta {
    AccountId    account;
    std::int32_t rating_delta = 0;
    std::uint32_t wins_delta        = 0;
    std::uint32_t losses_delta      = 0;
    std::uint32_t disconnects_delta = 0;
};

struct LadderRules {
    /// W3 default; SC uses 16. Override per client tag.
    double k_factor = 32.0;
    /// Disconnect counted as a loss for rating purposes.
    bool   disconnect_is_loss = true;
};

class LadderCalculator {
public:
    explicit LadderCalculator(LadderRules rules = {}) : rules_(rules) {}

    /// Compute the per-player delta for a 1v1 or N-player free-for-all.
    /// Caller passes the *current* ladder entry for each participant
    /// (in the same order as `report.results`); missing entries are
    /// treated as fresh 1500-rated rows.
    std::vector<LadderDelta>
    compute(const MatchReport& report, std::span<const LadderEntry> entries) const {
        std::vector<LadderDelta> out;
        out.reserve(report.results.size());

        for (std::size_t i = 0; i < report.results.size(); ++i) {
            const auto& r = report.results[i];
            const std::int32_t cur_rating =
                (i < entries.size()) ? entries[i].rating : 1500;
            const double opp_mean = opponent_mean_(entries, i);

            LadderDelta d;
            d.account = r.account;

            switch (r.outcome) {
                case MatchOutcome::Win:
                    d.wins_delta = 1;
                    d.rating_delta = rating_step_(cur_rating, opp_mean, 1.0);
                    break;
                case MatchOutcome::Loss:
                    d.losses_delta = 1;
                    d.rating_delta = rating_step_(cur_rating, opp_mean, 0.0);
                    break;
                case MatchOutcome::Draw:
                    d.rating_delta = rating_step_(cur_rating, opp_mean, 0.5);
                    break;
                case MatchOutcome::Disconnect:
                    d.disconnects_delta = 1;
                    if (rules_.disconnect_is_loss) {
                        d.losses_delta = 1;
                        d.rating_delta = rating_step_(cur_rating, opp_mean, 0.0);
                    }
                    break;
            }
            out.push_back(d);
        }
        return out;
    }

private:
    double opponent_mean_(std::span<const LadderEntry> entries, std::size_t self) const noexcept {
        if (entries.size() <= 1) return 1500.0;
        double sum = 0.0;
        std::size_t n = 0;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (i == self) continue;
            sum += static_cast<double>(entries[i].rating);
            ++n;
        }
        return n == 0 ? 1500.0 : sum / static_cast<double>(n);
    }

    std::int32_t rating_step_(std::int32_t cur, double opp_mean, double score) const noexcept {
        const double expected =
            1.0 / (1.0 + std::pow(10.0, (opp_mean - static_cast<double>(cur)) / 400.0));
        const double delta = rules_.k_factor * (score - expected);
        return static_cast<std::int32_t>(std::lround(delta));
    }

    LadderRules rules_;
};

}  // namespace pvpgn::domain::ladder
