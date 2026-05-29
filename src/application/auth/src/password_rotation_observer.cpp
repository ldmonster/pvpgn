// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/password_rotation_observer.hpp"

#include <string>
#include <variant>

namespace pvpgn::application::auth {

namespace {

namespace de = pvpgn::domain::events;

}  // namespace

PasswordRotationObserver::PasswordRotationObserver(core::ILogger& logger,
                                                   ports::IEventBus& bus)
    : logger_(logger), bus_(bus) {
    token_ = bus_.subscribe(
        [this](const de::DomainEvent& e) noexcept { on_event(e); });
}

PasswordRotationObserver::~PasswordRotationObserver() {
    try {
        bus_.unsubscribe(token_);
    } catch (...) {
        // dtor must not throw.
    }
}

void PasswordRotationObserver::on_event(
    const de::DomainEvent& e) noexcept {
    try {
        if (const auto* req =
                std::get_if<de::AccountPasswordRotationRequired>(&e)) {
            const std::string id = std::to_string(req->id.value());
            const core::ILogger::Field fields[] = {
                {"event",      "password_rotation_required"},
                {"account_id", std::string_view{id}},
                {"action",     "set"},
            };
            logger_.log_kv(core::LogLevel::Info,
                           std::string_view{kChannel},
                           "must_change_password set",
                           std::span<const core::ILogger::Field>{fields});
            return;
        }
        if (const auto* cle =
                std::get_if<de::AccountPasswordRotationCleared>(&e)) {
            const std::string id = std::to_string(cle->id.value());
            const core::ILogger::Field fields[] = {
                {"event",      "password_rotation_cleared"},
                {"account_id", std::string_view{id}},
                {"action",     "cleared"},
            };
            logger_.log_kv(core::LogLevel::Info,
                           std::string_view{kChannel},
                           "must_change_password cleared",
                           std::span<const core::ILogger::Field>{fields});
            return;
        }
    } catch (...) {
        // Allocation failure mid-format: drop the record. The flag
        // flip in the aggregate is the source of truth; the audit
        // line is best-effort.
    }
}

}  // namespace pvpgn::application::auth
