/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999  Ross Combs (rocombs@cs.nmsu.edu)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef INCLUDED_SERVER_TYPES
#define INCLUDED_SERVER_TYPES

#include <ctime>

namespace pvpgn
{

	namespace bnetd
	{

#ifdef SERVER_INTERNAL_ACCESS

		typedef enum
		{
			laddr_type_bnet,	/* classic battle.net service (usually on port 6112) */
			laddr_type_w3route, /* warcraft 3 playgame routing (def. port 6200) */
			laddr_type_irc,   	/* Internet Relay Chat service (port is varying; mostly on port 6667 or 7000) */
			laddr_type_wolv1,   /* Westwood Online v1 (IRC) Services (port is 4000) */
			laddr_type_wolv2,   /* Westwood Online v2 (IRC) Services (port is 4005) */
			laddr_type_apireg,  /* Westwood API Register Services (port is 5700) */
			laddr_type_wgameres,/* Westwood gameres Services (port is 4807) */
			laddr_type_telnet 	/* telnet service (usually on port 23) */
		} t_laddr_type;


		/* listen address structure */
		typedef struct
		{
			int          ssocket; /* TCP listen socket */
			int          usocket; /* UDP socket */
			t_laddr_type type;
		} t_laddr_info;

#endif

		extern std::time_t now;

	}

}

#endif


/*****/
#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_SERVER_PROTOS
#define INCLUDED_SERVER_PROTOS

#include <vector>
#include <functional>

// Forward-declare the C-library `sockaddr_in` at global scope so
// that elaborated specifiers inside `namespace pvpgn::bnetd` below
// resolve to `::sockaddr_in` (defined by <winsock2.h> / <netinet/in.h>
// in translation units that include the full header), instead of
// creating a stray `pvpgn::bnetd::sockaddr_in` forward declaration.
struct sockaddr_in;

namespace pvpgn
{

	namespace bnetd
	{
		// Forward-declare for the v3 owned-socket factory below.
		// Defined in `connection.h`.
		struct connection;
		typedef struct connection t_connection;

		enum restart_mode
		{
			restart_mode_none, // always first (0)

			restart_mode_all,
			restart_mode_i18n,
			restart_mode_channels,
			restart_mode_realms,
			restart_mode_autoupdate,
			restart_mode_news,
			restart_mode_versioncheck,
			restart_mode_ipbans,
			restart_mode_helpfile,
			restart_mode_banners,
			restart_mode_tracker,
			restart_mode_commandgroups,
			restart_mode_aliasfile,
			restart_mode_transfile,
			restart_mode_tournament,
			restart_mode_icons,
			restart_mode_anongame,
			restart_mode_lua
		};

		extern unsigned int server_get_uptime(void);
		extern unsigned int server_get_starttime(void);
		extern void server_quit_delay(int delay);
		extern void server_set_hostname(void);
		extern char const * server_get_hostname(void);
		extern void server_clear_hostname(void);
		extern int server_process(void);

		extern void server_quit_wraper(void);
		extern void server_restart_wraper(int mode);
		extern void server_save_wraper(void);

		/* --- v3 strangler-fig hooks ---------------------------- */

		/* When set to true *before* `server_process()` runs the
		 * setup phase, the legacy server skips registering bnet
		 * UDP sockets with the fdwatch loop. The sockets are still
		 * opened, bound, and made non-blocking (so callers can
		 * adopt them). The v3 integration uses this to take over
		 * UDP reception with `infra::net::UdpEndpoint`.
		 */
		extern void server_set_skip_legacy_udp_fdwatch(bool skip);
		extern bool server_get_skip_legacy_udp_fdwatch(void);

		/* Hook fired by `server_process()` once all listen
		 * sockets have been set up and before the main fdwatch
		 * loop starts. Used by the v3 integration to install its
		 * UDP bridge against the reserved fds while they are
		 * known to be valid. Pass `nullptr` to clear.
		 */
		extern void server_set_after_setup_hook(void (*hook)(void));

		/* Hook fired by `server_process()` after the main loop
		 * exits and before listen sockets are closed. Used by the
		 * v3 integration to stop its UDP bridge while the fds are
		 * still valid.
		 */
		extern void server_set_before_shutdown_hook(void (*hook)(void));

