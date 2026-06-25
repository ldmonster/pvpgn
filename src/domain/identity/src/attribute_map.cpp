// SPDX-License-Identifier: GPL-2.0-or-later

#include "domain/identity/attribute_map.hpp"

#include <sstream>

namespace pvpgn::domain::identity {

// --- Identity attributes ---

std::optional<std::string> AttributeMap::username() const {
    auto val = get("BNET\\acct\\username");
    if (!val) return std::nullopt;
    return std::string{val.value()};
}

std::optional<std::string> AttributeMap::email() const {
    auto val = get("BNET\\acct\\email");
    if (!val) return std::nullopt;
    return std::string{val.value()};
}

std::optional<std::string> AttributeMap::sex() const {
    auto val = get("BNET\\acct\\sex");
    if (!val) return std::nullopt;
    return std::string{val.value()};
}

std::optional<std::string> AttributeMap::location() const {
    auto val = get("BNET\\acct\\location");
    if (!val) return std::nullopt;
    return std::string{val.value()};
}

std::optional<std::string> AttributeMap::description() const {
    auto val = get("BNET\\acct\\description");
    if (!val) return std::nullopt;
    return std::string{val.value()};
}

std::optional<core::SystemTime> AttributeMap::last_login() const {
    auto val = get("BNET\\acct\\lastlogin_time");
    if (!val) return std::nullopt;
    // Parse timestamp from string
    try {
        auto timestamp = std::stoll(std::string{val.value()});
        return core::SystemTime{std::chrono::seconds{timestamp}};
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<core::SystemTime> AttributeMap::created_at() const {
    auto val = get("BNET\\acct\\ctime");
    if (!val) return std::nullopt;
    // Parse timestamp from string
    try {
        auto timestamp = std::stoll(std::string{val.value()});
        return core::SystemTime{std::chrono::seconds{timestamp}};
    } catch (...) {
        return std::nullopt;
    }
}

void AttributeMap::set_email(std::string_view v) {
    set("BNET\\acct\\email", std::string{v});
}

void AttributeMap::set_sex(std::string_view v) {
    set("BNET\\acct\\sex", std::string{v});
}

void AttributeMap::set_location(std::string_view v) {
    set("BNET\\acct\\location", std::string{v});
}

void AttributeMap::set_description(std::string_view v) {
    set("BNET\\acct\\description", std::string{v});
}

void AttributeMap::set_last_login(core::SystemTime t) {
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
    set("BNET\\acct\\lastlogin_time", std::to_string(timestamp));
}

void AttributeMap::set_created_at(core::SystemTime t) {
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
    set("BNET\\acct\\ctime", std::to_string(timestamp));
}

// --- Game statistics per ClientTag ---

static std::string make_stat_key(std::string_view prefix, ClientTag tag) {
    std::ostringstream oss;
    oss << "Record\\" << prefix << "\\" << tag.packed_be();
    return oss.str();
}

std::uint32_t AttributeMap::wins(ClientTag tag) const noexcept {
    auto val = get(make_stat_key("wins", tag));
    if (!val) return 0;
    try {
        return static_cast<std::uint32_t>(std::stoul(std::string{val.value()}));
    } catch (...) {
        return 0;
    }
}

std::uint32_t AttributeMap::losses(ClientTag tag) const noexcept {
    auto val = get(make_stat_key("losses", tag));
    if (!val) return 0;
    try {
        return static_cast<std::uint32_t>(std::stoul(std::string{val.value()}));
    } catch (...) {
        return 0;
    }
}

std::uint32_t AttributeMap::disconnects(ClientTag tag) const noexcept {
    auto val = get(make_stat_key("disconnects", tag));
    if (!val) return 0;
    try {
        return static_cast<std::uint32_t>(std::stoul(std::string{val.value()}));
    } catch (...) {
        return 0;
    }
}

std::uint32_t AttributeMap::ladder_wins(ClientTag tag) const noexcept {
    auto val = get(make_stat_key("ladder_wins", tag));
    if (!val) return 0;
    try {
        return static_cast<std::uint32_t>(std::stoul(std::string{val.value()}));
    } catch (...) {
        return 0;
    }
}

std::uint32_t AttributeMap::ladder_losses(ClientTag tag) const noexcept {
    auto val = get(make_stat_key("ladder_losses", tag));
    if (!val) return 0;
    try {
        return static_cast<std::uint32_t>(std::stoul(std::string{val.value()}));
    } catch (...) {
        return 0;
    }
}

void AttributeMap::increment_wins(ClientTag tag) {
    auto current = wins(tag);
    set(make_stat_key("wins", tag), std::to_string(current + 1));
}

void AttributeMap::increment_losses(ClientTag tag) {
    auto current = losses(tag);
    set(make_stat_key("losses", tag), std::to_string(current + 1));
}

void AttributeMap::increment_disconnects(ClientTag tag) {
    auto current = disconnects(tag);
    set(make_stat_key("disconnects", tag), std::to_string(current + 1));
}

void AttributeMap::increment_ladder_wins(ClientTag tag) {
    auto current = ladder_wins(tag);
    set(make_stat_key("ladder_wins", tag), std::to_string(current + 1));
}

void AttributeMap::increment_ladder_losses(ClientTag tag) {
    auto current = ladder_losses(tag);
    set(make_stat_key("ladder_losses", tag), std::to_string(current + 1));
}

}  // namespace pvpgn::domain::identity
