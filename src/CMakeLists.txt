# Root for the v3 sub-tree.
# Built only when PVPGN_BUILD_V3=ON (legacy default keeps building untouched).

include(${CMAKE_SOURCE_DIR}/cmake/v3.cmake)
include(FetchContent)

# ---- LTO (Plan 09 /WX — Milestone 7) ------------------------------------
# Enable link-time optimisation for Release builds when the toolchain supports
# it.  CMAKE_INTERPROCEDURAL_OPTIMIZATION is set to ON by the v3-release preset;
# this block validates that the toolchain actually supports IPO before letting
# CMake propagate the property to every target.  If IPO is not supported the
# preset variable is silently cleared so the build still succeeds.
if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _pvpgn_ipo_supported OUTPUT _pvpgn_ipo_output)
    if(NOT _pvpgn_ipo_supported)
        message(STATUS
            "PvPGN v3: IPO/LTO requested but not supported by this toolchain "
            "(${_pvpgn_ipo_output}); disabling.")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    else()
        message(STATUS "PvPGN v3: IPO/LTO enabled for Release build.")
    endif()
endif()

# ---- third-party (FetchContent, header/source only) ---------------------

option(PVPGN_V3_WITH_SPDLOG  "Fetch & build spdlog for the v3 tree"      ON)
option(PVPGN_V3_WITH_TOMLPP  "Fetch & build toml++ for the v3 tree"      ON)
option(PVPGN_V3_WITH_BOOST   "Use system Boost (Asio + optional Fiber)"  ON)
option(PVPGN_V3_WITH_FIBER   "Enable Boost.Fiber-backed awaiters"        OFF)
option(PVPGN_V3_WITH_ANKERL  "Fetch & build ankerl/unordered_dense"      ON)
option(PVPGN_V3_WITH_PUGIXML "Fetch & build pugixml for the v3 tree"     ON)

if(PVPGN_V3_WITH_ANKERL)
    FetchContent_Declare(
        ankerl_unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG        v4.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(ankerl_unordered_dense)
endif()

if(PVPGN_V3_WITH_SPDLOG)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL      OFF CACHE BOOL "" FORCE)
    set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.14.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(spdlog)
    foreach(_t spdlog spdlog_header_only)
        if(TARGET ${_t})
            set_target_properties(${_t} PROPERTIES SYSTEM TRUE)
        endif()
    endforeach()
endif()

if(PVPGN_V3_WITH_TOMLPP)
    FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG        v3.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(tomlplusplus)
    if(TARGET tomlplusplus_tomlplusplus)
        set_target_properties(tomlplusplus_tomlplusplus PROPERTIES SYSTEM TRUE)
    endif()
endif()

