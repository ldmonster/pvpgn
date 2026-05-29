# PvPGN v3 bntrackd -- UDP server-tracker daemon (modernized).
#
# Modernized v3 port of legacy `src/bntrackd/`:
#   * No link against legacy `common` / `compat`.
#   * Logging through v3 `core` (`LOG_INFO` / `LOG_DEBUG` / `LOG_ERROR`).
#   * Sockets via direct POSIX `<sys/socket.h>` / Winsock2 (no
#     `compat/psock`).
#   * `std::vector<ServerEntry>` instead of legacy `t_list` + `xmalloc`.
#   * Builds under the full v3 warning set (no `-w` envelope).
#
# See plans/refactoring-plan-legacy-tools.md.

add_executable(bntrackd bntrackd.cpp)

pvpgn_v3_apply_flags(bntrackd)

target_link_libraries(bntrackd PRIVATE core ${NETWORK_LIBRARIES})

target_compile_definitions(bntrackd PRIVATE
    $<$<BOOL:${PVPGN_VERSION}>:PVPGN_VERSION="${PVPGN_VERSION}">)

install(TARGETS bntrackd RUNTIME DESTINATION ${SBINDIR})
