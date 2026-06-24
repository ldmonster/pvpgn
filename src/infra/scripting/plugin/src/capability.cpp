#include "infra/scripting/plugin/capability.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace pvpgn::infra::scripting {

namespace {

// Token → capability lookup. This is a small (17-entry), fully-static,
// read-heavy table that is queried once per declared capability at plugin
// load time. The canonical C++23 spelling is `std::flat_map`, but the GCC13
// floor lacks `<flat_map>` (libstdc++ ships it from 14). A sorted
// `constexpr std::array` + binary search is the
// equivalent flat shape and is in fact strictly better here: it is
// allocation-free (the prior `unordered_map<std::string,...>` heap-allocated
// a `std::string` on every lookup and built a hash table on first use) and
// lives entirely in `.rodata`. Swap to `std::flat_map` once GCC14 is the
// floor without touching call sites.
//
// Entries MUST stay sorted by key — the static_assert below pins that.
constexpr std::array<std::pair<std::string_view, Capability>, 17> kCapTable{{
    {"admin.reload_config", Capability::ADMIN_RELOAD_CONFIG},
    {"admin.shutdown",      Capability::ADMIN_SHUTDOWN},
    {"chat.emote",          Capability::CHAT_EMOTE},
    {"chat.send",           Capability::CHAT_SEND},
    {"commands.register",   Capability::COMMANDS_REGISTER},
    {"db.read",             Capability::DB_READ},
    {"db.write",            Capability::DB_WRITE},
    {"events.publish",      Capability::EVENTS_PUBLISH},
    {"events.subscribe",    Capability::EVENTS_SUBSCRIBE},
    {"fs.read",             Capability::FS_READ},
    {"fs.write",            Capability::FS_WRITE},
    {"moderation.ban",      Capability::MODERATION_BAN},
    {"moderation.kick",     Capability::MODERATION_KICK},
    {"net.http",            Capability::NET_HTTP},
    {"net.socket",          Capability::NET_SOCKET},
    {"store.read",          Capability::STORE_READ},
    {"store.write",         Capability::STORE_WRITE},
}};

static_assert(std::ranges::is_sorted(kCapTable, {},
                                     &std::pair<std::string_view, Capability>::first),
              "kCapTable must stay sorted by token for binary search");

}  // namespace

Capability parse_capability(std::string_view str)
{
    const auto it = std::ranges::lower_bound(
        kCapTable, str, {},
        &std::pair<std::string_view, Capability>::first);
    if (it != kCapTable.end() && it->first == str) {
        return it->second;
    }

    // Unknown token: an empty capability (no bits granted).
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