if(PVPGN_V3_WITH_PUGIXML)
    set(PUGIXML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG        v1.14
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(pugixml)
    if(TARGET pugixml-static)
        set_target_properties(pugixml-static PROPERTIES SYSTEM TRUE)
        # pugixml's CMake exports the static target as pugixml::static
        # but also as pugixml-static; ensure the canonical alias exists.
        if(NOT TARGET pugixml::static)
            add_library(pugixml::static ALIAS pugixml-static)
        endif()
    endif()
endif()

if(PVPGN_V3_WITH_BOOST)
    set(_pvpgn_boost_components system)
    if(PVPGN_V3_WITH_FIBER)
        list(APPEND _pvpgn_boost_components fiber context)
    endif()
    find_package(Boost 1.75 REQUIRED COMPONENTS ${_pvpgn_boost_components})
endif()

# Find OpenSSL for runtime peer_link support. Optional -- peer_link is the
# only consumer, and it is fine to build without it when developers do not
# have OpenSSL installed (the v3 single-node binary still works).
find_package(OpenSSL QUIET)
if(NOT OpenSSL_FOUND)
    message(STATUS "OpenSSL not found -- runtime/peer_link.cpp will be excluded")
endif()

# ---- core ---------------------------------------------------------------

pvpgn_v3_add_library(core
    STATIC
    SOURCES
        core/src/logging.cpp
        core/src/error.cpp
        core/src/trace.cpp
        core/src/metrics.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

# ---- runtime (service host, peer-to-peer communication) -------------------

pvpgn_v3_add_library(runtime
    STATIC
    SOURCES
        runtime/src/service_host.cpp
        runtime/src/peer_link.cpp
        runtime/src/cli.cpp
        runtime/src/daemonize.cpp
        runtime/src/crash_handler.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include
    PUBLIC_DEPS
        core
)

# Suppress false positive null-dereference warnings from boost/asio
if(NOT MSVC)
    target_compile_options(runtime PRIVATE -Wno-null-dereference)
endif()

# Platform-specific runtime sources
if(WIN32)
    target_sources(runtime PRIVATE runtime/src/win_service.cpp)
endif()

# OpenSSL is optional. When found, link it; when missing, drop peer_link.cpp
# from the runtime library so the rest of the v3 tree still builds.
if(OpenSSL_FOUND)
    target_link_libraries(runtime PRIVATE OpenSSL::SSL OpenSSL::Crypto)
else()
    get_target_property(_runtime_src runtime SOURCES)
    list(FILTER _runtime_src EXCLUDE REGEX "peer_link\\.cpp$")
    set_target_properties(runtime PROPERTIES SOURCES "${_runtime_src}")
endif()

# ---- infra/log ----------------------------------------------------------

# ---- infra/compat -------------------------------------------------------
#
# Thin C++20 wrappers around platform-specific helpers. Replaces the
# headers under src/compat/. Header-only consumers can include any of
# the headers below without linking; consumers of process info must link
# against `infra_compat`.
#
# Header inventory (all under infra/compat/include/infra/compat/):
#
#   platform.hpp      — compile-time OS detection (kIsWindows, kIsLinux, …)
#   process.hpp       — current_process_id(), host_name(), system_info()
#                       replaces: pgetpid.h, gethostname.h
#   netinet_in.hpp    — kInAddrLoopback, kInAddrAny constants
#                       replaces: netinet_in.h
#   read.hpp          — ensures read() / _read() is declared
#                       replaces: read.h
#   stdfileno.hpp     — kStdinFd, kStdoutFd, kStderrFd constants
#                       replaces: stdfileno.h
#   termios.hpp       — Termios type + tcgetattr/tcsetattr (POSIX or stub)
#                       replaces: termios.h
#   socket.hpp        — pulls in platform socket headers
#                       replaces: socket.h
#   recv.hpp          — ensures recv() is declared (delegates to socket.hpp)
#                       replaces: recv.h
#   send.hpp          — ensures send() is declared (delegates to socket.hpp)
#                       replaces: send.h
#   mkdir.hpp         — make_directory() via std::filesystem
#                       replaces: mkdir.h
#   rename.hpp        — rename_file() via std::filesystem
#                       replaces: rename.h
#   runtime_libs.hpp  — DynamicLibrary RAII + open/get/close_library()
#                       replaces: runtime_libs.h
#   strerror.hpp      — error_string(), socket_error_string()
#                       replaces: strerror.h + strerror.cpp
#   directory.hpp     — DirectoryIterator / DirectoryEntry RAII types,
#                       open_directory() / read_directory() / close_directory(),
#                       list_files() (replaces dir_getfiles())
#                       replaces: pdir.h + pdir.cpp  [✅ R113]
#   getopt.hpp        — CommandLineParser class + parse_args() free function
#                       ArgSpec (short/long/description/has_arg), ParseResult
#                       replaces: pgetopt.h + pgetopt.cpp  [✅ R114]
#
# Still pending (Tier C — Round 115+):
#   psock migration   — goes to infra/net/ (Boost.Asio), not infra/compat/

pvpgn_v3_add_library(infra_compat
    STATIC
    SOURCES
        infra/compat/src/process.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/compat/include
    PUBLIC_DEPS
        core
)

# ---- infra/xml ----------------------------------------------------------
#
# Thin C++20 wrapper over pugixml.  Header-only from the consumer's
# perspective; links pugixml::static transitively.
#
# Provides:
#   XmlDocument  — owns a parsed XML document (move-only)
#   XmlNode      — lightweight wrapper around pugi::xml_node (value type)
#   load_file()  — parse from std::filesystem::path → std::optional<XmlDocument>
#   load_string()— parse from std::string_view     → std::optional<XmlDocument>
#
# All in namespace pvpgn::v3::infra::xml.

if(PVPGN_V3_WITH_PUGIXML)
    add_subdirectory(infra/xml)
endif()

# ---- infra/crypto -------------------------------------------------------
#
# Clean C++20 reimplementation of Battle.net cryptographic primitives
# (Blizzard's broken-SHA1 hash, true SHA-1 helpers, ...). Replaces the
# legacy src/common/{bnethash,bnethashconv,bnetsrp3,bigint,wolhash,
# peerchat}.cpp. Migration is incremental -- legacy headers stay until
# every consumer has switched.

pvpgn_v3_add_library(infra_crypto
    STATIC
    SOURCES
        infra/crypto/src/bnet_hash.cpp
        infra/crypto/src/bnet_hash_conv.cpp
        infra/crypto/src/big_uint.cpp
        infra/crypto/src/wol_hash.cpp
        infra/crypto/src/peerchat.cpp
        infra/crypto/src/bnet_srp3.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/crypto/include
    PUBLIC_DEPS
        core
        Boost::headers
)

# R281: NLS/SRP-6a crypto (WAR3/W3XP 1024-bit prime, OpenSSL BN_*).
# Only built when OpenSSL is available.
if(OpenSSL_FOUND OR TARGET OpenSSL::Crypto)
    add_subdirectory(infra/crypto)
endif()

# Always-available structured logger backends (no third-party deps).
pvpgn_v3_add_library(infra_log_simple
    STATIC
    SOURCES
        infra/log/src/json_line_logger.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/log/include
    PUBLIC_DEPS
        core
)

if(PVPGN_V3_WITH_SPDLOG)
    pvpgn_v3_add_library(infra_log
        STATIC
        SOURCES
            infra/log/src/spdlog_logger.cpp
            infra/log/src/logger_factory.cpp
            infra/log/src/rotating_file_sink.cpp
            infra/log/src/console_sink.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/infra/log/include
        PUBLIC_DEPS
            core
            $<$<TARGET_EXISTS:infra_config>:infra_config>
        DEPS
            spdlog::spdlog
    )
endif()

# ---- infra/config -------------------------------------------------------

if(PVPGN_V3_WITH_TOMLPP)
    pvpgn_v3_add_library(infra_config
        STATIC
        SOURCES
            infra/config/src/server_config.cpp
            infra/config/src/config_watcher.cpp
            infra/config/src/d2cs_server_config.cpp
            infra/config/src/d2dbs_server_config.cpp
            infra/config/src/channel_config_loader.cpp
            infra/config/src/toml_schema_validator.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/infra/config/include
        PUBLIC_DEPS
            core
            application_ports
            tomlplusplus::tomlplusplus
        DEPS
    )
endif()

# ---- infra/compression --------------------------------------------------
#
# Thin zlib adapter for the legacy FINDANONGAME INFOREPLY framing.
# Kept out of `protocol_bnet` so the protocol layer stays free of any
# external compression dependency.

option(PVPGN_V3_FETCH_ZLIB
       "Fetch & build zlib for the v3 tree if not already available"
       ON)

if(NOT TARGET ZLIB::ZLIB)
    find_package(ZLIB QUIET)
endif()

if(NOT TARGET ZLIB::ZLIB AND PVPGN_V3_FETCH_ZLIB)
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL    ON  CACHE BOOL "" FORCE)
    FetchContent_Declare(
        zlib_vendored
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG        v1.3.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(zlib_vendored)
    if(TARGET zlibstatic AND NOT TARGET ZLIB::ZLIB)
        # zlibstatic does not auto-propagate its source-dir include path
        # to consumers; do it explicitly.
        target_include_directories(zlibstatic
            PUBLIC
                $<BUILD_INTERFACE:${zlib_vendored_SOURCE_DIR}>
                $<BUILD_INTERFACE:${zlib_vendored_BINARY_DIR}>)
        set_target_properties(zlibstatic PROPERTIES SYSTEM TRUE)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
endif()

if(NOT TARGET ZLIB::ZLIB)
    find_package(ZLIB REQUIRED)
endif()

pvpgn_v3_add_library(infra_compression
    STATIC
    SOURCES
        infra/compression/src/zlib_anongame.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/compression/include
    PUBLIC_DEPS
        core
        application_ports
    DEPS
        ZLIB::ZLIB
)

# ---- infra/scripting/plugin -------------------------------------------------

add_subdirectory(infra/scripting)

# ---- infra/lua --------------------------------------------------------------
#
# Clean C++20 RAII wrapper around a Lua VM (`lua_State*`).
# Builds as a stub (all methods are no-ops) when Lua is not found.
# Consumers link against `infra_lua`; the PVPGN_HAVE_LUA compile definition
# is propagated publicly so they can use the same guard.

add_subdirectory(infra/lua)

# ---- protocol/common ----------------------------------------------------

pvpgn_v3_add_library(protocol_common
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/common/include
    PUBLIC_DEPS
        core
    INTERFACE_SOURCES
        protocol/common/include/protocol/common/packet.hpp
        protocol/common/include/protocol/common/reader.hpp
        protocol/common/include/protocol/common/writer.hpp
        protocol/common/include/protocol/common/replay.hpp
        protocol/common/include/protocol/common/decode_error.hpp
        protocol/common/include/protocol/common/frame_view.hpp
        protocol/common/include/protocol/common/next_frame.hpp
)

# ---- protocol/bnet ------------------------------------------------------

pvpgn_v3_add_library(protocol_bnet
    STATIC
    SOURCES
        protocol/bnet/src/codec.cpp
        # R-split: codec.cpp split into codec/ modules (plan 15 §8)
        protocol/bnet/src/codec/codec_auth.cpp
        protocol/bnet/src/codec/codec_chat.cpp
        protocol/bnet/src/codec/codec_game.cpp
        protocol/bnet/src/codec/codec_friends.cpp
        protocol/bnet/src/codec/codec_clan.cpp
        protocol/bnet/src/codec/codec_ladder.cpp
        protocol/bnet/src/codec/codec_realm.cpp
        protocol/bnet/src/codec/codec_legacy_ols.cpp
        protocol/bnet/src/codec/codec_w3.cpp
        protocol/bnet/src/fsm.cpp
        protocol/bnet/src/anongame.cpp
        protocol/bnet/src/anongame_tags.cpp
        protocol/bnet/src/event_dispatcher.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/bnet/include
    PRIVATE_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/bnet/src
    PUBLIC_DEPS
        core
        protocol_common
        application_ports
        application_auth
        application_chat
        application_game
        application_moderation
)

# MSVC: codec.cpp and split modules exceed the default object-file section count.
if(MSVC)
    target_compile_options(protocol_bnet PRIVATE /bigobj)
endif()

# ---- protocol/irc -------------------------------------------------------

pvpgn_v3_add_library(protocol_irc
    STATIC
    SOURCES
        protocol/irc/src/codec.cpp
        protocol/irc/src/fsm.cpp
        protocol/irc/src/bridge_fsm.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/irc/include
    PUBLIC_DEPS
        core
        protocol_common
        application_auth
        application_chat
        domain_chat
        domain_shared
)

# ---- protocol/udp -------------------------------------------------------

pvpgn_v3_add_library(protocol_udp
    STATIC
    SOURCES
        protocol/udp/src/codec.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/udp/include
    PUBLIC_DEPS
        core
        protocol_common
)

# ---- protocol/telnet ----------------------------------------------------

pvpgn_v3_add_library(protocol_telnet
    STATIC
    SOURCES
        protocol/telnet/src/codec.cpp
        protocol/telnet/src/admin_fsm.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/telnet/include
    PUBLIC_DEPS
        core
        protocol_common
        protocol_bnet
        application_ports
)

# ---- protocol/file ------------------------------------------------------

pvpgn_v3_add_library(protocol_file
    STATIC
    SOURCES
        protocol/file/src/codec.cpp
        protocol/file/src/bnftp_fsm.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/file/include
    PUBLIC_DEPS
        core
        protocol_common
)

# ---- protocol/d2cs ------------------------------------------------------

pvpgn_v3_add_library(protocol_d2cs
    STATIC
    SOURCES
        protocol/d2cs/src/codec.cpp
        protocol/d2cs/src/fsm.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/d2cs/include
    PUBLIC_DEPS
        core
        protocol_common
)

# ---- protocol/d2gs ------------------------------------------------------

pvpgn_v3_add_library(protocol_d2gs
    STATIC
    SOURCES
        protocol/d2gs/src/codec.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/d2gs/include
    PUBLIC_DEPS
        core
        protocol_common
)

# ---- protocol/d2dbs -----------------------------------------------------

pvpgn_v3_add_library(protocol_d2dbs
    STATIC
    SOURCES
        protocol/d2dbs/src/fsm.cpp
        protocol/d2dbs/src/codec.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/d2dbs/include
    PUBLIC_DEPS
        core
        protocol_common
)

# ---- protocol/wolgameres ------------------------------------------------

pvpgn_v3_add_library(protocol_wolgameres
    STATIC
    SOURCES
        protocol/wolgameres/src/codec.cpp
        protocol/wolgameres/src/wol_fsm.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wolgameres/include
    PUBLIC_DEPS
        core
        protocol_common
        application_ports
)

# ---- protocol/wol (WOL IRC-like chat protocol FSM) ----------------------
#
# Implements the Westwood Online (WOL) text-based chat protocol used by
# Command & Conquer, Red Alert, Tiberian Sun, and other Westwood/EA games.
# The protocol is IRC-like (line-oriented, \r\n terminated) with WOL-specific
# extensions (CVERS, VERCHK, APGAR, GAMEOPT, STARTG, JOINGAME, etc.).
#
# States: Connecting → Authenticating → Authenticated → InChannel / InGame
# Commands: NICK, USER, PASS, PING, QUIT, LIST, JOIN, PART, PRIVMSG

pvpgn_v3_add_library(protocol_wol
    STATIC
    SOURCES
        protocol/wol/src/wol_fsm.cpp
        protocol/wol/src/codec.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wol/include
    PUBLIC_DEPS
        core
        protocol_common
        application_auth
        application_chat
        domain_chat
        domain_shared
)

# ---- domain/shared (value objects, events, IDs) -------------------------

pvpgn_v3_add_library(domain_shared
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/shared/include
    PUBLIC_DEPS
        core
)

# ---- domain/identity (Account aggregate) --------------------------------

pvpgn_v3_add_library(domain_identity
    SOURCES
        domain/identity/src/attribute_map.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/identity/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/chat (Channel aggregate) ------------------------------------

pvpgn_v3_add_library(domain_chat
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/chat/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/social (FriendList, Clan) -----------------------------------

pvpgn_v3_add_library(domain_social
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/social/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/gameplay (Game aggregate + FSM) -----------------------------

pvpgn_v3_add_library(domain_gameplay
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/gameplay/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/ladder (LadderCalculator service + D2Ladder aggregate) ------

pvpgn_v3_add_library(domain_ladder
    STATIC
    SOURCES
        domain/ladder/src/d2_ladder.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/ladder/include
    PUBLIC_DEPS
        core
        domain_shared
        domain_realm
)

# ---- domain/moderation (IpBanList) --------------------------------------

pvpgn_v3_add_library(domain_moderation
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/moderation/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/matchmaking (AnonGameQueue, Tournament) ---------------------

pvpgn_v3_add_library(domain_matchmaking
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/matchmaking/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/realm (Realm + Character + CharacterList) -------------------

pvpgn_v3_add_library(domain_realm
    STATIC
    SOURCES
        domain/realm/src/character.cpp
        domain/realm/src/dupe_checker.cpp
        domain/realm/src/character_list.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/domain/realm/include
    PUBLIC_DEPS
        core
        domain_shared
)

# ---- domain/connection (ConnectionFsm — Phase 3 Step 2) -----------------

add_subdirectory(domain/connection)

# ---- domain/d2cs (D2CS domain layer — Phase 4 Step 2) -------------------

add_subdirectory(domain/d2cs)

# ---- domain/d2dbs (D2DBS domain layer — Phase 4 Step 6) -----------------

add_subdirectory(domain/d2dbs)

# ---- application/ports (interface-only) ---------------------------------

pvpgn_v3_add_library(application_ports
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/ports/include
    PUBLIC_DEPS
        core
        domain_shared
        domain_identity
        domain_chat
        domain_gameplay
        domain_moderation
        domain_social
        domain_realm
        domain_ladder
)

# ---- infra/persistence (adapter registry + shared SQL helpers) ----------
add_subdirectory(infra/persistence)

# ---- application/auth (LoginUser) ---------------------------------------

pvpgn_v3_add_library(application_auth
    STATIC
    SOURCES
        application/auth/src/login_user.cpp
        application/auth/src/login_user_nls.cpp
        application/auth/src/login_classifier.cpp
        application/auth/src/password_rotation_observer.cpp
        application/auth/src/change_password.cpp
        application/auth/src/create_account.cpp
        application/auth/src/logout_user.cpp
        application/auth/src/account_lock.cpp
        application/auth/src/permission_checker.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/auth/include
    PUBLIC_DEPS
        core
        application_ports
        domain_shared
        domain_identity
        domain_moderation
    PRIVATE_DEPS
        application_chat
)

# ---- application/connection (ConnectionFsm) -----------------------------
#
# Application-layer state machine driving a single BNet client connection.
# Composes auth and chat use-cases via abstract ports; uses the
# `domain::connection::IConnectionContext` port for I/O.
#
# R298: moved here from `domain/connection` so the FSM can depend on
# application use-cases without violating the hexagonal layering rule
# `domain MUST NOT depend on application`.
pvpgn_v3_add_library(application_connection
    STATIC
    SOURCES
        application/connection/src/connection_fsm.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/connection/include
    PUBLIC_DEPS
        core
        domain_shared
        domain_connection
        application_auth
        application_chat
)

# ---- infra/crypto/nls_crypto_adapter ------------------------------------
#
# Concrete `application::auth::INlsCryptoService` adapter backed by
# `infra::crypto::NlsServer`. Defined AFTER application_auth so we can
# link to it without forward-reference issues.
pvpgn_v3_add_library(infra_crypto_nls_adapter
    STATIC
    SOURCES
        infra/crypto/src/nls_crypto_adapter.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/crypto/include
    PUBLIC_DEPS
        core
        infra_crypto_nls
        application_auth
)


# ---- application/realm (CharacterLock, GameServerQueue) --------------------

pvpgn_v3_add_library(application_realm
    STATIC
    SOURCES
        application/realm/src/character_lock.cpp
        application/realm/src/gs_queue.cpp
        application/realm/src/realm_list.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/realm/include
    PUBLIC_DEPS
        core
        domain_realm
)
# ---- application/anongame_infoply ---------------------------------------
#
# Composes a full FINDANONGAME INFOREPLY set: typed payloads ->
# serialize -> zlib-compress -> AnonGameInfoReply envelopes.

pvpgn_v3_add_library(application_anongame_infoply
    STATIC
    SOURCES
        application/anongame_infoply/src/inforeply_builder.cpp
        application/anongame_infoply/src/type_composer.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/anongame_infoply/include
    PUBLIC_DEPS
        core
        application_ports
        protocol_bnet
)

# ---- application/icon_table ---------------------------------------------
#
# Pure builder for the FINDANONGAME GET_ICON (sub-option 0x09) reply
# table. Mirrors `_client_anongame_get_icon` logic but accepts all
# account state as inputs so it can be tested without the legacy
# globals. Depends on infra/legacy_config for the `IconReqTable`
# threshold table type.

pvpgn_v3_add_library(application_icon_table
    STATIC
    SOURCES
        application/icon_table/src/icon_table.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/icon_table/include
    PUBLIC_DEPS
        core
)

# ---- application/tournament ---------------------------------------------
#
# Pure builder for the FINDANONGAME 0x07 (TOURNAMENT) reply. Mirrors
# the legacy `_client_anongame_tournament` state machine but takes
# every input as a value so the logic can be exercised in isolation.

pvpgn_v3_add_library(application_tournament
    STATIC
    SOURCES
        application/tournament/src/tournament_reply.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/tournament/include
    PUBLIC_DEPS
        core
        protocol_bnet
)

# ---- application/admin_commands -----------------------------------------
#
# R216 strangler-fig: pure routing decision for legacy bnetd chat
# commands that have been migrated to the v3 dispatch path. Owns name
# normalisation and permission-predicate hand-off; the actual I/O
# (sending text to t_connection, userlog) stays in the bridge.

pvpgn_v3_add_library(application_admin_commands
    STATIC
    SOURCES
        application/admin_commands/src/router.cpp
        application/admin_commands/src/help_corpus.cpp
        application/admin_commands/src/help_corpus_parser.cpp
        application/admin_commands/src/file_help_responder.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/admin_commands/include
    PUBLIC_DEPS
        core
)

# ---- application/profile ------------------------------------------------
#
# Pure builder for the FINDANONGAME 0x04 (PROFILE) reply. Mirrors the
# legacy `_client_anongame_profile` ladder/race/AT-team assembly but
# accepts a snapshot value as input so it can be tested without bnetd.

pvpgn_v3_add_library(application_profile
    STATIC
    SOURCES
        application/profile/src/profile_reply.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/profile/include
    PUBLIC_DEPS
        core
        protocol_bnet
)

# ---- application/chat ---------------------------------------------------
#
# Pure classifier for CLIENT_CHATCOMMAND payloads. Splits the raw chat
# input into Whisper / Command / ChannelMessage / Empty so the future
# CHAT bridge can route deterministically without depending on the
# legacy `t_connection`. Scaffold for the CHAT vertical (Batch 17a).

pvpgn_v3_add_library(application_chat
    STATIC
    SOURCES
        application/chat/src/chat_command.cpp
        application/chat/src/whisper_use_case.cpp
        application/chat/src/join_channel.cpp
        application/chat/src/post_message.cpp
        application/chat/src/leave_channel.cpp
        application/chat/src/list_channels.cpp
        application/chat/src/send_emote.cpp
        application/chat/src/kick_from_channel.cpp
        application/chat/src/ban_from_channel.cpp
        application/chat/src/set_channel_topic.cpp
        application/chat/src/command_registry.cpp
        application/chat/src/chat_event_compose.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/chat/include
    PUBLIC_DEPS
        core
        application_ports
        domain_chat
        protocol_bnet
)

# ---- application/game ---------------------------------------------------
#
# Game management use-cases (StartGame, JoinGame, LeaveGame).
# Orchestrates between repositories and the Game aggregate.

pvpgn_v3_add_library(application_game
    STATIC
    SOURCES
        application/game/src/start_game.cpp
        application/game/src/join_game.cpp
        application/game/src/leave_game.cpp
        application/game/src/report_game_result.cpp
        application/game/src/list_public_games.cpp
        application/game/src/create_private_game.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/game/include
    PUBLIC_DEPS
        core
        application_ports
        domain_gameplay
)

# ---- application/moderation -----------------------------------------------
#
# Moderation use-cases (CheckIpBan).
# Checks IP ban status via the IpBanRepository port.

pvpgn_v3_add_library(application_moderation
    STATIC
    SOURCES
        application/moderation/src/check_ip_ban.cpp
        application/moderation/src/ban_account.cpp
        application/moderation/src/unban_account.cpp
        application/moderation/src/ban_ip.cpp
        application/moderation/src/kick_connection.cpp
        application/moderation/src/silence_user.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/moderation/include
    PUBLIC_DEPS
        core
        application_ports
        domain_moderation
)

# ---- application/social --------------------------------------------------
#
# Social use-cases: friend list management, clan operations.

pvpgn_v3_add_library(application_social
    STATIC
    SOURCES
        application/social/src/add_friend.cpp
        application/social/src/remove_friend.cpp
        application/social/src/list_friends.cpp
        application/social/src/create_clan.cpp
        application/social/src/disband_clan.cpp
        application/social/src/invite_to_clan.cpp
        application/social/src/kick_from_clan.cpp
        application/social/src/promote_clan_member.cpp
        application/social/src/set_clan_motd.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/social/include
    PUBLIC_DEPS
        core
        application_ports
        domain_social
)

# ---- application/i18n ---------------------------------------------------

pvpgn_v3_add_library(application_i18n
    STATIC
    SOURCES
        application/i18n/src/string_table.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/i18n/include
    PUBLIC_DEPS
        core
)

# ---- application/init ---------------------------------------------------

pvpgn_v3_add_library(application_init
    STATIC
    SOURCES
        application/init/src/init_conn_dispatch.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/init/include
    PUBLIC_DEPS
        core
        protocol_bnet
)

# ---- application/bnet_packet_pump (R179.a scaffold) --------------------
#
# Pure-C++ lifecycle FSM + ConnClass enum for the future v3 bnet
# packet pump. Headers only at this stage; a placeholder .cpp
# keeps the static lib non-empty. Future rounds (R179.c+) add the
# driver code that consumes `protocol/bnet/init_codec.hpp` and
# replaces the relocated `handle_init_packet` body.
pvpgn_v3_add_library(application_bnet_packet_pump
    STATIC
    SOURCES
        application/bnet_packet_pump/src/lifecycle_placeholder.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/bnet_packet_pump/include
    PUBLIC_DEPS
        core
        protocol_bnet
        application_init
)

# ---- application/email_management (R169.d skeleton) --------------------
#
# Interface-only module for the `handle_changeemail` /
# `handle_setemail` / `handle_getpassword` families. R169.d ships
# headers + placeholder TU only; R170 lands the real dispatcher
# logic alongside the legacy strangler bridges.
pvpgn_v3_add_library(application_email_management
    STATIC
    SOURCES
        application/email_management/src/email_change.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/email_management/include
    PUBLIC_DEPS
        core
)

# ---- application/ads (R170.b skeleton) ---------------------------------
#
# Interface-only module for `_client_adreq` / `_client_adack` /
# `_client_adclick` / `_client_adclick2`. Skeleton only; impl lands
# once AdBannerList is hidden behind a repository interface.
pvpgn_v3_add_library(application_ads
    STATIC
    SOURCES
        application/ads/src/ad_pick.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/ads/include
    PUBLIC_DEPS
        core
)

# ---- application/anongame_lobby (R170.d skeleton) ----------------------
#
# Interface-only module for the FindAnonGame lobby queue +
# bracket-fill promotion. Impl lands once anongame state is moved
# behind a queue repository interface.
pvpgn_v3_add_library(application_anongame_lobby
    STATIC
    SOURCES
        application/anongame_lobby/src/lobby.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/application/anongame_lobby/include
    PUBLIC_DEPS
        core
)

# ---- infra/legacy_config (parse legacy bnetd .conf files) ---------------
#
# Bridge adapters that convert legacy bnetd config-file dialects into the
# typed v3 snapshots used by the application services.

pvpgn_v3_add_library(infra_legacy_config
    STATIC
    SOURCES
        infra/legacy_config/src/anongame_infos_loader.cpp
        infra/legacy_config/src/anongame_maplists_loader.cpp
        infra/legacy_config/src/icon_req_loader.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/legacy_config/include
    PUBLIC_DEPS
        core
        protocol_bnet
        application_anongame_infoply
        application_icon_table
)

# ---- infra/audit (audit log implementations) --------------------------------

pvpgn_v3_add_library(infra_audit
    STATIC
    SOURCES
        infra/audit/src/in_memory_audit_log.cpp
        infra/audit/src/file_audit_log.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/audit/include
    PUBLIC_DEPS
        core
        application_ports
)

# ---- infra/tracker (UDP track-server reporter) ------------------------------

pvpgn_v3_add_library(infra_tracker
    STATIC
    SOURCES
        infra/tracker/src/tracker_client.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/tracker/include
    PUBLIC_DEPS
        core
)

# ---- infra/process (external program launcher) ------------------------------

pvpgn_v3_add_library(infra_process
    STATIC
    SOURCES
        infra/process/src/external_program.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/process/include
    PUBLIC_DEPS
        core
)

# ---- infra/inmemory (test + dev adapters, header-only) ------------------

pvpgn_v3_add_library(infra_inmemory
    STATIC
    SOURCES
        infra/inmemory/src/unit_of_work_factory.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/inmemory/include
        ${ankerl_unordered_dense_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        application_ports
        domain_shared
        domain_identity
        domain_realm
        domain_ladder
)

# ---- infra/migrations, infra/sqlite, infra/file -------------------------
# Must come after infra_inmemory (InMemoryTeamRepository placeholder) and
# after infra/persistence (port headers). The sqlite/file CMakeLists.txt
# reference pvpgn_infra_inmemory; create an alias from infra_inmemory.
if(NOT TARGET pvpgn_infra_inmemory)
    add_library(pvpgn_infra_inmemory ALIAS infra_inmemory)
endif()
if(NOT TARGET pvpgn_core)
    add_library(pvpgn_core ALIAS core)
endif()
if(NOT TARGET pvpgn_application)
    add_library(pvpgn_application ALIAS application_ports)
endif()

if(NOT TARGET pvpgn_infra_migrations)
    add_subdirectory(infra/migrations)
endif()
if(NOT TARGET pvpgn_infra_sqlite)
    add_subdirectory(infra/sqlite)
endif()
if(NOT TARGET pvpgn_infra_file)
    add_subdirectory(infra/file)
endif()
if(NOT TARGET pvpgn_infra_shadow)
    add_subdirectory(infra/shadow)
endif()
if(NOT TARGET pvpgn_infra_mysql)
    add_subdirectory(infra/mysql)
endif()
if(NOT TARGET pvpgn_infra_postgresql)
    add_subdirectory(infra/postgres)
endif()

# Wire backend_registration.cpp link dependencies now that the targets exist.
# infra_persistence includes backend_registration.cpp which #includes headers
# Wire backend_registration.cpp link dependencies now that the targets exist.
# Disabled: `backend_registration.cpp` is no longer built (see
# infra/persistence/CMakeLists.txt). The pvpgn_infra_sqlite / pvpgn_infra_file
# targets are still defined so they can be opted into directly if needed.
# if(TARGET pvpgn_infra_sqlite)
#     target_link_libraries(infra_persistence PRIVATE pvpgn_infra_sqlite)
# endif()
# if(TARGET pvpgn_infra_file)
#     target_link_libraries(infra_persistence PRIVATE pvpgn_infra_file)
# endif()

# ---- infra/storage/repository (Phase 2 in-memory repositories) -----------

add_subdirectory(infra/storage/repository)

# ---- infra/routing (message routing layer) -------------------------------

pvpgn_v3_add_library(infra_routing
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/routing/include
    PUBLIC_DEPS
        core
        application_ports
)

# ---- infra/session (protocol-specific session factories) -----------------

pvpgn_v3_add_library(infra_session
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/session/include
    PUBLIC_DEPS
        core
        application_ports
        application_auth
        infra_net
        infra_routing
        protocol_bnet
        protocol_irc
        protocol_file
        protocol_telnet
        protocol_wolgameres
        domain_shared
        domain_connection
)

# ---- infra/legacy_crypto (bnet_hash adapter) ----------------------------
#
# Adapts the legacy `pvpgn::bnet_hash` (broken-SHA-1) implementation
# to the v3 `IPasswordHasher` port. Linked against the legacy
# `common` library so we never reimplement the algorithm.

if(PVPGN_BUILD_LEGACY)
    pvpgn_v3_add_library(infra_legacy_crypto
        STATIC
        SOURCES
            infra/legacy_crypto/src/bnet_session_hasher.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/infra/legacy_crypto/include
        PUBLIC_DEPS
            core
            application_ports
            domain_shared
        DEPS
            common
    )
    # Legacy `common/setup_before.h` is not warning-clean under /WX.
    target_compile_options(infra_legacy_crypto PRIVATE -w)
    target_include_directories(infra_legacy_crypto PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR})
endif()

# ---- integration/legacy_bnetd (strangler-fig adapter) -------------------

pvpgn_v3_add_library(integration_legacy_bnetd
    STATIC
    SOURCES
        integration/legacy_bnetd/src/legacy_protocol_handler.cpp
        integration/legacy_bnetd/src/legacy_bnet_frame_router.cpp
        integration/legacy_bnetd/src/bnet_strangler_handler.cpp
        integration/legacy_bnetd/src/anongame_bootstrap.cpp
        integration/legacy_bnetd/src/bridge_logger.cpp
        integration/legacy_bnetd/src/legacy_chat_reply_sink.cpp
        integration/legacy_bnetd/src/login_user_bridge.cpp
        integration/legacy_bnetd/src/change_password_bridge.cpp
        integration/legacy_bnetd/src/send_packet_bridge.cpp
        integration/legacy_bnetd/src/init_conn_bridge.cpp
        integration/legacy_bnetd/src/send_authreq1_server_bridge.cpp
        integration/legacy_bnetd/src/send_authreply1_bridge.cpp
        integration/legacy_bnetd/src/send_authreply109_bridge.cpp
        integration/legacy_bnetd/src/send_authinfo_reply_bridge.cpp
        integration/legacy_bnetd/src/send_handshake_bridge.cpp
        integration/legacy_bnetd/src/send_echoreq_bridge.cpp
        integration/legacy_bnetd/src/send_messagebox_bridge.cpp
        integration/legacy_bnetd/src/send_friendadd_ack_bridge.cpp
        integration/legacy_bnetd/src/send_frienddel_ack_bridge.cpp
        integration/legacy_bnetd/src/send_friendmove_ack_bridge.cpp
        integration/legacy_bnetd/src/send_readmemory_bridge.cpp
        integration/legacy_bnetd/src/send_requiredwork_bridge.cpp
        integration/legacy_bnetd/src/send_clan_invitereply_bridge.cpp
        integration/legacy_bnetd/src/send_clan_membernewchief_reply_bridge.cpp
        integration/legacy_bnetd/src/send_clanmember_remove_reply_bridge.cpp
        integration/legacy_bnetd/src/send_clanmember_rankupdate_reply_bridge.cpp
        integration/legacy_bnetd/src/send_changepassack_bridge.cpp
        integration/legacy_bnetd/src/send_loginreply_bridge.cpp
        integration/legacy_bnetd/src/send_loginreply_w3_bridge.cpp
        integration/legacy_bnetd/src/send_logonproof_reply_bridge.cpp
        integration/legacy_bnetd/src/send_createacct_reply_bridge.cpp
        integration/legacy_bnetd/src/send_iconreply_bridge.cpp
        integration/legacy_bnetd/src/send_channellist_bridge.cpp
        integration/legacy_bnetd/src/channel_state_bridge.cpp
        integration/legacy_bnetd/src/game_report_bridge.cpp
        integration/legacy_bnetd/src/startgame_bridge.cpp
        integration/legacy_bnetd/src/gamelist_join_bridge.cpp
        integration/legacy_bnetd/src/anongame_dispatch_bridge.cpp
        integration/legacy_bnetd/src/anongame_entry_bridge.cpp
        integration/legacy_bnetd/src/anongame_infos_bridge.cpp
        integration/legacy_bnetd/src/anongame_infos_get_bridge.cpp
        integration/legacy_bnetd/src/runprog_bridge.cpp        # R230(1)
        integration/legacy_bnetd/src/news_bridge.cpp           # R230(2)
        integration/legacy_bnetd/src/userlog_bridge.cpp        # R230(3)
        integration/legacy_bnetd/src/versioncheck_bridge.cpp   # R230(4)
        integration/legacy_bnetd/src/clan_dispatch_bridge.cpp
        integration/legacy_bnetd/src/clan_send_bridge.cpp
        integration/legacy_bnetd/src/friends_dispatch_bridge.cpp
        integration/legacy_bnetd/src/auth_dispatch_bridge.cpp
        integration/legacy_bnetd/src/keepalive_dispatch_bridge.cpp
        integration/legacy_bnetd/src/realm_dispatch_bridge.cpp
        integration/legacy_bnetd/src/account_dispatch_bridge.cpp
        integration/legacy_bnetd/src/profile_dispatch_bridge.cpp
        integration/legacy_bnetd/src/ladder_dispatch_bridge.cpp
        integration/legacy_bnetd/src/d2_character_dispatch_bridge.cpp
        integration/legacy_bnetd/src/telemetry_dispatch_bridge.cpp
        integration/legacy_bnetd/src/ad_dispatch_bridge.cpp
        integration/legacy_bnetd/src/ads_bridge.cpp
        integration/legacy_bnetd/src/realm_list_bridge.cpp
        integration/legacy_bnetd/src/anongame_lobby_bridge.cpp
        integration/legacy_bnetd/src/progident_dispatch_bridge.cpp
        integration/legacy_bnetd/src/gameport_dispatch_bridge.cpp
        integration/legacy_bnetd/src/cdkey_dispatch_bridge.cpp
        integration/legacy_bnetd/src/passemail_dispatch_bridge.cpp
        integration/legacy_bnetd/src/handshake_dispatch_bridge.cpp
        integration/legacy_bnetd/src/stub_dispatch_bridge.cpp
        integration/legacy_bnetd/src/file_dispatch_bridge.cpp
        integration/legacy_bnetd/src/send_cdkey_reply_bridge.cpp
        integration/legacy_bnetd/src/send_fileinforeply_bridge.cpp
        integration/legacy_bnetd/src/send_passchange_bridge.cpp
        integration/legacy_bnetd/src/send_statsreply_bridge.cpp
        integration/legacy_bnetd/src/send_friendslist_bridge.cpp
        integration/legacy_bnetd/src/send_friendinfo_bridge.cpp
        integration/legacy_bnetd/src/send_atfriendscreen_bridge.cpp
        integration/legacy_bnetd/src/send_atinvitefriend_bridge.cpp
        integration/legacy_bnetd/src/send_atacceptdecline_bridge.cpp
        integration/legacy_bnetd/src/send_motdw3_bridge.cpp
        integration/legacy_bnetd/src/send_realmlist_bridge.cpp
        integration/legacy_bnetd/src/send_realmlistlegacy_bridge.cpp
        integration/legacy_bnetd/src/send_claninfo_bridge.cpp
        integration/legacy_bnetd/src/send_profilereply_bridge.cpp
        integration/legacy_bnetd/src/send_realmjoin_bridge.cpp
        integration/legacy_bnetd/src/send_chatevent_compose_bridge.cpp
        integration/legacy_bnetd/src/send_charlistreply_bridge.cpp
        integration/legacy_bnetd/src/send_adreply_bridge.cpp
        integration/legacy_bnetd/src/send_adclick2reply_bridge.cpp
        integration/legacy_bnetd/src/send_playerinforeply_bridge.cpp
        integration/legacy_bnetd/src/send_gamelistreply_bridge.cpp
        integration/legacy_bnetd/src/send_startgame1ack_bridge.cpp
        integration/legacy_bnetd/src/send_startgame3ack_bridge.cpp
        integration/legacy_bnetd/src/send_startgame4ack_bridge.cpp
        integration/legacy_bnetd/src/send_ladderreply_bridge.cpp
        integration/legacy_bnetd/src/send_laddersearchreply_bridge.cpp
        integration/legacy_bnetd/src/send_mapauthreply1_bridge.cpp
        integration/legacy_bnetd/src/send_mapauthreply2_bridge.cpp
        integration/legacy_bnetd/src/send_clandisbandreply_bridge.cpp
        integration/legacy_bnetd/src/send_clancreateinviteforward_bridge.cpp
        integration/legacy_bnetd/src/send_clancreateinvitereply_bridge.cpp
        integration/legacy_bnetd/src/send_raw_text_bridge.cpp
        integration/legacy_bnetd/src/d2cs_link_dispatch_bridge.cpp
        integration/legacy_bnetd/src/bot_dispatch_bridge.cpp
        integration/legacy_bnetd/src/telnet_dispatch_bridge.cpp
        integration/legacy_bnetd/src/wol_dispatch_bridge.cpp
        integration/legacy_bnetd/src/irc_dispatch_bridge.cpp
        integration/legacy_bnetd/src/connection_dispatch_bridge.cpp
        integration/legacy_bnetd/src/message_dispatch_bridge.cpp
        integration/legacy_bnetd/src/server_dispatch_bridge.cpp
        integration/legacy_bnetd/src/send_clan_motdreply_bridge.cpp
        integration/legacy_bnetd/src/encode_clanmemberupdate_bridge.cpp
        integration/legacy_bnetd/src/clan_bridges.cpp
        integration/legacy_bnetd/src/send_anongame_search_reply_bridge.cpp
        integration/legacy_bnetd/src/send_anongame_found_bridge.cpp
        integration/legacy_bnetd/src/send_anongame_cancel_bridge.cpp
        integration/legacy_bnetd/src/send_w3route_bridge.cpp
        integration/legacy_bnetd/src/send_udptest_bridge.cpp
        integration/legacy_bnetd/src/send_d2cs_bnetd_bridges.cpp
        integration/legacy_bnetd/src/send_file_bridge.cpp
        integration/legacy_bnetd/src/prefs_bridge.cpp
        # R243: bnetd per-connection timer subsystem bridges.
        integration/legacy_bnetd/src/timer_bridge.cpp
        # R244: bnetd IP-ban subsystem bridges.
        integration/legacy_bnetd/src/ipban_bridge.cpp
        # R245-R247: bnetd small-module lifecycle bridges (unified).
        # Originally three separate files (R245, R246, R247); collapsed into
        # one TU as a Plan 06 quick win (2026-05-30).
        # Subsystems: helpfile, autoupdate, output, support, mail,
        #             i18n, icons, attrlayer, tracker, team, udptest_send,
        #             alias_command, command_groups, anongame_maplists, handle_udp.
        # See: plans/16-strangler-completion-detail.md §5 "Can be done now"
        integration/legacy_bnetd/src/bnetd_lifecycle_bridges.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/integration/legacy_bnetd/include
    PUBLIC_DEPS
        core
        application_ports
        application_admin_commands
        application_anongame_infoply
        application_chat
        application_i18n
        application_init
        application_ads
        application_realm
        infra_compression
        infra_legacy_config
        protocol_common
        protocol_bnet
        $<$<TARGET_EXISTS:infra_config>:infra_config>
)

# ---- integration/legacy_d2cs (strangler-fig scaffold) -------------------
#
# Dispatcher-only scaffold for the d2cs server, mirroring the
# legacy_bnetd pattern. Provides the v3->legacy send_packet
# C ABI (`pvpgn_v3_d2cs_send_packet_try`) and a runtime-pluggable
# handler. The linked half (which would call legacy
# `packet_create(packet_class_d2cs)` + `conn_d2cs_outqueue`) is
# deferred to a future round, pending extraction of a
# `d2cs_legacy` static library from the d2cs executable target.

pvpgn_v3_add_library(integration_legacy_d2cs
    STATIC
    SOURCES
        integration/legacy_d2cs/src/send_packet_bridge.cpp
        integration/legacy_d2cs/src/send_loginreply_bridge.cpp
        integration/legacy_d2cs/src/send_createcharreply_bridge.cpp
        integration/legacy_d2cs/src/send_charloginreply_bridge.cpp
        integration/legacy_d2cs/src/send_creategamereply_bridge.cpp
        integration/legacy_d2cs/src/send_joingamereply_bridge.cpp
        integration/legacy_d2cs/src/send_init_bnetd_bridge.cpp
        integration/legacy_d2cs/src/send_authreply_bnetd_bridge.cpp
        integration/legacy_d2cs/src/send_gameinforeply_bnetd_bridge.cpp
        integration/legacy_d2cs/src/send_handle_d2gs_bridges.cpp
        integration/legacy_d2cs/src/send_simple_replies.cpp
        integration/legacy_d2cs/src/send_listreply_bridges.cpp
        integration/legacy_d2cs/src/send_ladderreply_bridge.cpp
        integration/legacy_d2cs/src/send_charlistreply_bridge.cpp
        integration/legacy_d2cs/src/send_outbound_obs_bridges.cpp
        integration/legacy_d2cs/src/d2cs_prefs_bridge.cpp
        # R233: observation lifecycle bridges (d2cs).
        integration/legacy_d2cs/src/bridge_logger.cpp
        integration/legacy_d2cs/src/d2ladder_bridge.cpp
        integration/legacy_d2cs/src/s2s_bridge.cpp
        integration/legacy_d2cs/src/handle_signal_bridge.cpp
        # R234: more d2cs observation bridges (loop + init + conn).
        integration/legacy_d2cs/src/server_bridge.cpp
        integration/legacy_d2cs/src/handle_init_bridge.cpp
        integration/legacy_d2cs/src/conn_bridge.cpp
        # R235: d2cs packet dispatcher bridges (d2cs/d2gs/bnetd).
        integration/legacy_d2cs/src/handle_d2cs_packet_bridge.cpp
        integration/legacy_d2cs/src/handle_d2gs_packet_bridge.cpp
        integration/legacy_d2cs/src/handle_bnetd_packet_bridge.cpp
        # R236: d2cs queue + d2gs-list lifecycle bridges.
        integration/legacy_d2cs/src/gamequeue_bridge.cpp
        integration/legacy_d2cs/src/serverqueue_bridge.cpp
        integration/legacy_d2cs/src/d2gs_bridge.cpp
        # R238: d2cs game catalogue + per-game lifecycle bridges.
        integration/legacy_d2cs/src/game_bridge.cpp
        # R241: d2cs net.cpp socket helper bridges.
        integration/legacy_d2cs/src/net_bridge.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/integration/legacy_d2cs/include
    PUBLIC_DEPS
        core
        protocol_common
        protocol_d2cs
        protocol_d2gs
        $<$<TARGET_EXISTS:infra_config>:infra_config>
)

# ---- integration/legacy_d2cs/linked (real d2cs_legacy hookup) -----------
#
# MOVED to src/v3/integration/legacy_d2cs/CMakeLists.txt by R196.b
# (same R193.fix systemic ordering bug as legacy_bnetd): the previous
# `if(TARGET d2cs_legacy)` guard here was always FALSE because
# `add_subdirectory(src/v3)` runs before `add_subdirectory(src)`, so
# `d2cs_legacy` did not yet exist. The block now lives in a separate
# CMakeLists added LATE in the top-level configure, after `src` has
# been processed. See plans/r196b-checklist.md.

# ---- integration/legacy_d2dbs (strangler-fig scaffold) -----------------
#
# Dispatcher-only scaffold for the d2dbs server, mirroring the
# legacy_d2cs pattern.

pvpgn_v3_add_library(integration_legacy_d2dbs
    STATIC
    SOURCES
        integration/legacy_d2dbs/src/send_packet_bridge.cpp
        integration/legacy_d2dbs/src/send_echorequest_bridge.cpp
        integration/legacy_d2dbs/src/d2dbs_prefs_bridge.cpp
        # R231: observation lifecycle bridges (d2dbs).
        integration/legacy_d2dbs/src/bridge_logger.cpp
        integration/legacy_d2dbs/src/charlock_bridge.cpp
        integration/legacy_d2dbs/src/d2ladder_bridge.cpp
        # R232: observation bridges for dupecheck / dbserver / signals.
        integration/legacy_d2dbs/src/dbsdupecheck_bridge.cpp
        integration/legacy_d2dbs/src/dbserver_bridge.cpp
        integration/legacy_d2dbs/src/handle_signal_bridge.cpp
        # R242: d2dbs packet dispatcher bridges (handle / timeout / keepalive).
        integration/legacy_d2dbs/src/dbspacket_bridge.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/integration/legacy_d2dbs/include
    PUBLIC_DEPS
        core
        protocol_common
        protocol_d2dbs
        $<$<TARGET_EXISTS:infra_config>:infra_config>
)

# ---- integration/legacy_d2dbs/linked (real d2dbs_legacy hookup) --------
#
# MOVED to src/v3/integration/legacy_d2dbs/CMakeLists.txt by R199, applying
# the R193.fix Strategy-B / R196.b playbook. The block below was dead code
# under the old ordering (`d2dbs_legacy` target did not exist yet at v3
# configure time). See plans/r199-checklist.md.

# ---- integration/legacy_bnetd/linked (real bnetd_legacy hookup) ---------
#
# MOVED to src/v3/integration/legacy_bnetd/CMakeLists.txt by R193.fix
# (Strategy B, two-pass `src/v3`). That CMakeLists is added LATE in the
# top-level configure -- after `add_subdirectory(src)` -- so that the
# `bnetd_legacy` target already exists when we check for it. The previous
# placement here was UNREACHABLE because src/v3 is processed before src/.
# See plans/r192-checklist.md + plans/r193-audit-checklist.md.


# ---- infra/net ----------------------------------------------------------
#
# Socket abstraction layer — v3 replacement for src/compat/psock.h.
# Two distinct layers live here:
#
#   socket.hpp        — Thin C++20 RAII wrapper over POSIX sockets / Winsock2.
#                       No Boost.Asio dependency — uses OS socket APIs directly.
#                       Replaces: psock.h + psock.cpp  [✅ R115]
#                       Types:    SocketFd, IpAddress, Port, SocketAddress,
#                                 UniqueSocket, WinsockGuard
#                       Factory:  make_tcp_socket(), make_udp_socket(),
#                                 bind_socket(), listen_socket(),
#                                 accept_connection(), connect_socket(),
#                                 set_nonblocking(), set_reuse_addr(),
#                                 socket_send(), socket_recv(),
#                                 socket_error(), socket_error_string()
#
#   io_runtime.hpp    — Boost.Asio IoRuntime (thread-pool, strand, timers)
#   tcp_acceptor.hpp  — Boost.Asio TcpAcceptor (async accept loop)
#   tcp_session.hpp   — Boost.Asio TcpSession (async read/write)
#   udp_endpoint.hpp  — Boost.Asio UdpEndpoint (async send/recv)
#   fiber_pool.hpp    — Boost.Fiber pool (optional, PVPGN_V3_WITH_FIBER)
#   fiber_session.hpp — Fiber-backed session wrapper (optional)
#   signal_handler.hpp — OS signal → async notification bridge
#   shutdown_coordinator.hpp — Graceful shutdown sequencer
#
# The Boost.Asio layer (infra_net target) requires PVPGN_V3_WITH_BOOST=ON.
# socket.hpp is header-only and has no Boost dependency — it is included
# in the infra_net PUBLIC_INCLUDES so it is available to all consumers.
#
# Caller migration (17 files) is tracked in plans/step8-networking-checklist.md
# and deferred to Round 116+.

if(PVPGN_V3_WITH_BOOST)
    set(_pvpgn_net_public_deps core Boost::system Threads::Threads)
    set(_pvpgn_net_deps)
    if(PVPGN_V3_WITH_FIBER)
        list(APPEND _pvpgn_net_public_deps Boost::fiber Boost::context)
    endif()
    find_package(Threads REQUIRED)
    pvpgn_v3_add_library(infra_net
        STATIC
        SOURCES
            infra/net/src/io_runtime.cpp
            infra/net/src/tcp_acceptor.cpp
            infra/net/src/tcp_session.cpp
            infra/net/src/udp_endpoint.cpp
            infra/net/src/fiber_pool.cpp
            infra/net/src/signal_handler.cpp
            infra/net/src/shutdown_coordinator.cpp
        PUBLIC_INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/infra/net/include
        PUBLIC_DEPS
            ${_pvpgn_net_public_deps}
            application_ports
            infra_config
        DEPS
            ${_pvpgn_net_deps}
    )
    if(PVPGN_V3_WITH_FIBER)
        target_compile_definitions(infra_net PUBLIC PVPGN_V3_HAVE_FIBER=1)
    endif()
    # Asio's any_executor equality operator trips -Wnull-dereference under
    # GCC; the access is in fact guarded but the analyser cannot see it.
    # Relaxed PUBLICly so consumers of infra_net inherit the suppression.
    # MSVC has no equivalent flag and rejects the GCC spelling outright,
    # so guard on compiler.
    if(NOT MSVC)
        target_compile_options(infra_net PUBLIC -Wno-null-dereference)
    endif()
endif()

# ---- infra/metrics (observability: Prometheus metrics, HTTP /metrics endpoint) --

pvpgn_v3_add_library(infra_metrics
    STATIC
    SOURCES
        infra/metrics/src/in_memory_metrics_registry.cpp
        infra/metrics/src/server_metrics.cpp
        infra/metrics/src/http_metrics_server.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/metrics/include
    PUBLIC_DEPS
        core
        application_ports
        infra_net
    DEPS
        Boost::system
)

# ---- infra/health (HTTP handlers for /healthz, /readyz, /varz, /metrics) ----

pvpgn_v3_add_library(infra_health
    STATIC
    SOURCES
        infra/health/src/health_handler.cpp
        infra/health/src/metrics_handler.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/health/include
    PUBLIC_DEPS
        core
        application_ports
)

# ---- infra/tracing (distributed tracing adapters: log + OTLP) -------------

option(PVPGN_V3_WITH_OTLP
    "Build the OTLP/HTTP trace exporter (requires Boost.Beast)"
    OFF)

set(_tracing_sources
    infra/tracing/src/log_trace_sink.cpp
)
if(PVPGN_V3_WITH_OTLP)
    list(APPEND _tracing_sources infra/tracing/src/otlp_trace_sink.cpp)
endif()

pvpgn_v3_add_library(infra_tracing
    STATIC
    SOURCES
        ${_tracing_sources}
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/tracing/include
    PUBLIC_DEPS
        core
        application_ports
    DEPS
        spdlog::spdlog
)

if(PVPGN_V3_WITH_OTLP)
    target_compile_definitions(infra_tracing PUBLIC PVPGN_V3_WITH_OTLP)
    target_link_libraries(infra_tracing PRIVATE Boost::system)
    message(STATUS "pvpgn v3: OTLP trace exporter enabled")
else()
    message(STATUS "pvpgn v3: OTLP trace exporter disabled (PVPGN_V3_WITH_OTLP=OFF)")
endif()

# ---- infra/webui (REST API server + HTML dashboard) -----------------------

# Header-only sub-library: JSON serialisers that do NOT require Boost.
# Testable without a live IoRuntime.
pvpgn_v3_add_library(infra_webui_json
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/infra/webui/include
    DEPS
        application_ports
        domain_chat
)

if(PVPGN_V3_WITH_BOOST)
    pvpgn_v3_add_library(infra_webui
        STATIC
        SOURCES
            infra/webui/src/web_server.cpp
        PUBLIC_DEPS
            core
            application_ports
            infra_net
            infra_webui_json
        DEPS
            Boost::system
    )
endif()

# ---- tools (utilities for migration and administration) ----------------------

add_subdirectory(tools/conf_converter)
add_subdirectory(tools/bnpass)
add_subdirectory(tools/bniutils)
add_subdirectory(tools/bntrackd)
add_subdirectory(tools/client)

# ---- services (composition roots — one per server process) ------------------
#
# Each services/<name>/ subdirectory defines a static library that wires
# infrastructure adapters together.  The app/<name>/ executables link against
# these libraries so that main() stays a thin bootstrap.

add_subdirectory(services)

# ---- app/bnetd (v3 composition root — standalone server binary) -------------
#
# Wires all v3 protocol FSMs (BnetFsm, BnftpFsm, WolFsm) together with a
# Boost.Asio event loop. Only built when Boost is available (infra_net dep).
# The guard is also inside app/bnetd/CMakeLists.txt for safety.

add_subdirectory(app/bnetd)

# ---- app/d2cs (D2CS session handler — Phase 4 Step 3) -------------------

add_subdirectory(app/d2cs)

# ---- app/d2dbs (D2DBS session handler — Phase 4 Step 6) -----------------

add_subdirectory(app/d2dbs)

# ---- app/pvpgn-migrate (R321/R322/R323 — account migration CLI tool) ----
#
# Standalone tool for migrating legacy *.plain account files to SQLite or
# TOML-file storage backends.  No Boost dependency — always built.

add_subdirectory(app/pvpgn-migrate)

# R332: CLI tool for validating and inspecting server configuration files.
# Supports --validate, --print-effective, --print-schema.  No Boost dependency.
add_subdirectory(app/pvpgn-config)

# ---- tests --------------------------------------------------------------

if(PVPGN_V3_BUILD_TESTS)
    # Set up Catch2 for all test subdirectories
    include(FetchContent)
    
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    
    # Ensure Catch2 is built with C++20 to support std::string_view
    foreach(_t Catch2 Catch2WithMain)
        if(TARGET ${_t})
            target_compile_features(${_t} PUBLIC cxx_std_20)
            set_target_properties(${_t} PROPERTIES SYSTEM TRUE)
        endif()
    endforeach()
    
    enable_testing()
    include(CTest)
    
    add_subdirectory(
        ${CMAKE_SOURCE_DIR}/tests/unit
        ${CMAKE_BINARY_DIR}/tests/unit)
    add_subdirectory(
        ${CMAKE_SOURCE_DIR}/tests/functional
        ${CMAKE_BINARY_DIR}/tests/functional)
    add_subdirectory(
        ${CMAKE_SOURCE_DIR}/tests/e2e
        ${CMAKE_BINARY_DIR}/tests/e2e)
endif()
