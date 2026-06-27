// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_topic_store.hpp
/// In-memory ITopicStore — channel-NAME-keyed topic map. Thread-safe; mirrors
/// the original server's class_topiclist (topic.cpp), which keeps topics in
/// memory with no topicfile present. Topics outlive the Channel object so a
/// re-joiner of an emptied-then-recreated channel sees the prior topic.

#include <algorithm>
#include <cctype>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "domain/chat/topic_store.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryTopicStore final : public domain::chat::ITopicStore {
public:
    void set(std::string_view channel_name, std::string topic) override {
        std::unique_lock lock(mutex_);
        by_name_[canonical(channel_name)] = std::move(topic);
    }

    [[nodiscard]] std::string
    get(std::string_view channel_name) const override {
        std::shared_lock lock(mutex_);
        auto it = by_name_.find(canonical(channel_name));
        if (it == by_name_.end()) return {};
        return it->second;
    }

private:
    static std::string canonical(std::string_view name) {
        std::string out(name);
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
        return out;
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> by_name_;
};

}  // namespace pvpgn::infra::inmemory
