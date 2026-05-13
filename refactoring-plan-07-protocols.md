# 07 · Protocol Layer

The legacy code mixes wire decoding, validation, business logic,
replies, persistence, and logging in `handle_*.cpp` files of 1–4 kLOC
apiece. The new design isolates a **pure codec + state machine** per
protocol, depending only on `core` (no domain, no I/O).

## 1. Layout

```
src/protocol/
├── common/
│   ├── packet.hpp             # immutable byte view, header parsed
│   ├── reader.hpp / .cpp      # bounded LE/BE reader; never throws OOB
│   ├── writer.hpp / .cpp      # builds a packet with size headers
│   ├── frame.hpp              # decoded message variant
│   └── error.hpp
│
├── bnet/                      # Battle.net binary protocol
│   ├── messages.hpp           # 0x0E SID_LOGON_REQ, etc. as structs
│   ├── codec.{hpp,cpp}        # bytes <-> Message variant
│   ├── fsm.{hpp,cpp}          # per-session state machine
│   ├── version_check.{hpp,cpp}# version/CRC validation rules
│   └── server_signature.{hpp,cpp}
│
├── irc/                       # RFC1459 + Battle.net extensions
│   ├── tokeniser.{hpp,cpp}
│   ├── messages.hpp
│   └── codec.{hpp,cpp}
│
├── wol/                       # Westwood Online (IRC dialect + extensions)
│   ├── codec.{hpp,cpp}
│   ├── gameres_codec.{hpp,cpp}# binary gameres protocol
│   └── apireg_codec.{hpp,cpp}
│
├── d2cs/                      # bnetd↔d2cs and d2cs↔client
│   └── codec.{hpp,cpp}
│
├── d2gs/                      # d2cs↔d2gs and d2cs↔d2dbs
│   └── codec.{hpp,cpp}
│
├── file/                      # bnftp file-transfer wire format
│   └── codec.{hpp,cpp}
│
├── udp/                       # UDP keepalive / port-test
│   └── codec.{hpp,cpp}
│
└── telnet/                    # admin telnet
    └── codec.{hpp,cpp}
```

## 2. Decoding model

```cpp
struct PacketHeader { std::uint8_t marker; std::uint8_t code; std::uint16_t size; };

struct Reader {
    Reader(std::span<const std::byte>);
    template <class T> tl::expected<T,DecodeError> readLE();
    tl::expected<std::string_view,DecodeError>    readNTString();
    tl::expected<std::span<const std::byte>,DecodeError> readBytes(std::size_t);
    bool remaining() const noexcept;
};
```

* **Bounded**: every read checks size; on overflow returns
  `DecodeError::Truncated`. Replaces the manual `length<size_of_x ?
  abort:` checks scattered today.
* **`std::string_view` for strings**: zero-copy; lifetime tied to the
  packet buffer.
* **Variant of messages**: `using BnetMessage = std::variant<
  Logon, Ping, JoinGame, Whisper, …>`; codec returns the variant.

## 3. Server-side FSM example (BNet)

```cpp
enum class BnetState { Init, AuthSent, LoggedIn, InChannel, InGame, Closing };

class BnetFsm {
public:
    explicit BnetFsm(SessionContext&);
    void on(InitMessage);
    void on(LogonReq);
    void on(JoinChannel);
    void on(GameCreate);
    void on(ChatCommand);
    void on(Disconnect);
    BnetState state() const noexcept;
private:
    SessionContext& ctx_;     // exposes use-cases + message router
    BnetState st_ = BnetState::Init;
};
```

* `on(Message)` overloads make the FSM a one-line switch dispatched by
  `std::visit`.
* Each `on(...)` invokes one or more **use-cases** through `ctx_`. No
  database, no socket access inside.
* `ctx_.send(BnetReply{...})` writes via `IMessageRouter`; the
  outgoing path encodes back through `codec.cpp`.

## 4. Why this matters

Concrete pain points from `handle_bnet.cpp` (~3 kLOC) addressed:

* The current code reads packet fields inline (`bn_int_get(...)`) and
  cooks responses with `packet_create`/`packet_set_xxx_field` in the
  same function — making it impossible to unit-test a message in
  isolation. With the codec, **encoding round-trips become trivially
  testable**.
* Adding a new packet today requires touching `bnet_protocol.h`,
  `handle_bnet.cpp`, sometimes `connection.cpp` (for new state),
  `command_groups.cpp`. After: add a struct to `messages.hpp`, a
  variant arm, an `on()` overload.
* Fuzzing today corrupts session state (the parser mutates
  `t_connection`). After: codec is a pure function over bytes; a
  libFuzzer harness covers it directly.

## 5. Cross-protocol commonalities

* All codecs share `protocol/common/reader.hpp` / `writer.hpp` for
  safe binary I/O.
* Strings & encodings: `std::u8string`/`std::u16string_view` for D2
  character names (UTF-16-LE on the wire), `std::string_view` for
  ASCII. Conversion via ICU optional (or `simdutf` for fast paths).
* Versioning: each codec module exposes a small `Capabilities` record
  consumed by the FSM (e.g. "WC3 1.27+ uses GenericAuthChallenge").

## 6. Streaming reader

Some PDUs span TCP packet boundaries.  The reader uses a per-session
`std::vector<std::byte> rxbuf_` and `try_parse()` returns either a
fully-formed `Frame` and bytes-consumed count, or `NeedMore`. The
`TcpSession` fiber loop simply reads more bytes and retries.

## 7. Replay & golden-file tests

* `tests/protocol_replay/` ships captured `.bin` traces (sanitized) per
  client (StarCraft 1.16, WC3 1.27, WCG bots, IRC).
* Tests run the FSM against the trace with mock use-cases and assert
  that all outgoing packets match a golden file.
* This becomes the regression net for the migration.

## 8. Fuzz harnesses

For each protocol family, a libFuzzer driver:

```cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    pvpgn::protocol::bnet::Codec codec;
    auto msg = codec.decode({reinterpret_cast<const std::byte*>(data), size});
    if (msg) {
        std::array<std::byte, 8192> out;
        (void)codec.encode(*msg, out);
    }
    return 0;
}
```

Run as part of CI and as a long-running OSS-Fuzz integration.

## 9. Migration of legacy `handle_*` files

A spreadsheet (`docs/protocol_migration.md`) tracks every legacy
handler and its mapping to one or more new `on(Message)` calls. Each
migrated handler triggers deletion of its legacy file from the build
list, **never both at once** — preventing two implementations
diverging.
