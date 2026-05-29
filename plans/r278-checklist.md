# R278 — WoL Codec with Typed Message Structs

## Status: COMPLETE

## Files Created
- `src/v3/protocol/wol/include/protocol/wol/messages.hpp`
- `src/v3/protocol/wol/include/protocol/wol/codec.hpp`
- `src/v3/protocol/wol/src/codec.cpp`
- `tests/unit/protocol/wol/codec_test.cpp`

## Files Modified
- `src/v3/CMakeLists.txt` — added `protocol/wol/src/codec.cpp` to `protocol_wol` SOURCES and `protocol_common` to PUBLIC_DEPS
- `tests/unit/protocol/wol/CMakeLists.txt` — added `test_protocol_wol_codec` target

## Message Types Implemented

### Client → Server (24 types in `ClientMessage` variant)

| Struct       | Command    | Opcode / Description                                      |
|--------------|------------|-----------------------------------------------------------|
| `Nick`       | `NICK`     | Set nickname (first message in auth flow)                 |
| `User`       | `USER`     | Provide username/hostname/realname                        |
| `Pass`       | `PASS`     | Provide password (completes auth with NICK+USER)          |
| `Ping`       | `PING`     | Keepalive from client                                     |
| `Pong`       | `PONG`     | Client response to server PING                            |
| `Quit`       | `QUIT`     | Graceful disconnect                                       |
| `List`       | `LIST`     | Request channel/lobby list                                |
| `Join`       | `JOIN`     | Join a channel or game lobby                              |
| `Part`       | `PART`     | Leave a channel                                           |
| `Privmsg`    | `PRIVMSG`  | Send a message to a channel or user                       |
| `Cvers`      | `CVERS`    | WOL: client version announcement                          |
| `Verchk`     | `VERCHK`   | WOL: version check request                                |
| `Apgar`      | `APGAR`    | WOL: alternate password/auth token                        |
| `Setopt`     | `SETOPT`   | WOL: set a client option                                  |
| `Serial`     | `SERIAL`   | WOL: CD-key serial number                                 |
| `Gameopt`    | `GAMEOPT`  | WOL: game options for hosted game                         |
| `Startg`     | `STARTG`   | WOL: start a game with player list                        |
| `Joingame`   | `JOINGAME` | WOL: join an existing game (host IP + port)               |
| `Finduser`   | `FINDUSER` | WOL: find a user by nickname                              |
| `Page`       | `PAGE`     | WOL: send a page/notification to a user                   |
| `Addbuddy`   | `ADDBUDDY` | WOL: add to buddy list                                    |
| `Delbuddy`   | `DELBUDDY` | WOL: remove from buddy list                               |
| `Getbuddy`   | `GETBUDDY` | WOL: request buddy list                                   |
| `WolCommand` | (catch-all)| Known WOL commands accepted but not fully structured:     |
|              |            | ADVERTR, ADVERTC, CHANCHK, HOST, INVMSG, INVDEL, USERIP, |
|              |            | SQUADINFO, CLANBYNAME, SETCODEPAGE, GETCODEPAGE,          |
|              |            | SETLOCALE, GETLOCALE, GETINSIDER, LISTSEARCH, RUNGSEARCH, |
|              |            | HIGHSCORE, NAMES, TOPIC, TIME, KICK, MODE, FINDUSEREX     |

### Server → Client (2 types in `ServerMessage` variant)

| Struct         | Description                                              |
|----------------|----------------------------------------------------------|
| `NumericReply` | IRC-style `:server NNN target :text\r\n` reply           |
| `RawLine`      | Raw text line (PONG, JOIN echo, ERROR, etc.)             |

## Codec Behaviour

### `decode_client(line)`
- Accepts a single CRLF-stripped line (framing is the FSM's responsibility)
- Strips optional IRC prefix (`:server.name CMD ...`)
- Upper-cases the command for case-insensitive matching
- Returns `DecodeError::Truncated` for empty lines
- Returns `DecodeError::UnknownOpcode` for unrecognised commands
- Returns `DecodeError::MalformedString` for missing required parameters

### `encode_server(msg)`
- Overloaded for `NumericReply`, `RawLine`, and `ServerMessage` variant
- Always appends `\r\n` to produce wire-ready output
- `NumericReply` formats as `:server_name NNN target :text\r\n`

## Test Coverage (codec_test.cpp)

- 24 decode success cases (one per command type)
- 3 WolCommand catch-all cases (CHANCHK, MODE, USERIP)
- IRC prefix stripping test
- Case-insensitive command matching (2 cases)
- 9 error cases:
  - Empty line → `DecodeError::Truncated`
  - Unknown opcode → `DecodeError::UnknownOpcode`
  - NICK/JOIN/STARTG/FINDUSER/ADDBUDDY/DELBUDDY with missing params → `DecodeError::MalformedString`
- 5 encode tests (NumericReply, RawLine, ServerMessage variant, 3-digit code formatting)

## Notes

- WOL is an IRC-like **text** protocol (not binary), so there are no binary opcodes.
  The "opcode" is the ASCII command name (NICK, USER, PASS, etc.).
- The codec operates at the **line** level. The `WolFsm` handles byte accumulation
  and CRLF splitting; the codec handles parsing of individual lines.
- `WolCommand` is a typed catch-all for the ~23 WOL-specific commands that the FSM
  silently accepts but does not yet fully implement. This allows the codec to return
  a typed value (rather than an error) for all known commands.
- The `protocol_common` dependency provides `DecodeError` from R274.
- No dependency on `protocol/common/reader.hpp` — WOL is text-based, not binary,
  so the binary `Reader` is not needed.
