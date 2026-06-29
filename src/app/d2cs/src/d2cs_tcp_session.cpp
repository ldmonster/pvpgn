// SPDX-License-Identifier: GPL-2.0-or-later

/// @file d2cs_tcp_session.cpp
/// Implementation of `D2CSTcpSession`.
///
/// Egress serialisation uses the static packet-builder methods on
/// `D2CSSessionFsm` (and the byte-accurate `ladderreply` encoder) for the
/// client-facing replies. The game-lobby replies (create/join/list/info game)
/// have no egress path yet — they require the D2CS<->D2GS server link and a
/// d2cs game store, deferred as a subsystem feature.

#include "app/d2cs/d2cs_tcp_session.hpp"

// Full definition of TcpSession — only available in the binary (infra_net).
// The header uses a forward declaration to keep app_d2cs Boost-free.
#include "infra/net/tcp_session.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <atomic>
#include <cstring>

#include "core/bytes.hpp"
#include "domain/d2cs/types.hpp"
#include "protocol/d2cs/fsm.hpp"
#include "protocol/d2cs/wire_types.hpp"
#include "protocol/d2cs/ladderreply_encoder.hpp"

namespace pvpgn::app::d2cs {

namespace {

// Shared realm secret used to derive/validate the d2cs LOGINREQ token
// (blizzard_hash(key ‖ account ‖ sessionnum ‖ seqno)). A realm-join issues the
// token with this key; the d2cs validates it here. Mirrors the original's
// session-bound auth without a live bnetd link. Mock clients use the same key.
constexpr const char* kRealmKey = "pvpgn-v3-d2cs-realm-secret-v1";

// Realm name echoed to a D2GS in the AUTHREQ handshake (informational).
constexpr const char* kRealmName = "pvpgn-v3";

// D2GS server-to-server link constants (d2cs_d2gs_protocol.h).
constexpr std::uint8_t  kInitClassD2cs        = 0x01;
constexpr std::uint8_t  kInitClassD2gs        = 0x64;
constexpr std::uint16_t kD2gsAuthReq          = 0x10;  // d2cs -> d2gs
constexpr std::uint16_t kD2gsAuthReply        = 0x11;  // both directions
constexpr std::uint16_t kD2gsSetGsInfo        = 0x12;  // d2gs -> d2cs (maxgame)
constexpr std::uint16_t kD2gsCreateGameReq    = 0x20;  // d2cs<->d2gs creategame
constexpr std::uint16_t kD2gsJoinGameReq      = 0x21;  // d2cs<->d2gs joingame
constexpr std::uint32_t kD2gsAuthReplySucceed = 0x00;
constexpr std::uint32_t kD2gsCreateGameSucceed = 0x00;
constexpr std::uint32_t kD2gsJoinGameSucceed   = 0x00;
constexpr std::size_t   kD2gsHeaderSize       = 8;     // size(2)+type(2)+seqno(4)

// Monotonic session-number source for D2GS links (mirrors the original's
// per-connection sessionnum). Relaxed: only uniqueness matters.
std::atomic<std::uint32_t> g_d2gs_sessionnum{0};

// Portrait constants mirror the legacy d2cs encoding (d2charfile.cpp /
// d2cs_d2gs_character.h):
//   header = 0x8084 (LE bytes 0x84 0x80), gfx/color/u2 pad = 0xFF,
//   u1 = 0x80 (MASK), status carries the 0x80 MASK bit, chclass = class+1,
//   ladder = 1 (ladder) or 0xFF (pad).
constexpr uint8_t kPortraitPadByte = 0xFF;
constexpr uint8_t kPortraitMask    = 0x80;

// Build the 33 wire bytes of a character's portrait block. The legacy server
// appends the portrait with `packet_append_string`, i.e. the bytes up to (but
// not including) the struct's trailing `end` (0x00) byte, followed by a NUL.
// The `charlistreply` encoder appends that trailing NUL itself, so we emit the
// 33 leading bytes here.
std::vector<std::byte> build_portrait(const domain::d2cs::CharacterInfo& c) {
    std::vector<std::byte> p;
    p.reserve(33);
    auto push = [&p](uint8_t b) { p.push_back(static_cast<std::byte>(b)); };

    push(0x84);                               // header low
    push(0x80);                               // header high
    for (int i = 0; i < 11; ++i) push(kPortraitPadByte);  // gfx[11]
    push(static_cast<uint8_t>(static_cast<uint8_t>(c.class_) + 1));  // chclass
    for (int i = 0; i < 11; ++i) push(kPortraitPadByte);  // color[11]
    push(c.level);                            // level
    push(static_cast<uint8_t>(static_cast<uint8_t>(c.flags) | kPortraitMask));  // status
    for (int i = 0; i < 3; ++i) push(kPortraitMask);      // u1[3]
    const bool is_ladder =
        domain::d2cs::has_flag(c.flags, domain::d2cs::CharacterFlags::Ladder);
    push(is_ladder ? 0x01 : kPortraitPadByte); // ladder
    for (int i = 0; i < 2; ++i) push(kPortraitPadByte);   // u2[2]
    // The trailing `end` (0x00) byte is supplied by the encoder's NUL.
    return p;
}

}  // namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

D2CSTcpSession::D2CSTcpSession(std::shared_ptr<infra::net::TcpSession> tcp,
                               std::shared_ptr<D2gsRegistry> registry)
    : tcp_(std::move(tcp))
    , char_repo_{}
    , ladder_repo_{}
    , handler_(std::make_unique<D2CSSessionHandler>(
          char_repo_, ladder_repo_, *this, std::string{kRealmKey}))
    , registry_(std::move(registry))
{
    // Intercept CREATEGAMEREQ so the session can route it across to a D2GS link
    // via the shared registry (the handler stub cannot reach other sessions).
    // Owned by this session, so the FSM cannot outlive `this` — raw capture is
    // safe. With no registry the route falls back to a FAILED reply.
    auto cb = handler_->make_callbacks();
    cb.on_create_game =
        [this](const protocol::d2cs::D2CSCreateGameRequest& req) {
            return route_create_game(req);
        };
    cb.on_join_game =
        [this](const protocol::d2cs::D2CSJoinGameRequest& req) {
            return route_join_game(req);
        };
    fsm_ = std::make_unique<protocol::d2cs::D2CSSessionFsm>(std::move(cb));
}

// ---------------------------------------------------------------------------
// start()
// ---------------------------------------------------------------------------

void D2CSTcpSession::start() {
    auto self = shared_from_this();

    tcp_->set_on_bytes([self](core::ByteView bv) {
        const auto* data = reinterpret_cast<const uint8_t*>(bv.data());
        size_t      size = bv.size();

        // The connection opens with a single init class byte
        // (CLIENT_INITCONN_CLASS_D2CS = 0x01) before any framed packet, like
        // the BNCS/BNFTP listeners. Consume it once; anything else is a bad
        // connection class and the original drops the connection.
        if (!self->init_consumed_) {
            if (size == 0) return;            // wait for the byte
            const std::uint8_t init_class = data[0];
            if (init_class == kInitClassD2gs) {
                // A game server link: switch to the D2GS handshake and begin it
                // by sending AUTHREQ.
                self->init_consumed_ = true;
                self->d2gs_link_     = true;
                ++data;
                --size;
                self->start_d2gs_link();
                if (size == 0) return;        // init byte arrived alone
                self->feed_d2gs(data, size);
                return;
            }
            if (init_class != kInitClassD2cs) {
                std::cerr << "[d2cs] bad init class byte: "
                          << static_cast<int>(init_class) << "\n";
                self->tcp_->close();
                return;
            }
            self->init_consumed_ = true;
            ++data;
            --size;
            if (size == 0) return;            // init byte arrived alone
        }

        if (self->d2gs_link_) {
            self->feed_d2gs(data, size);
            return;
        }

        // Feed the remaining raw bytes into the FSM reassembly buffer.
        auto result = self->fsm_->feed(data, size);
        if (!result) {
            // Malformed packet or callback error — close the session.
            std::cerr << "[d2cs] session error: "
                      << result.error().message() << "\n";
            self->tcp_->close();
        }
    });

    tcp_->set_on_close([self](const boost::system::error_code& ec) {
        if (ec && ec != boost::asio::error::eof) {
            std::cerr << "[d2cs] session closed: " << ec.message() << "\n";
        }
        // Drop this D2GS link from the registry so it is no longer choosable.
        if (self->registry_ && self->d2gs_registered_) {
            self->registry_->remove_d2gs(self.get());
        }
        // FSM disconnect callback fires via on_disconnect in callbacks.
    });

    tcp_->start();
}

// ---------------------------------------------------------------------------
// send_raw() — internal helper
// ---------------------------------------------------------------------------

void D2CSTcpSession::send_raw(std::vector<uint8_t> bytes) {
    if (!tcp_) return;
    std::vector<std::byte> buf;
    buf.reserve(bytes.size());
    for (auto b : bytes) {
        buf.push_back(static_cast<std::byte>(b));
    }
    tcp_->send(std::move(buf));
}

// ---------------------------------------------------------------------------
// D2GS server-to-server link handshake
// ---------------------------------------------------------------------------

void D2CSTcpSession::send_d2gs_frame(std::uint16_t type, std::uint32_t seqno,
                                     const std::vector<uint8_t>& body) {
    const std::size_t total = kD2gsHeaderSize + body.size();
    std::vector<uint8_t> pkt;
    pkt.reserve(total);
    auto push16 = [&pkt](std::uint16_t v) {
        pkt.push_back(static_cast<uint8_t>(v & 0xFF));
        pkt.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    };
    auto push32 = [&pkt](std::uint32_t v) {
        pkt.push_back(static_cast<uint8_t>(v & 0xFF));
        pkt.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        pkt.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        pkt.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    push16(static_cast<std::uint16_t>(total));
    push16(type);
    push32(seqno);
    pkt.insert(pkt.end(), body.begin(), body.end());
    send_raw(std::move(pkt));
}

void D2CSTcpSession::start_d2gs_link() {
    d2gs_sessionnum_ = g_d2gs_sessionnum.fetch_add(1, std::memory_order_relaxed) + 1;

    // AUTHREQ (0x10): sessionnum(u32), signlen(u32 = 0), realmname + NUL.
    std::vector<uint8_t> body;
    auto push32 = [&body](std::uint32_t v) {
        body.push_back(static_cast<uint8_t>(v & 0xFF));
        body.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        body.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        body.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    push32(d2gs_sessionnum_);
    push32(0);  // signlen
    for (const char* p = kRealmName; *p; ++p) {
        body.push_back(static_cast<uint8_t>(*p));
    }
    body.push_back(0);  // realmname NUL
    send_d2gs_frame(kD2gsAuthReq, /*seqno*/ 0, body);
}

void D2CSTcpSession::feed_d2gs(const uint8_t* data, std::size_t size) {
    d2gs_buf_.insert(d2gs_buf_.end(), data, data + size);

    // Parse complete [size:2][type:2][seqno:4] frames.
    while (d2gs_buf_.size() >= kD2gsHeaderSize) {
        const std::uint16_t fsize =
            static_cast<std::uint16_t>(d2gs_buf_[0] | (d2gs_buf_[1] << 8));
        if (fsize < kD2gsHeaderSize) {
            std::cerr << "[d2cs] d2gs bad frame size " << fsize << "\n";
            tcp_->close();
            return;
        }
        if (d2gs_buf_.size() < fsize) break;  // wait for the rest

        const std::uint16_t type =
            static_cast<std::uint16_t>(d2gs_buf_[2] | (d2gs_buf_[3] << 8));
        const std::uint32_t seqno =
            static_cast<std::uint32_t>(d2gs_buf_[4]) |
            (static_cast<std::uint32_t>(d2gs_buf_[5]) << 8) |
            (static_cast<std::uint32_t>(d2gs_buf_[6]) << 16) |
            (static_cast<std::uint32_t>(d2gs_buf_[7]) << 24);

        if (type == kD2gsAuthReply) {
            // D2GS authenticated. With version/checksum validation disabled
            // (the v3 default), always accept. Reply AUTHREPLY(SUCCEED).
            std::vector<uint8_t> body = {
                static_cast<uint8_t>(kD2gsAuthReplySucceed & 0xFF),
                static_cast<uint8_t>((kD2gsAuthReplySucceed >> 8) & 0xFF),
                static_cast<uint8_t>((kD2gsAuthReplySucceed >> 16) & 0xFF),
                static_cast<uint8_t>((kD2gsAuthReplySucceed >> 24) & 0xFF),
            };
            send_d2gs_frame(kD2gsAuthReply, /*seqno*/ 0, body);
        } else if (type == kD2gsSetGsInfo) {
            // The D2GS reported its capacity (maxgame>0) and is now choosable
            // for game creation. Register it once.
            if (registry_ && !d2gs_registered_) {
                d2gs_registered_ = true;
                registry_->add_d2gs(weak_from_this());
            }
        } else if (type == kD2gsCreateGameReq) {
            // CREATEGAMEREPLY (0x20) from the D2GS: result(u32) + gameid(u32).
            // Correlate back to the waiting client via the echoed frame seqno.
            std::uint32_t result = kD2gsCreateGameSucceed;
            std::uint32_t gameid = 0;
            if (fsize >= kD2gsHeaderSize + 8) {
                const auto* b = d2gs_buf_.data() + kD2gsHeaderSize;
                result = b[0] | (b[1] << 8) | (b[2] << 16) |
                         (static_cast<std::uint32_t>(b[3]) << 24);
                gameid = b[4] | (b[5] << 8) | (b[6] << 16) |
                         (static_cast<std::uint32_t>(b[7]) << 24);
            }
            (void)gameid;  // the D2GS's internal id; d2cs assigns its own below
            if (registry_) {
                if (auto pending = registry_->take_pending(seqno)) {
                    if (auto client = pending->client.lock()) {
                        const bool ok = (result == kD2gsCreateGameSucceed);
                        // d2cs assigns the client-facing game id (like the
                        // original's d2cs_game_get_id), not the D2GS's internal one.
                        const std::uint32_t client_gameid =
                            ok ? registry_->next_game_id() : 0u;
                        const std::uint32_t client_result =
                            ok ? protocol::d2cs::wire::kCreateGameReplySucceed
                               : protocol::d2cs::wire::kCreateGameReplyFailed;
                        if (ok) {
                            // Record the game so a later JOINGAMEREQ finds its
                            // host (this D2GS-link session).
                            registry_->add_game(pending->game_name, client_gameid,
                                                weak_from_this());
                        }
                        client->send_create_game_reply(
                            pending->client_seqno, client_gameid, client_result);
                    }
                }
            }
        } else if (type == kD2gsJoinGameReq) {
            // JOINGAMEREPLY (0x21) from the D2GS: result(u32) + gameid(u32).
            std::uint32_t result = kD2gsJoinGameSucceed;
            std::uint32_t gameid = 0;
            if (fsize >= kD2gsHeaderSize + 8) {
                const auto* b = d2gs_buf_.data() + kD2gsHeaderSize;
                result = b[0] | (b[1] << 8) | (b[2] << 16) |
                         (static_cast<std::uint32_t>(b[3]) << 24);
                gameid = b[4] | (b[5] << 8) | (b[6] << 16) |
                         (static_cast<std::uint32_t>(b[7]) << 24);
            }
            if (registry_) {
                if (auto pending = registry_->take_pending(seqno)) {
                    if (auto client = pending->client.lock()) {
                        const bool ok = (result == kD2gsJoinGameSucceed);
                        const std::uint32_t client_result =
                            ok ? protocol::d2cs::wire::kJoinGameReplySucceed
                               : protocol::d2cs::wire::kJoinGameReplyFailed;
                        // The game server the client connects to is this D2GS
                        // link; report a placeholder addr/token (a later round
                        // plumbs the real gs endpoint).
                        client->send_join_game_reply(
                            pending->client_seqno, gameid,
                            /*gs_ip*/ 0x0100007Fu /*127.0.0.1*/,
                            /*token*/ gameid, client_result);
                    }
                }
            }
        }
        // Other D2GS->D2CS packets (ECHO/updategameinfo) are accepted + ignored.

        d2gs_buf_.erase(d2gs_buf_.begin(), d2gs_buf_.begin() + fsize);
    }
}

// ---------------------------------------------------------------------------
// Cross-session game-lobby routing
// ---------------------------------------------------------------------------

core::Result<void, core::Error> D2CSTcpSession::route_create_game(
    const protocol::d2cs::D2CSCreateGameRequest& req) {
    const auto client_seqno = static_cast<std::uint16_t>(req.seqno);

    std::shared_ptr<D2CSTcpSession> gs =
        registry_ ? registry_->choose_d2gs() : nullptr;
    if (!gs) {
        // No game server available — reply FAILED rather than hang the client.
        send_create_game_reply(client_seqno, 0,
                               protocol::d2cs::wire::kCreateGameReplyFailed);
        return core::Result<void, core::Error>();
    }

    const std::uint32_t corr =
        registry_->add_pending(weak_from_this(), client_seqno, req.game_name);

    // D2CS_D2GS_CREATEGAMEREQ (0x20): ladder, expansion, difficulty, hardcore
    // bytes, then gamename / pass / desc / acct / char / ip c-strings.
    std::vector<uint8_t> body;
    body.push_back(0);                                  // ladder
    body.push_back(static_cast<uint8_t>(req.expansion));
    body.push_back(static_cast<uint8_t>(req.difficulty));
    body.push_back(static_cast<uint8_t>(req.hardcore));
    auto put_cstr = [&body](const std::string& s) {
        body.insert(body.end(), s.begin(), s.end());
        body.push_back(0);
    };
    put_cstr(req.game_name);
    put_cstr(req.game_password);
    put_cstr(req.game_description);
    put_cstr("");           // create-by account  (plumbed in a later round)
    put_cstr("");           // create-by character
    put_cstr("127.0.0.1");  // create-by ip

    gs->send_d2gs_request(kD2gsCreateGameReq, corr, body);
    return core::Result<void, core::Error>();
}

void D2CSTcpSession::send_d2gs_request(std::uint16_t type, std::uint32_t corr,
                                       const std::vector<uint8_t>& body) {
    send_d2gs_frame(type, corr, body);
}

void D2CSTcpSession::send_create_game_reply(std::uint16_t client_seqno,
                                            std::uint32_t game_id,
                                            std::uint32_t result) {
    send_raw(protocol::d2cs::D2CSSessionFsm::make_create_game_reply(
        client_seqno, game_id, result));
}

core::Result<void, core::Error> D2CSTcpSession::route_join_game(
    const protocol::d2cs::D2CSJoinGameRequest& req) {
    const auto client_seqno = static_cast<std::uint16_t>(req.seqno);

    auto game = registry_ ? registry_->find_game(req.game_name) : std::nullopt;
    std::shared_ptr<D2CSTcpSession> gs = game ? game->gs.lock() : nullptr;
    if (!gs) {
        // Unknown game (or its host disconnected) -> FAILED.
        send_join_game_reply(client_seqno, 0, 0, 0,
                             protocol::d2cs::wire::kJoinGameReplyFailed);
        return core::Result<void, core::Error>();
    }

    const std::uint32_t corr =
        registry_->add_pending(weak_from_this(), client_seqno, req.game_name);

    // D2CS_D2GS_JOINGAMEREQ (0x21): gameid(u32) + token(u32) + char / acct / ip.
    std::vector<uint8_t> body;
    auto put_u32 = [&body](std::uint32_t v) {
        body.push_back(static_cast<uint8_t>(v & 0xFF));
        body.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        body.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        body.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    put_u32(game->gameid);
    put_u32(corr);  // token (correlation; a later round uses a real join token)
    auto put_cstr = [&body](const std::string& s) {
        body.insert(body.end(), s.begin(), s.end());
        body.push_back(0);
    };
    put_cstr("");           // character (plumbed in a later round)
    put_cstr("");           // account
    put_cstr("127.0.0.1");  // client ip

    gs->send_d2gs_request(kD2gsJoinGameReq, corr, body);
    return core::Result<void, core::Error>();
}

void D2CSTcpSession::send_join_game_reply(std::uint16_t client_seqno,
                                          std::uint32_t game_id,
                                          std::uint32_t gs_ip,
                                          std::uint32_t token,
                                          std::uint32_t result) {
    send_raw(protocol::d2cs::D2CSSessionFsm::make_join_game_reply(
        client_seqno, game_id, gs_ip, token, result));
}

// ---------------------------------------------------------------------------
// ID2CSSessionEgress implementation
// ---------------------------------------------------------------------------

void D2CSTcpSession::send_realm_logon_result(
    domain::d2cs::RealmLogonResult result) {
    const auto code = static_cast<uint32_t>(result);
    send_raw(protocol::d2cs::D2CSSessionFsm::make_login_reply(code));
}

void D2CSTcpSession::send_motd(std::string_view message) {
    send_raw(protocol::d2cs::D2CSSessionFsm::make_motd_reply(message));
}

// Default per-account character cap (legacy d2cs prefs_get_maxchar default).
static constexpr uint16_t kDefaultMaxChar = 8;

void D2CSTcpSession::send_char_list(
    const std::vector<domain::d2cs::CharacterInfo>& chars) {
    // Build the per-character entries (name + portrait block) for the
    // wire-accurate CHARLISTREPLY encoder.
    std::vector<protocol::d2cs::charlistreply::CharEntry> entries;
    entries.reserve(chars.size());
    for (const auto& c : chars) {
        protocol::d2cs::charlistreply::CharEntry e;
        e.charname = c.name;
        e.portrait = build_portrait(c);
        entries.push_back(std::move(e));
    }

    // `maxchar` doubles as the "new char allowed" signal: report the cap only
    // when there is still room, otherwise 0 (so the client disables Create).
    const uint16_t maxchar_field =
        (chars.size() < kDefaultMaxChar) ? kDefaultMaxChar : 0;

    send_raw(protocol::d2cs::D2CSSessionFsm::make_char_list_reply(
        maxchar_field, entries));
}

void D2CSTcpSession::send_char_list_110(
    const std::vector<domain::d2cs::CharacterInfo>& chars) {
    // Same as send_char_list but emits the 1.10+ reply (0x19) with a per-char
    // expire_time. With character expiry disabled (the default config,
    // char_expire_day = 0), the legacy server emits 0x7FFFFFFF for every char;
    // the in-memory v3 realm has no per-char last-access time, so it mirrors
    // that "never expires" sentinel.
    constexpr std::uint32_t kNeverExpires = 0x7FFFFFFFu;

    std::vector<protocol::d2cs::charlistreply::CharEntry> entries;
    entries.reserve(chars.size());
    for (const auto& c : chars) {
        protocol::d2cs::charlistreply::CharEntry e;
        e.charname    = c.name;
        e.portrait    = build_portrait(c);
        e.expire_time = kNeverExpires;
        entries.push_back(std::move(e));
    }

    const uint16_t maxchar_field =
        (chars.size() < kDefaultMaxChar) ? kDefaultMaxChar : 0;

    send_raw(protocol::d2cs::D2CSSessionFsm::make_char_list_reply_110(
        maxchar_field, entries));
}

void D2CSTcpSession::send_char_list_result(bool success) {
    // Send an empty char list on failure (client will show "no characters").
    if (!success) {
        send_raw(protocol::d2cs::D2CSSessionFsm::make_char_list_reply(
            kDefaultMaxChar, {}));
    }
    // On success the caller should have called send_char_list() instead.
}

void D2CSTcpSession::send_char_select_result(
    bool success,
    const domain::d2cs::CharacterInfo* /*info*/) {
    // result_code: 0x00 = success, 0x46 = not found
    const uint32_t code = success ? 0x00u : 0x46u;
    send_raw(protocol::d2cs::D2CSSessionFsm::make_char_login_reply(code));
}

void D2CSTcpSession::send_char_create_result(
    domain::d2cs::CharacterCreateResult result) {
    // Map to the original CREATECHARREPLY wire codes: SUCCEED 0x00,
    // ALREADY_EXIST 0x14 (bad name OR duplicate), FAILED 0x01.
    uint32_t code = 0x01u;  // Failed
    switch (result) {
        case domain::d2cs::CharacterCreateResult::Succeed:  code = 0x00u; break;
        case domain::d2cs::CharacterCreateResult::Rejected: code = 0x14u; break;
        case domain::d2cs::CharacterCreateResult::Failed:   code = 0x01u; break;
    }
    send_raw(protocol::d2cs::D2CSSessionFsm::make_create_char_reply(code));
}

void D2CSTcpSession::send_char_delete_result(bool success) {
    // result_code: 0x00 = success, 0x01 = failed
    const uint32_t code = success ? 0x00u : 0x01u;
    send_raw(protocol::d2cs::D2CSSessionFsm::make_delete_char_reply(code));
}

void D2CSTcpSession::send_ladder(
    const std::vector<domain::d2cs::LadderEntry>& entries) {
    // LADDERREPLY (0x11). The legacy d2cs_send_client_ladder sends NO packet
    // when the ladder is empty (npacket == 0); only the charladderreq lookup-
    // miss path emits a 10-byte all-zero reply. The previous 3-byte header-only
    // stub here was malformed (a real client underflows the 10-byte
    // t_d2cs_client_ladderreply struct). Wire the byte-accurate encoder, and
    // match the oracle's "empty -> silent" behaviour for the regular request.
    namespace lr = protocol::d2cs::ladderreply;

    if (entries.empty()) {
        return;  // oracle sends nothing for an empty regular LADDERREQ
    }

    std::vector<lr::LadderInfo> infos;
    infos.reserve(entries.size());
    for (const auto& e : entries) {
        lr::LadderInfo li{};
        li.exp_low  = e.experience;
        li.exp_high = 0;
        li.status   = 0;  // domain LadderEntry carries no status flags
        li.level    = e.level;
        li.u1       = 0;
        li.charname.fill('\0');
        const std::size_t n =
            std::min(li.charname.size(), e.character_name.size());
        std::memcpy(li.charname.data(), e.character_name.data(), n);
        infos.push_back(li);
    }

    // start_pos is the absolute (0-based) position of the first entry; derive
    // it from the first entry's 1-based rank (0 = unranked -> top of list).
    const std::uint16_t start_pos =
        (entries.front().rank > 0)
            ? static_cast<std::uint16_t>(entries.front().rank - 1)
            : 0;

    for (const auto& pkt : lr::encode(/*type=*/0, start_pos, infos)) {
        std::vector<std::uint8_t> raw;
        raw.reserve(pkt.bytes.size());
        for (std::byte b : pkt.bytes) raw.push_back(static_cast<std::uint8_t>(b));
        send_raw(std::move(raw));
    }
}

} // namespace pvpgn::app::d2cs
