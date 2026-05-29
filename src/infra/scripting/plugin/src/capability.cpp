#include "infra/scripting/plugin/capability.hpp"
#include <unordered_map>

namespace pvpgn::infra::scripting {

Capability parse_capability(std::string_view str)
{
    static const std::unordered_map<std::string, Capability> cap_map = {
        {"chat.send", Capability::CHAT_SEND},
        {"chat.emote", Capability::CHAT_EMOTE},
        {"db.read", Capability::DB_READ},
        {"db.write", Capability::DB_WRITE},
        {"events.subscribe", Capability::EVENTS_SUBSCRIBE},
        {"events.publish", Capability::EVENTS_PUBLISH},
        {"fs.read", Capability::FS_READ},
        {"fs.write", Capability::FS_WRITE},
        {"net.http", Capability::NET_HTTP},
        {"net.socket", Capability::NET_SOCKET},
        {"commands.register", Capability::COMMANDS_REGISTER},
        {"moderation.ban", Capability::MODERATION_BAN},
        {"moderation.kick", Capability::MODERATION_KICK},
        {"store.read", Capability::STORE_READ},
        {"store.write", Capability::STORE_WRITE},
        {"admin.reload_config", Capability::ADMIN_RELOAD_CONFIG},
        {"admin.shutdown", Capability::ADMIN_SHUTDOWN},
    };
    
    auto it = cap_map.find(std::string(str));
    if (it != cap_map.end()) {
        return it->second;
    }
    
    // Return a dummy capability if not found
    return static_cast<Capability>(0);
}

std::string capability_to_string(Capability cap)
{
    switch (cap) {
        case Capability::CHAT_SEND: return "chat.send";
        case Capability::CHAT_EMOTE: return "chat.emote";
        case Capability::DB_READ: return "db.read";
        case Capability::DB_WRITE: return "db.write";
        case Capability::EVENTS_SUBSCRIBE: return "events.subscribe";
        case Capability::EVENTS_PUBLISH: return "events.publish";
        case Capability::FS_READ: return "fs.read";
        case Capability::FS_WRITE: return "fs.write";
        case Capability::NET_HTTP: return "net.http";
        case Capability::NET_SOCKET: return "net.socket";
        case Capability::COMMANDS_REGISTER: return "commands.register";
        case Capability::MODERATION_BAN: return "moderation.ban";
        case Capability::MODERATION_KICK: return "moderation.kick";
        case Capability::STORE_READ: return "store.read";
        case Capability::STORE_WRITE: return "store.write";
        case Capability::ADMIN_RELOAD_CONFIG: return "admin.reload_config";
        case Capability::ADMIN_SHUTDOWN: return "admin.shutdown";
        default: return "unknown";
    }
}

} // namespace pvpgn::infra::scripting
