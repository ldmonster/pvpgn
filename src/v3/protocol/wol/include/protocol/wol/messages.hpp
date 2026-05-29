// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages.hpp
/// WOL (Westwood Online) protocol messages — pure value types, no logic.
///
/// WOL is an IRC-like text protocol used by Command & Conquer, Red Alert,
/// Tiberian Sun, and other Westwood/EA games. It is line-oriented (\r\n
/// terminated) and uses IRC-style commands with WOL-specific extensions.
///
/// Each struct corresponds to one WOL command. The wire format is the
/// text line itself; these structs hold the parsed fields.
///
/// **No dependency on domain types** — `std::string`, integers only.
/// Conversions to/from domain types happen in the FSM, not here.
///
/// Client commands handled:
///   NICK, USER, PASS, PING, PONG, QUIT,
///   LIST, JOIN, PART, PRIVMSG,
///   CVERS, VERCHK, APGAR, SETOPT, SERIAL,
///   GAMEOPT, STARTG, JOINGAME, FINDUSER, FINDUSEREX,
///   PAGE, ADVERTR, ADVERTC, CHANCHK, GETBUDDY,
///   ADDBUDDY, DELBUDDY, HOST, INVMSG, INVDEL,
///   USERIP, SQUADINFO, CLANBYNAME, SETCODEPAGE,
///   GETCODEPAGE, SETLOCALE, GETLOCALE, GETINSIDER,
///   LISTSEARCH, RUNGSEARCH, HIGHSCORE, NAMES,
///   TOPIC, TIME, KICK, MODE
///
/// Server reply types:
///   NumericReply (IRC-style NNN codes), RawLine

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace pvpgn::protocol::wol {

// ---------------------------------------------------------------------------
// WOL command name constants (wire text, upper-cased)
// ---------------------------------------------------------------------------

inline constexpr const char* kCmdNick        = "NICK";
inline constexpr const char* kCmdUser        = "USER";
inline constexpr const char* kCmdPass        = "PASS";
inline constexpr const char* kCmdPing        = "PING";
inline constexpr const char* kCmdPong        = "PONG";
inline constexpr const char* kCmdQuit        = "QUIT";
inline constexpr const char* kCmdList        = "LIST";
inline constexpr const char* kCmdJoin        = "JOIN";
inline constexpr const char* kCmdPart        = "PART";
inline constexpr const char* kCmdPrivmsg     = "PRIVMSG";
// WOL-specific client commands
inline constexpr const char* kCmdCvers       = "CVERS";
inline constexpr const char* kCmdVerchk      = "VERCHK";
inline constexpr const char* kCmdApgar       = "APGAR";
inline constexpr const char* kCmdSetopt      = "SETOPT";
inline constexpr const char* kCmdSerial      = "SERIAL";
inline constexpr const char* kCmdGameopt     = "GAMEOPT";
inline constexpr const char* kCmdStartg      = "STARTG";
inline constexpr const char* kCmdJoingame    = "JOINGAME";
inline constexpr const char* kCmdFinduser    = "FINDUSER";
inline constexpr const char* kCmdFinduserex  = "FINDUSEREX";
inline constexpr const char* kCmdPage        = "PAGE";
inline constexpr const char* kCmdAdvertr     = "ADVERTR";
inline constexpr const char* kCmdAdvertc     = "ADVERTC";
inline constexpr const char* kCmdChanchk     = "CHANCHK";
inline constexpr const char* kCmdGetbuddy    = "GETBUDDY";
inline constexpr const char* kCmdAddbuddy    = "ADDBUDDY";
inline constexpr const char* kCmdDelbuddy    = "DELBUDDY";
inline constexpr const char* kCmdHost        = "HOST";
inline constexpr const char* kCmdInvmsg      = "INVMSG";
inline constexpr const char* kCmdInvdel      = "INVDEL";
inline constexpr const char* kCmdUserip      = "USERIP";
inline constexpr const char* kCmdSquadinfo   = "SQUADINFO";
inline constexpr const char* kCmdClanbyname  = "CLANBYNAME";
inline constexpr const char* kCmdSetcodepage = "SETCODEPAGE";
inline constexpr const char* kCmdGetcodepage = "GETCODEPAGE";
inline constexpr const char* kCmdSetlocale   = "SETLOCALE";
inline constexpr const char* kCmdGetlocale   = "GETLOCALE";
inline constexpr const char* kCmdGetinsider  = "GETINSIDER";
inline constexpr const char* kCmdListsearch  = "LISTSEARCH";
inline constexpr const char* kCmdRungsearch  = "RUNGSEARCH";
inline constexpr const char* kCmdHighscore   = "HIGHSCORE";
inline constexpr const char* kCmdNames       = "NAMES";
inline constexpr const char* kCmdTopic       = "TOPIC";
inline constexpr const char* kCmdTime        = "TIME";
inline constexpr const char* kCmdKick        = "KICK";
inline constexpr const char* kCmdMode        = "MODE";

// ---------------------------------------------------------------------------
// IRC numeric reply codes used by WOL
// ---------------------------------------------------------------------------

inline constexpr int kRplWelcome        = 1;    ///< 001 RPL_WELCOME
inline constexpr int kRplYourhost       = 2;    ///< 002 RPL_YOURHOST
inline constexpr int kRplMotdStart      = 375;  ///< 375 RPL_MOTDSTART
inline constexpr int kRplEndOfMotd      = 376;  ///< 376 RPL_ENDOFMOTD
inline constexpr int kRplListStart      = 321;  ///< 321 RPL_LISTSTART
inline constexpr int kRplList           = 322;  ///< 322 RPL_LIST
inline constexpr int kRplListEnd        = 323;  ///< 323 RPL_LISTEND
inline constexpr int kRplTopic          = 332;  ///< 332 RPL_TOPIC
inline constexpr int kRplEndOfNames     = 366;  ///< 366 RPL_ENDOFNAMES
inline constexpr int kErrNotRegistered  = 451;  ///< 451 ERR_NOTREGISTERED
inline constexpr int kErrNeedMoreParams = 461;  ///< 461 ERR_NEEDMOREPARAMS
inline constexpr int kErrNoNickGiven    = 431;  ///< 431 ERR_NONICKNAMEGIVEN
inline constexpr int kErrNotOnChannel   = 442;  ///< 442 ERR_NOTONCHANNEL
inline constexpr int kErrNoRecipient    = 411;  ///< 411 ERR_NORECIPIENT
inline constexpr int kErrUnknownCommand = 421;  ///< 421 ERR_UNKNOWNCOMMAND

// ---------------------------------------------------------------------------
// Client → Server message structs
// ---------------------------------------------------------------------------

/// NICK <nickname>
/// Sent first to set the client's nickname.
struct Nick {
    std::string nickname;  ///< Desired nickname

    bool operator==(const Nick&) const = default;
};

/// USER <username> <hostname> <servername> :<realname>
/// Sent during authentication to provide user info.
struct User {
    std::string username;    ///< Login username
    std::string hostname;    ///< Client hostname (often "0" or actual hostname)
    std::string servername;  ///< Server name (often "*")
    std::string realname;    ///< Real name / display name (trailing param)

    bool operator==(const User&) const = default;
};

/// PASS <password>
/// Sent during authentication to provide the password.
struct Pass {
    std::string password;  ///< Password (may be hashed by WOL client)

    bool operator==(const Pass&) const = default;
};

/// PING <token>
/// Keepalive from client; server responds with PONG :<token>.
struct Ping {
    std::string token;  ///< Ping token to echo back

    bool operator==(const Ping&) const = default;
};

/// PONG :<token>
/// Client response to a server PING.
struct Pong {
    std::string token;  ///< Token echoed from server PING

    bool operator==(const Pong&) const = default;
};

/// QUIT [:<reason>]
/// Client requests graceful disconnect.
struct Quit {
    std::string reason;  ///< Optional quit reason (may be empty)

    bool operator==(const Quit&) const = default;
};

/// LIST
/// Request the list of available channels/lobbies.
struct List {
    bool operator==(const List&) const = default;
};

/// JOIN #<channel>
/// Join a channel or game lobby.
struct Join {
    std::string channel;  ///< Channel name (including '#' prefix)

    bool operator==(const Join&) const = default;
};

/// PART #<channel> [:<reason>]
/// Leave a channel.
struct Part {
    std::string channel;  ///< Channel name to leave
    std::string reason;   ///< Optional part reason

    bool operator==(const Part&) const = default;
};

/// PRIVMSG <target> :<message>
/// Send a private or channel message.
struct Privmsg {
    std::string target;   ///< Recipient: nick or #channel
    std::string message;  ///< Message text (trailing param)

    bool operator==(const Privmsg&) const = default;
};

/// CVERS <client_id> <version>
/// WOL-specific: client version announcement.
struct Cvers {
    std::string client_id;  ///< Game/client identifier
    std::string version;    ///< Version string

    bool operator==(const Cvers&) const = default;
};

/// VERCHK <client_id> <version>
/// WOL-specific: version check request.
struct Verchk {
    std::string client_id;  ///< Game/client identifier
    std::string version;    ///< Version string

    bool operator==(const Verchk&) const = default;
};

/// APGAR <password_hash> <flags>
/// WOL-specific: alternate password/authentication token.
struct Apgar {
    std::string password_hash;  ///< Hashed password or auth token
    std::string flags;          ///< Authentication flags

    bool operator==(const Apgar&) const = default;
};

/// SETOPT <option> <value>
/// WOL-specific: set a client option.
struct Setopt {
    std::string option;  ///< Option name
    std::string value;   ///< Option value

    bool operator==(const Setopt&) const = default;
};

/// SERIAL <serial_number>
/// WOL-specific: client serial number for CD-key validation.
struct Serial {
    std::string serial_number;  ///< Game serial number

    bool operator==(const Serial&) const = default;
};

/// GAMEOPT <options>
/// WOL-specific: game options for a hosted game.
struct Gameopt {
    std::string options;  ///< Game option string

    bool operator==(const Gameopt&) const = default;
};

/// STARTG <game_name> [<player>...]
/// WOL-specific: start a game with listed players.
struct Startg {
    std::string              game_name;  ///< Game/lobby name
    std::vector<std::string> players;   ///< List of player nicks

    bool operator==(const Startg&) const = default;
};

/// JOINGAME <game_name> <host_ip> <host_port>
/// WOL-specific: join an existing game.
struct Joingame {
    std::string game_name;  ///< Game/lobby name
    std::string host_ip;    ///< Host IP address
    std::string host_port;  ///< Host port number

    bool operator==(const Joingame&) const = default;
};

/// FINDUSER <nickname>
/// WOL-specific: find a user by nickname.
struct Finduser {
    std::string nickname;  ///< Nickname to search for

    bool operator==(const Finduser&) const = default;
};

/// PAGE <nickname> :<message>
/// WOL-specific: send a page/notification to a user.
struct Page {
    std::string nickname;  ///< Target nickname
    std::string message;   ///< Page message

    bool operator==(const Page&) const = default;
};

/// ADDBUDDY <nickname>
/// WOL-specific: add a user to the buddy list.
struct Addbuddy {
    std::string nickname;  ///< Nickname to add

    bool operator==(const Addbuddy&) const = default;
};

/// DELBUDDY <nickname>
/// WOL-specific: remove a user from the buddy list.
struct Delbuddy {
    std::string nickname;  ///< Nickname to remove

    bool operator==(const Delbuddy&) const = default;
};

/// GETBUDDY
/// WOL-specific: request the buddy list.
struct Getbuddy {
    bool operator==(const Getbuddy&) const = default;
};

/// Generic WOL-specific command that is acknowledged but not fully parsed.
/// Used for ADVERTR, ADVERTC, CHANCHK, HOST, INVMSG, INVDEL, USERIP,
/// SQUADINFO, CLANBYNAME, SETCODEPAGE, GETCODEPAGE, SETLOCALE, GETLOCALE,
/// GETINSIDER, LISTSEARCH, RUNGSEARCH, HIGHSCORE, NAMES, TOPIC, TIME,
/// KICK, MODE, FINDUSEREX.
struct WolCommand {
    std::string command;  ///< Upper-cased command name
    std::string params;   ///< Raw params string (everything after command)

    bool operator==(const WolCommand&) const = default;
};

// ---------------------------------------------------------------------------
// Server → Client message structs
// ---------------------------------------------------------------------------

/// IRC numeric reply: ":server NNN target :text\r\n"
struct NumericReply {
    std::string server_name;  ///< Server prefix (without ':')
    int         code = 0;     ///< Numeric reply code (1–999)
    std::string target;       ///< Target nick or channel
    std::string text;         ///< Reply text (trailing param)

    bool operator==(const NumericReply&) const = default;
};

/// Raw text line (server → client), e.g. PONG, JOIN echo, ERROR.
struct RawLine {
    std::string line;  ///< Full line text (without trailing \r\n)

    bool operator==(const RawLine&) const = default;
};

// ---------------------------------------------------------------------------
// Variant types
// ---------------------------------------------------------------------------

/// All possible decoded client → server messages.
using ClientMessage = std::variant<
    Nick,
    User,
    Pass,
    Ping,
    Pong,
    Quit,
    List,
    Join,
    Part,
    Privmsg,
    Cvers,
    Verchk,
    Apgar,
    Setopt,
    Serial,
    Gameopt,
    Startg,
    Joingame,
    Finduser,
    Page,
    Addbuddy,
    Delbuddy,
    Getbuddy,
    WolCommand  ///< Catch-all for known-but-unstructured WOL commands
>;

/// All possible decoded server → client messages.
using ServerMessage = std::variant<
    NumericReply,
    RawLine
>;

}  // namespace pvpgn::protocol::wol