		/* Returns the list of UDP socket fds that the legacy
		 * server opened for `laddr_type_bnet` listeners. Only
		 * meaningful after `server_process()` has completed its
		 * setup phase and before `server_process()` returns.
		 * Caller must not close the fds.
		 */
		extern std::vector<int> server_get_bnet_udp_fds(void);

		/* When set to true *before* `server_process()` runs the
		 * setup phase, the legacy server skips registering bnet
		 * TCP listening sockets with the fdwatch loop. The
		 * sockets are still opened, bound, listening, and made
		 * non-blocking so callers can adopt them. The v3
		 * integration uses this to take over TCP accept with
		 * `infra::net::TcpAcceptor`.
		 *
		 * Only `laddr_type_bnet` listeners are affected. Other
		 * listener types (IRC, WOL, telnet, w3route, wgameres,
		 * apireg) keep the legacy accept loop.
		 */
		extern void server_set_skip_legacy_tcp_fdwatch(bool skip);
		extern bool server_get_skip_legacy_tcp_fdwatch(void);

		/* Per-listener metadata captured at setup time for the
		 * v3 TCP bridge. `ssocket` is the TCP listen fd, `usocket`
		 * is the matching UDP fd for `laddr_type_bnet` listeners
		 * (or -1 otherwise). Addresses are in host byte order.
		 */
		struct bnet_tcp_listener_info {
			int            ssocket;
			int            usocket;
			unsigned int   laddr_ip;
			unsigned short laddr_port;
			int            type;          /* t_laddr_type cast to int */
			void *         opaque_laddr;  /* `t_addr *` for callbacks */
		};

		extern std::vector<bnet_tcp_listener_info> server_get_bnet_tcp_listeners(void);

		/* Detach the listening fd at @p listener_index from legacy
		 * ownership.  After this call legacy's `_shutdown_addrs`
		 * will NOT `psock_close()` the fd; the caller (v3 bridge)
		 * must close it.  Returns the original fd, or -1 if the
		 * index is out of range or the listener was already
		 * released. */
		extern int server_release_bnet_tcp_listener_fd(std::size_t listener_index);

		/* Hand a v3-accepted TCP socket back to legacy bnetd so
		 * it can run the post-accept setup (ipban check,
		 * SO_KEEPALIVE, getsockname, non-blocking, conn_create,
		 * conn_add_fdwatch, listener-type specific class/state).
		 * Returns 0 on success, -1 on failure; on failure the
		 * legacy side has already closed `csocket`.
		 * `caddr` must be a fully populated `sockaddr_in` for the
		 * peer (network byte order). `listener_index` is an index
		 * into the vector returned by
		 * `server_get_bnet_tcp_listeners()`.
		 */
		extern int server_handle_v3_accepted_bnet_socket(
		    std::size_t              listener_index,
		    int                      csocket,
		    struct sockaddr_in const* caddr);

		/* v3 strangler-fig (38f): factory that produces a
		 * `t_connection*` for an accepted bnet socket whose fd is
		 * owned by a v3 infra::net::TcpSession. Reuses the same
		 * pipeline as `server_handle_v3_accepted_bnet_socket` -- ip
		 * ban check, keepalive, getsockname, conn_create, bnet
		 * initkill timer -- BUT skips `conn_add_fdwatch` and does
		 * NOT call `psock_close` on the fd. Marks the returned
		 * connection with `conn_set_v3_owns_socket(c, 1)` before
		 * returning so `conn_destroy` will not double-close the fd.
		 * Returns the new connection on success or `NULL` on
		 * failure. On failure the v3 caller is responsible for
		 * closing the socket via its `TcpSession`. */
		extern t_connection * server_handle_v3_owned_bnet_socket(
		    std::size_t               listener_index,
		    int                       csocket,
		    struct sockaddr_in const* caddr);

		/* v3 strangler-fig (38g): post a callback to be invoked
		 * on the legacy main loop thread. Used by the TcpBridge
		 * to defer `conn_destroy` (and other connlist-touching
		 * work) off the Asio worker threads so it runs in the
		 * same single-threaded context as `timerlist_check_timers`
		 * and `fdwatch_handle`. Safe to call from any thread.
		 * The callback is invoked exactly once, on the next
		 * iteration of the main loop, before `connlist_reap`.
		 * If the main loop has already exited, queued callbacks
		 * are silently dropped (process is going away). */
		extern void server_post_to_main(std::function<void()> fn);

	}

}

#endif
#endif
