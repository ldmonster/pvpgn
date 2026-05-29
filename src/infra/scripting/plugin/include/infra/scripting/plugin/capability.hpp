#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>

namespace pvpgn::infra::scripting {

/// Capability flags for plugin sandboxing
enum class Capability : std::uint32_t {
    // Chat capabilities
    CHAT_SEND = 1 << 0,           // pvpgn.chat.send_channel, send_whisper
    CHAT_EMOTE = 1 << 1,          // pvpgn.chat.emote
    
    // Database capabilities
    DB_READ = 1 << 2,             // pvpgn.account.find_by_name, list
    DB_WRITE = 1 << 3,            // pvpgn.account.* mutations
    
    // Event capabilities
    EVENTS_SUBSCRIBE = 1 << 4,    // pvpgn.events.on
    EVENTS_PUBLISH = 1 << 5,      // pvpgn.events.publish (future)
    
    // Filesystem capabilities
    FS_READ = 1 << 6,             // io.open for reading
    FS_WRITE = 1 << 7,            // io.open for writing
    
    // Network capabilities
    NET_HTTP = 1 << 8,            // pvpgn.http.get/post
    NET_SOCKET = 1 << 9,          // raw socket access (future)
    
    // Command capabilities
    COMMANDS_REGISTER = 1 << 10,  // pvpgn.commands.register
    
    // Moderation capabilities
    MODERATION_BAN = 1 << 11,     // pvpgn.moderation.ban_*
    MODERATION_KICK = 1 << 12,    // pvpgn.moderation.kick_*
    
    // Store capabilities
    STORE_READ = 1 << 13,         // pvpgn.store.get
    STORE_WRITE = 1 << 14,        // pvpgn.store.put
    
    // Admin capabilities
    ADMIN_RELOAD_CONFIG = 1 << 15, // pvpgn.admin.reload_config
    ADMIN_SHUTDOWN = 1 << 16,      // pvpgn.admin.shutdown
};

/// Capability set for a plugin
class CapabilitySet {
public:
    CapabilitySet() = default;
    explicit CapabilitySet(std::uint32_t mask) : mask_(mask) {}
    
    void grant(Capability cap) { mask_ |= static_cast<std::uint32_t>(cap); }
    void revoke(Capability cap) { mask_ &= ~static_cast<std::uint32_t>(cap); }
    bool has(Capability cap) const { return (mask_ & static_cast<std::uint32_t>(cap)) != 0; }
    
    std::uint32_t mask() const { return mask_; }
    
private:
    std::uint32_t mask_ = 0;
};

/// Parse capability string (e.g., "chat.send", "db.read")
Capability parse_capability(std::string_view str);

/// Convert capability to string
std::string capability_to_string(Capability cap);

} // namespace pvpgn::infra::scripting
