// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/file/ip_ban_repository.hpp"

#include <charconv>
#include <cstdio>
#include <fstream>
#include <string_view>

#include "core/clock.hpp"
#include "domain/moderation/ban_pattern.hpp"
#include "domain/moderation/ip_ban_list.hpp"

namespace pvpgn::infra::file {

namespace {

/// Trim leading/trailing ASCII whitespace.
std::string_view trim_(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

/// Split a trimmed bnban.conf line into its IP field and an optional
/// trailing endtime field (the legacy `<ip> <endtime>` format). The IP
/// field is everything up to the first run of whitespace.
std::pair<std::string_view, std::string_view> split_line_(std::string_view line) {
    const auto ws = line.find_first_of(" \t");
    if (ws == std::string_view::npos) return {line, {}};
    std::string_view ip = line.substr(0, ws);
    std::string_view rest = trim_(line.substr(ws));
    return {ip, rest};
}

/// Parse the optional trailing endtime (epoch seconds) into a SystemTime.
/// `0` (or absent / unparseable) means a permanent ban (nullopt), matching
/// the original `endtime == 0` convention.
std::optional<core::SystemTime> parse_endtime_(std::string_view tok) {
    if (tok.empty()) return std::nullopt;
    long long secs = 0;
    const auto* begin = tok.data();
    const auto* end = tok.data() + tok.size();
    auto [ptr, ec] = std::from_chars(begin, end, secs);
    if (ec != std::errc{} || ptr != end || secs <= 0) return std::nullopt;
    return core::SystemTime{std::chrono::seconds{secs}};
}

}  // namespace

FileIpBanRepository::FileIpBanRepository(std::string_view ban_file)
    : ban_file_(ban_file),
      cache_(std::make_unique<inmemory::InMemoryIpBanRepository>()) {
    load_all();
}

void FileIpBanRepository::load_all() {
    std::ifstream file{std::string{ban_file_}};
    if (!file.is_open()) {
        return;
    }

    namespace mod = domain::moderation;
    mod::IpBanList banlist;
    const auto issued_at = core::SystemClock{}.now();
    const domain::AccountId issuer{0};
    const std::string reason{"Legacy ban"};

    std::string raw;
    while (std::getline(file, raw)) {
        std::string_view line = trim_(raw);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        auto [ip_field, time_field] = split_line_(line);
        auto expires_at = parse_endtime_(time_field);

        // Fast path: a full IPv4/IPv6 exact address (covers the bnban.conf
        // majority, including IPv6 which BanPattern does not model).
        if (auto exact = domain::IpAddress::parse(ip_field); exact.has_value()) {
            banlist.add(mod::IpBanEntry{exact.value(), reason, issuer,
                                        issued_at, expires_at});
            continue;
        }

        // Wildcard / range / CIDR / dotted-netmask forms.
        auto pattern = mod::BanPattern::parse(ip_field);
        if (!pattern.has_value()) {
            // Truly unparseable: log + skip (never silently lose a line).
            // No logger is injected into this repo yet; write to stderr so
            // operators migrating a legacy bnban.conf still see the warning.
            std::fprintf(stderr,
                         "[FileIpBanRepository] skipping malformed bnban.conf "
                         "entry: \"%.*s\"\n",
                         static_cast<int>(ip_field.size()), ip_field.data());
            continue;
        }

        switch (pattern->kind()) {
            case mod::BanPattern::Kind::Exact:
            case mod::BanPattern::Kind::Cidr: {
                // Route the contiguous forms through the CIDR fast path.
                auto cidr = pattern->as_cidr();
                banlist.add_range(cidr->first, cidr->second, reason, issuer,
                                  issued_at, expires_at);
                break;
            }
            case mod::BanPattern::Kind::Wildcard:
            case mod::BanPattern::Kind::Range:
                banlist.add_ban_pattern(*pattern, reason, issuer, issued_at,
                                        expires_at);
                break;
        }
    }

    (void)cache_->save_banlist(banlist);
}

core::Result<bool>
FileIpBanRepository::is_banned(const domain::IpAddress& ip) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->is_banned(ip);
}

core::Status<>
FileIpBanRepository::add_ban(domain::moderation::IpBanEntry entry) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->add_ban(std::move(entry));
}

core::Status<>
FileIpBanRepository::add_range_ban(domain::IpAddress network,
                                   std::uint8_t prefix_bits,
                                   std::string reason,
                                   domain::AccountId issuer,
                                   core::SystemTime issued_at,
                                   std::optional<core::SystemTime> expires_at) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->add_range_ban(network, prefix_bits, std::move(reason),
                                 issuer, issued_at, expires_at);
}

core::Status<>
FileIpBanRepository::remove_ban(const domain::IpAddress& ip) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->remove_ban(ip);
}

core::Status<>
FileIpBanRepository::remove_range_ban(domain::IpAddress network,
                                      std::uint8_t prefix_bits) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->remove_range_ban(network, prefix_bits);
}

void FileIpBanRepository::for_each_entry(
    std::function<bool(const domain::moderation::IpBanEntry&)> predicate) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    cache_->for_each_entry(std::move(predicate));
}

core::Result<domain::moderation::IpBanList>
FileIpBanRepository::load_banlist() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->load_banlist();
}

core::Status<>
FileIpBanRepository::save_banlist(const domain::moderation::IpBanList& banlist) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->save_banlist(banlist);
}

}  // namespace pvpgn::infra::file
