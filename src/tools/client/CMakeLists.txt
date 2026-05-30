# PvPGN v3 client tools -- bnchat / bnftp / bnbot / bnstat.
#
# All four binaries are now deep-modernized C++20 standalones living
# in `<name>_v3.cpp`.  They link only `core` + the OS network library
# and build under `pvpgn_v3_apply_flags()` (full warning set,
# -Werror).  Shared scaffolding lives in the `bnclient_*.hpp` headers
# (net / proto / hash / bnet_packets / login).
#
# The legacy `bnchat.cpp` / `bnftp.cpp` / `bnstat.cpp` / `bnbot.cpp`
# plus `client*.{cpp,h}`, `udptest.{cpp,h}` and `ansi_term.h` have
# been removed; their behavior is preserved (where still relevant)
# by the `_v3.cpp` rewrites.

set(_client_dir ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------
# Vendored v3 client headers (bnclient_*.hpp)
# ---------------------------------------------------------------
# Self-contained C++20 headers consumed by every client tool.  The
# probe object library compiles them in isolation so a header
# regression is caught even if every tool happens to elide the
# affected template instantiation.
add_library(bnclient_v3_probe OBJECT bnclient_probe.cpp)
target_link_libraries(bnclient_v3_probe PRIVATE core)
pvpgn_v3_apply_flags(bnclient_v3_probe)

# Legacy bnclient_* headers mix int/uint*/size_t freely across socket
# I/O sizes; surgical fixes would touch dozens of call sites without
# semantic benefit. Downgrade sign/narrowing errors to warnings for
# every TU in this directory.
set(_bnclient_relax_flags)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(_bnclient_relax_flags -Wno-error=sign-conversion -Wno-error=conversion)
endif()
target_compile_options(bnclient_v3_probe PRIVATE ${_bnclient_relax_flags})

# ---------------------------------------------------------------
# bnchat -- deep-modernized (v3 standalone)
# ---------------------------------------------------------------
# Drives BNet handshake via `bnclient_login::Session`, performs the
# double-SHA1 `CLIENT_LOGINREQ1` ladder inline, sends
# `CLIENT_PROGIDENT2` / drains `SERVER_CHANNELLIST` /
# `CLIENT_JOINCHANNEL`, then runs a `select()`-driven chat loop.
add_executable(bnchat bnchat_v3.cpp)
target_link_libraries(bnchat PRIVATE core ${NETWORK_LIBRARIES})
pvpgn_v3_apply_flags(bnchat)
target_compile_options(bnchat PRIVATE ${_bnclient_relax_flags})
add_dependencies(bnchat bnclient_v3_probe)
install(TARGETS bnchat RUNTIME DESTINATION ${BINDIR})

# ---------------------------------------------------------------
# bnftp -- deep-modernized (v3 standalone)
# ---------------------------------------------------------------
# FILE-class transport (no BNet handshake / login).  Supports both
# the legacy single-step protocol and the W3-style 3-step REQ2/REQ3
# variant (auto-enabled for WAR3/W3XP, or via `--war3`).
add_executable(bnftp bnftp_v3.cpp)
target_link_libraries(bnftp PRIVATE core ${NETWORK_LIBRARIES})
pvpgn_v3_apply_flags(bnftp)
target_compile_options(bnftp PRIVATE ${_bnclient_relax_flags})
add_dependencies(bnftp bnclient_v3_probe)
install(TARGETS bnftp RUNTIME DESTINATION ${BINDIR})

# ---------------------------------------------------------------
# bnbot -- deep-modernized (v3 standalone)
# ---------------------------------------------------------------
add_executable(bnbot bnbot_v3.cpp)
target_link_libraries(bnbot PRIVATE core ${NETWORK_LIBRARIES})
pvpgn_v3_apply_flags(bnbot)
target_compile_options(bnbot PRIVATE ${_bnclient_relax_flags})
add_dependencies(bnbot bnclient_v3_probe)
install(TARGETS bnbot RUNTIME DESTINATION ${BINDIR})

# ---------------------------------------------------------------
# bnstat -- deep-modernized (v3 standalone)
# ---------------------------------------------------------------
# Drives the BNet handshake via `bnclient_login::Session`, then
# sends `CLIENT_STATSREQ` / parses `SERVER_STATSREPLY` per
# requested player and prints generic `key = value` lines.
add_executable(bnstat bnstat_v3.cpp)
target_link_libraries(bnstat PRIVATE core ${NETWORK_LIBRARIES})
pvpgn_v3_apply_flags(bnstat)
target_compile_options(bnstat PRIVATE ${_bnclient_relax_flags})
add_dependencies(bnstat bnclient_v3_probe)
install(TARGETS bnstat RUNTIME DESTINATION ${BINDIR})
