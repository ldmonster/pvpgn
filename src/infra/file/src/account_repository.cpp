// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/file/account_repository.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#if defined(_WIN32)
#  include <io.h>  // _commit (fsync equivalent on Windows)
#  ifndef O_CLOEXEC
// Windows has no fork/exec, so close-on-exec is a no-op here.
#    define O_CLOEXEC 0
#  endif
#endif

#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/file/flat_db_reader.hpp"

namespace pvpgn::infra::file {

namespace {

/// Decode a 40-character hex string into a 20-byte BNHash.
/// Returns a zeroed BNHash on any parse error.
domain::BNHash bn_hash_from_hex(std::string_view hex) {
    if (hex.size() != 40) {
        return domain::BNHash{};
    }
    domain::BNHash::Bytes bytes{};
    for (std::size_t i = 0; i < 20; ++i) {
        unsigned int hi = 0, lo = 0;
        const auto h = static_cast<unsigned char>(hex[i * 2]);
        const auto l = static_cast<unsigned char>(hex[i * 2 + 1]);
        if (h >= '0' && h <= '9') hi = h - '0';
        else if (h >= 'a' && h <= 'f') hi = h - 'a' + 10;
        else if (h >= 'A' && h <= 'F') hi = h - 'A' + 10;
        else return domain::BNHash{};
        if (l >= '0' && l <= '9') lo = l - '0';
        else if (l >= 'a' && l <= 'f') lo = l - 'a' + 10;
        else if (l >= 'A' && l <= 'F') lo = l - 'A' + 10;
        else return domain::BNHash{};
        bytes[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return domain::BNHash{bytes};
}

/// Encode a BNHash to a 40-character lowercase hex string.
std::string bn_hash_to_hex(const domain::BNHash& hash) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(40);
    for (auto b : hash.bytes()) {
        out.push_back(kHex[(b >> 4) & 0xF]);
        out.push_back(kHex[b & 0xF]);
    }
    return out;
}

/// Build the account file path for a given username.
std::filesystem::path account_path(const std::string& data_dir,
                                   std::string_view username) {
    return std::filesystem::path{data_dir} / (std::string{username} + ".plain");
}

}  // namespace

// ---------------------------------------------------------------------------
// Construction / bulk load
// ---------------------------------------------------------------------------

FileAccountRepository::FileAccountRepository(std::string_view data_dir)
    : data_dir_(data_dir),
      cache_(std::make_unique<inmemory::InMemoryAccountRepository>()) {
    load_all();
}

void FileAccountRepository::load_all() {
    if (!std::filesystem::exists(data_dir_)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".plain") {
            auto account = load_account_file(entry.path().filename().string());
            if (account) {
                // Best-effort warm of the in-memory cache during load; a
                // failure here is non-fatal and intentionally discarded.
                (void)cache_->save(*account);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// R314 — parse a single .plain file
// ---------------------------------------------------------------------------

std::optional<domain::identity::Account> FileAccountRepository::load_account_file(
    std::string_view filename) {
    // Build full path: data_dir_ / filename
    const auto path = std::filesystem::path{data_dir_} / filename;

    // Read file content
    std::ifstream ifs{path};
    if (!ifs.is_open()) {
        return std::nullopt;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    const std::string content = oss.str();

    // Parse key=value map
    const auto kv = parse_account_file(content);

    // --- Required field: username ---
    const std::string username_str = get_field(kv, {"BNET", "acct", "username"});
    if (username_str.empty()) {
        return std::nullopt;
    }

    auto name_result = domain::UserName::parse(username_str);
    if (!name_result.has_value()) {
        return std::nullopt;
    }

    // --- Account ID: derive from filename (strip .plain suffix) ---
    // The legacy format stores the UID in "BNET\acct\userid"; fall back to 0.
    const auto uid_raw = get_numeric_field(kv, {"BNET", "acct", "userid"});
    const auto account_id = domain::AccountId{
        static_cast<std::uint32_t>(uid_raw > 0 ? uid_raw : 0)};

    // --- Password hash (40-hex BNet hash1) ---
    const std::string hash_hex = get_field(kv, {"BNET", "acct", "passhash1"});
    const domain::BNHash password = bn_hash_from_hex(hash_hex);

    // --- Locale ---
    const std::string locale_str = get_field(kv, {"BNET", "acct", "locale"});
    const domain::Locale locale = domain::Locale::parse_or_default(locale_str);

    // --- Command groups (legacy bitmask stored as integer) ---
    domain::identity::CommandGroupMask groups;
    const auto cg_raw = get_numeric_field(kv, {"BNET", "acct", "auth_command_groups"});
    if (cg_raw > 0) {
        for (std::uint8_t g = 1; g <= domain::identity::CommandGroupMask::kBits; ++g) {
            if (cg_raw & (1 << (g - 1))) {
                groups.grant(g);
            }
        }
    } else {
        // Default: grant group 1
        groups.grant(1);
    }

    // --- Locked flag ---
    const auto lock_raw = get_numeric_field(kv, {"BNET", "acct", "auth_lock"});
    const bool locked = (lock_raw != 0);

    // Rehydrate — no events emitted
    return domain::identity::Account::rehydrate(
        account_id,
        std::move(name_result.value()),
        password,
        locale,
        groups,
        std::nullopt,  // ban — not stored in .plain format
        locked,
        false          // must_change_password — not stored in .plain format
    );
}

// ---------------------------------------------------------------------------
// R315 — atomic write-back to disk
// ---------------------------------------------------------------------------

core::Status<> FileAccountRepository::save(
    const domain::identity::Account& account) {
    // 1. Update in-memory cache first (under write lock)
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        auto status = cache_->save(account);
        if (!status.has_value()) {
            return status;
        }
    }

    // 2. Serialize to key=value text
    std::ostringstream oss;
    oss << "BNET\\acct\\username=" << account.name().display() << "\n";
    oss << "BNET\\acct\\passhash1=" << bn_hash_to_hex(account.password_hash1()) << "\n";
    oss << "BNET\\acct\\auth_lock=" << (account.is_locked() ? "1" : "0") << "\n";

    // Command groups bitmask
    std::uint32_t cg_mask = 0;
    for (std::uint8_t g = 1; g <= domain::identity::CommandGroupMask::kBits; ++g) {
        if (account.command_groups().has(g)) {
            cg_mask |= (1u << (g - 1));
        }
    }
    oss << "BNET\\acct\\auth_command_groups=" << cg_mask << "\n";
    oss << "BNET\\acct\\locale=" << account.locale().text() << "\n";
    oss << "BNET\\acct\\userid=" << account.id().value() << "\n";

    const std::string content = oss.str();

    // 3. Determine paths
    const std::string username{account.name().display()};
    const auto final_path = account_path(data_dir_, username);
    const auto tmp_path   = std::filesystem::path{data_dir_} /
                            (username + ".plain.tmp");

    // 4. Write to .tmp file
    {
        // path::c_str() is wchar_t* on Windows; ::open takes char*, so use the
        // narrow string form (valid on both platforms).
        const std::string tmp_path_str = tmp_path.string();
        const int fd = ::open(tmp_path_str.c_str(),
                              O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                              0600);
        if (fd < 0) {
            return core::fail(core::Error{
                core::StatusCode::Internal,
                std::string{"file: open tmp failed: "} + std::strerror(errno)});
        }

        std::size_t written = 0;
        while (written < content.size()) {
            const auto n = ::write(fd,
                                   content.data() + written,
                                   content.size() - written);
            if (n < 0) {
                ::close(fd);
                return core::fail(core::Error{
                    core::StatusCode::Internal,
                    std::string{"file: write failed: "} + std::strerror(errno)});
            }
            written += static_cast<std::size_t>(n);
        }

        // 5. fsync before rename (Windows: _commit flushes the fd to disk)
#if defined(_WIN32)
        if (::_commit(fd) != 0) {
#else
        if (::fsync(fd) != 0) {
#endif
            ::close(fd);
            return core::fail(core::Error{
                core::StatusCode::Internal,
                std::string{"file: fsync failed: "} + std::strerror(errno)});
        }
        ::close(fd);
    }

    // 6. Atomic rename .tmp → .plain
    std::error_code ec;
    std::filesystem::rename(tmp_path, final_path, ec);
    if (ec) {
        return core::fail(core::Error{
            core::StatusCode::Internal,
            std::string{"file: rename failed: "} + ec.message()});
    }

    return core::ok();
}

// ---------------------------------------------------------------------------
// R315 — remove: delete .plain file from disk
// ---------------------------------------------------------------------------

core::Status<> FileAccountRepository::remove(domain::AccountId id) {
    // Look up the account name before removing from cache
    std::string username;
    {
        std::shared_lock<std::shared_mutex> rlock(cache_mutex_);
        auto result = cache_->find_by_id(id);
        if (!result.has_value()) {
            return core::fail(result.error());
        }
        username = std::string{result.value().name().display()};
    }

    // Remove from in-memory cache
    {
        std::unique_lock<std::shared_mutex> wlock(cache_mutex_);
        auto status = cache_->remove(id);
        if (!status.has_value()) {
            return status;
        }
    }

    // Delete the .plain file from disk (best-effort; ignore ENOENT)
    const auto path = account_path(data_dir_, username);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec && ec.value() != ENOENT) {
        return core::fail(core::Error{
            core::StatusCode::Internal,
            std::string{"file: remove failed: "} + ec.message()});
    }

    return core::ok();
}

// ---------------------------------------------------------------------------
// Read-only accessors (delegate to cache)
// ---------------------------------------------------------------------------

core::Result<domain::identity::Account>
FileAccountRepository::find_by_id(domain::AccountId id) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->find_by_id(id);
}

core::Result<domain::identity::Account>
FileAccountRepository::find_by_name(const domain::UserName& name) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->find_by_name(name);
}

void FileAccountRepository::forEach(
    std::function<bool(const domain::identity::Account&)> predicate) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    cache_->forEach(predicate);
}

std::size_t FileAccountRepository::size() const noexcept {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->size();
}

}  // namespace pvpgn::infra::file
