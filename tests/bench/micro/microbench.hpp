// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file microbench.hpp
/// A tiny, dependency-free microbenchmark harness.
///
/// `nanobench` (vcpkg, header-only) is not available in every build
/// environment, so this in-tree equivalent provides the same essentials:
/// warm-up, repeated samples, and a **median + MAD** summary (median-of-N + MAD
/// so a noisy CI runner does not flap the gate). Results serialise to a
/// `bench-results.json` for the runner / regression gate. Swap in nanobench
/// later without changing the bench cases if a richer report is wanted.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

namespace pvpgn::bench {

/// Keep the optimizer from eliding work whose result is otherwise unused.
template <class T>
inline void do_not_optimize(const T& value) {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "r,m"(value) : "memory");
#else
    volatile const T sink = value;
    (void)sink;
#endif
}

struct Result {
    std::string   name;
    std::uint64_t iters_per_sample = 0;
    int           samples          = 0;
    double        ns_per_op_median = 0.0;
    double        ns_per_op_mad    = 0.0;  // median absolute deviation
};

class Microbench {
public:
    /// Run @p body (one operation per call) @p iters times per sample, over
    /// @p samples samples plus one discarded warm-up sample. Records the median
    /// ns/op and its MAD.
    template <class F>
    void run(std::string name, std::uint64_t iters, F&& body, int samples = 7) {
        using clock = std::chrono::steady_clock;

        auto one_sample = [&]() -> double {
            const auto t0 = clock::now();
            for (std::uint64_t i = 0; i < iters; ++i) {
                body();
            }
            const auto t1 = clock::now();
            const double ns =
                std::chrono::duration<double, std::nano>(t1 - t0).count();
            return ns / static_cast<double>(iters);
        };

        (void)one_sample();  // warm-up (caches, branch predictor) — discarded

        std::vector<double> xs;
        xs.reserve(static_cast<std::size_t>(samples));
        for (int s = 0; s < samples; ++s) xs.push_back(one_sample());

        const double med = median(xs);
        std::vector<double> dev;
        dev.reserve(xs.size());
        for (double x : xs) dev.push_back(std::fabs(x - med));
        const double mad = median(dev);

        results_.push_back(
            Result{std::move(name), iters, samples, med, mad});
    }

    const std::vector<Result>& results() const noexcept { return results_; }

    void print_table(std::ostream& os) const {
        os << "\n  microbench (median of N samples, ns/op +- MAD)\n";
        os << "  ------------------------------------------------------------\n";
        for (const auto& r : results_) {
            os << "  " << pad(r.name, 34) << "  " << fmt_ns(r.ns_per_op_median)
               << "  +-" << fmt_ns(r.ns_per_op_mad) << "  (" << r.iters_per_sample
               << " it x" << r.samples << ")\n";
        }
        os << "\n";
    }

    /// Emit a bench-results.json. @p git_rev / @p host are stamped in by the
    /// caller (the harness cannot call Date.now()/exec).
    void write_json(const std::string& path, const std::string& git_rev,
                    const std::string& host) const {
        std::ofstream f(path);
        f << "{\n  \"git_rev\": \"" << git_rev << "\",\n  \"host\": \"" << host
          << "\",\n  \"suite\": \"micro\",\n  \"benchmarks\": [\n";
        for (std::size_t i = 0; i < results_.size(); ++i) {
            const auto& r = results_[i];
            f << "    {\"name\": \"" << r.name << "\", \"ns_per_op\": "
              << r.ns_per_op_median << ", \"mad_ns\": " << r.ns_per_op_mad
              << ", \"iters\": " << r.iters_per_sample << ", \"samples\": "
              << r.samples << "}" << (i + 1 < results_.size() ? "," : "") << "\n";
        }
        f << "  ]\n}\n";
    }

private:
    static double median(std::vector<double> v) {
        if (v.empty()) return 0.0;
        std::sort(v.begin(), v.end());
        const std::size_t n = v.size();
        return (n & 1u) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
    }
    static std::string pad(std::string s, std::size_t w) {
        if (s.size() < w) s.append(w - s.size(), ' ');
        return s;
    }
    static std::string fmt_ns(double ns) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%9.2f ns", ns);
        return buf;
    }

    std::vector<Result> results_;
};

}  // namespace pvpgn::bench
