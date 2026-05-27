/*
 * Copyright (C) 1998,1999  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 2000,2001  Marco Ziech (mmz@gmx.net)
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
#include "common/setup_before.h"
#define SERVER_INTERNAL_ACCESS
#include "server.h"

#include <cctype>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <vector>
#include <utility>
#include <functional>

#ifdef DO_POSIXSIG
# include <signal.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include "compat/strerror.h"
#ifndef _WIN32
#  include <sys/socket.h>
#  include <sys/select.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
#  include <netinet/in.h>
#  include <netdb.h>
#endif
#include "common/fdwatch.h"
#include "common/addr.h"
#include "common/eventlog.h"
#include "common/packet.h"
#include "common/hexdump.h"
#include "common/network.h"
#include "common/list.h"
#include "common/trans.h"
#ifdef WIN32
# include "win32/service.h"
# include "windows.h"
#endif
#include "common/util.h"

#include "prefs_v3_shim.h"
#include "connection.h"
#include "ipban.h"
#include "timer.h"
#include "handle_bnet.h"
#include "handle_bot.h"
#include "handle_telnet.h"
// R194: handle_init.h was deleted in commit 03f35f9 but not all callers
// were updated. The function `handle_init_packet` is now defined in
// `src/v3/integration/legacy_bnetd/src/init_packet_dispatch_link.cpp`
// (under PVPGN_V3_BNETD_INTEGRATION) or `src/bnetd/handle_init.cpp` (the
// orphan legacy file). Forward-declare it here in lieu of the removed
// header so we do not reintroduce the dangling #include.
namespace pvpgn { namespace bnetd {
extern int handle_init_packet(t_connection * c, t_packet const * const packet);
} }
#include "handle_d2cs.h"
#include "irc.h"
#include "handle_udp.h"
#include "handle_apireg.h"
#include "anongame.h"
#include "clan.h"
#include "attrlayer.h"
#include "account.h"
#include "message.h"
#include "game.h"
#include "cmdline.h"
#include "tracker.h"
#include "ladder.h"
#include "output.h"
#include "channel.h"
#include "realm.h"
#include "autoupdate.h"
#include "news.h"
#include "versioncheck.h"
#include "helpfile.h"
#include "adbanner.h"
#include "command_groups.h"
#include "alias_command.h"
#include "tournament.h"
#include "icons.h"
#include "anongame_infos.h"
#include "topic.h"
#include "i18n.h"

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_WS2TCPIP_H
# include <Ws2tcpip.h>
#endif

#include "common/setup_after.h"

#ifdef WITH_LUA
#include "luainterface.h"
#endif

#ifdef PVPGN_V3_BNETD_INTEGRATION
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include "core/logging.hpp"
#include "infra/log/json_line_logger.hpp"
#include "integration/legacy_bnetd/legacy_event_logger.hpp"
#include "integration/legacy_bnetd/legacy_chat_reply_sink.hpp"
#include "integration/legacy_bnetd/chat_reply_sink_override.hpp"
#include "application/i18n/string_table.hpp"
#include "application/auth/password_rotation_observer.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "integration/legacy_bnetd/install_v3_handlers.hpp"
extern "C" int pvpgn_v3_server_dispatch_try(void* opaque, char const* op) noexcept;
#endif

#include "server_v3_hook.h"

extern std::FILE * hexstrm; /* from main.c */
extern int g_ServiceStatus;

namespace pvpgn
{

	namespace bnetd
	{


		/* xstrdup-equivalent using new char[] for paired delete[] cleanup. */
		static char* sv_strdup(char const* s)
		{
			if (!s) return nullptr;
			std::size_t n = std::strlen(s) + 1;
			char* r = new char[n];
			std::memcpy(r, s, n);
			return r;
		}
#ifdef DO_POSIXSIG
		static void quit_sig_handle(int unused);
		static void restart_sig_handle(int unused);
		static void save_sig_handle(int unused);
#ifdef HAVE_SETITIMER
		static void timer_sig_handle(int unused);
#endif
#endif


		time_t now;
		static std::time_t starttime;
		static std::time_t curr_exittime;
		static volatile std::time_t sigexittime = 0;
		static volatile int do_restart = 0;
		static volatile int do_save = 0;
		static volatile int got_epipe = 0;
		static char const * server_hostname = NULL;

		/* --- v3 strangler-fig hooks: see server.h for contract --- */
		static bool             skip_legacy_udp_fdwatch = false;
		static std::vector<int> bnet_udp_fds;
		static bool             skip_legacy_tcp_fdwatch = false;
		static std::vector<bnet_tcp_listener_info> bnet_tcp_listeners;
		static void             (*after_setup_hook)(void) = nullptr;
		static void             (*before_shutdown_hook)(void) = nullptr;

		extern void server_set_skip_legacy_udp_fdwatch(bool skip)
		{
			skip_legacy_udp_fdwatch = skip;
		}

		extern bool server_get_skip_legacy_udp_fdwatch(void)
		{
			return skip_legacy_udp_fdwatch;
		}

		extern void server_set_skip_legacy_tcp_fdwatch(bool skip)
		{
			skip_legacy_tcp_fdwatch = skip;
		}

		extern bool server_get_skip_legacy_tcp_fdwatch(void)
		{
			return skip_legacy_tcp_fdwatch;
		}

		extern std::vector<bnet_tcp_listener_info> server_get_bnet_tcp_listeners(void)
		{
			return bnet_tcp_listeners;
		}

		/* --- v3 strangler-fig (38g): main-loop post seam --- */
		static std::mutex                       pending_main_mu;
		static std::vector<std::function<void()>> pending_main_q;
		static volatile bool                    pending_main_active = false;

		extern void server_post_to_main(std::function<void()> fn)
		{
			if (!fn) return;
			std::lock_guard<std::mutex> g{pending_main_mu};
			if (!pending_main_active) {
				// Main loop has exited (or never started). Drop
				// the callback -- the process is shutting down and
				// touching connlist from here would race with
				// `_shutdown_conns`.
				return;
			}
			pending_main_q.push_back(std::move(fn));
		}

		// Drain queued main-loop callbacks. Called once per main-loop
		// iteration, before `connlist_reap`, so callbacks see a
		// consistent connlist and can safely call `conn_destroy`.
		static void drain_pending_main(void)
		{
			std::vector<std::function<void()>> local;
			{
				std::lock_guard<std::mutex> g{pending_main_mu};
				if (pending_main_q.empty()) return;
				local.swap(pending_main_q);
			}
			for (auto& fn : local) {
				try { fn(); }
				catch (...) {
					eventlog(eventlog_level_error, __FUNCTION__,
					    "exception escaped a main-loop callback; ignored");
				}
			}
		}

		extern int server_release_bnet_tcp_listener_fd(std::size_t listener_index)
		{
			if (listener_index >= bnet_tcp_listeners.size()) {
				return -1;
			}
			auto & info = bnet_tcp_listeners[listener_index];
			const int fd = info.ssocket;
			info.ssocket = -1;
			// Also null out the legacy laddr_info entry so that
			// `_shutdown_addrs()` skips its `psock_close()`. After
			// this returns, the caller owns the fd and is
			// responsible for closing it.
			if (auto * curr_laddr = static_cast<t_addr *>(info.opaque_laddr)) {
				if (auto * laddr_info = static_cast<t_laddr_info *>(addr_get_data(curr_laddr).p)) {
					laddr_info->ssocket = -1;
				}
			}
			return fd;
		}

		extern void server_set_after_setup_hook(void (*hook)(void))
		{
			after_setup_hook = hook;
		}

		extern void server_set_before_shutdown_hook(void (*hook)(void))
		{
			before_shutdown_hook = hook;
		}

		extern std::vector<int> server_get_bnet_udp_fds(void)
		{
			return bnet_udp_fds;
		}

		// Forward decl: definition lives further down with the other
		// `sd_*` helpers. Declared here so the v3 strangler-fig
		// callback `server_handle_v3_accepted_bnet_socket` (below)
		// can refer to it.
		static int sd_finalize_accepted(
		    t_addr const *             curr_laddr,
		    t_laddr_info const *       laddr_info,
		    int                        csocket,
		    struct sockaddr_in const & caddr);

		extern int server_handle_v3_accepted_bnet_socket(
		    std::size_t              listener_index,
		    int                      csocket,
		    struct sockaddr_in const* caddr)
		{
			if (listener_index >= bnet_tcp_listeners.size()) {
				eventlog(eventlog_level_error, __FUNCTION__, "v3 accept callback: listener index {} out of range ({} listeners)", listener_index, bnet_tcp_listeners.size());
				close(csocket);
				return -1;
			}
			if (!caddr) {
				eventlog(eventlog_level_error, __FUNCTION__, "v3 accept callback: null peer address");
				close(csocket);
				return -1;
			}
	
			auto * curr_laddr  = static_cast<t_addr *>(bnet_tcp_listeners[listener_index].opaque_laddr);
			auto * laddr_info  = static_cast<t_laddr_info *>(addr_get_data(curr_laddr).p);
			if (!laddr_info) {
				eventlog(eventlog_level_error, __FUNCTION__, "v3 accept callback: null laddr_info for index {}", listener_index);
				close(csocket);
				return -1;
			}

			return sd_finalize_accepted(curr_laddr, laddr_info, csocket, *caddr);
		}

		extern t_connection * server_handle_v3_owned_bnet_socket(
		    std::size_t              listener_index,
		    int                      csocket,
		    struct sockaddr_in const* caddr)
		{
			if (listener_index >= bnet_tcp_listeners.size()) {
				eventlog(eventlog_level_error, __FUNCTION__, "v3 owned-socket factory: listener index {} out of range ({} listeners)", listener_index, bnet_tcp_listeners.size());
				return NULL;
			}
			if (!caddr) {
				eventlog(eventlog_level_error, __FUNCTION__, "v3 owned-socket factory: null peer address");
				return NULL;
			}

			auto * curr_laddr  = static_cast<t_addr *>(bnet_tcp_listeners[listener_index].opaque_laddr);
			auto * laddr_info  = static_cast<t_laddr_info *>(addr_get_data(curr_laddr).p);
			if (!laddr_info) {
				eventlog(eventlog_level_error, __FUNCTION__, "v3 owned-socket factory: null laddr_info for index {}", listener_index);
				return NULL;
			}

			char tempa[32];
			if (!addr_get_addr_str(curr_laddr, tempa, sizeof(tempa)))
				std::strcpy(tempa, "x.x.x.x:x");

			if (curr_exittime) {
				/* shutting down -- refuse new connections */
				return NULL;
			}

			char addrstr[INET_ADDRSTRLEN] = { 0 };
			if (ipbanlist_check(inet_ntop(AF_INET, &(caddr->sin_addr), addrstr, sizeof(addrstr))) != 0)
			{
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] v3-owned: connection from banned address {} denied", csocket, addrstr);
				return NULL;
			}

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] v3-owned: accepted connection from {} on {}", csocket, addr_num_to_addr_str(ntohl(caddr->sin_addr.s_addr), ntohs(caddr->sin_port)), tempa);

			if (prefs_v3::use_keepalive())
			{
				int val = 1;
				if (setsockopt(csocket, SOL_SOCKET, SO_KEEPALIVE, &val, (socklen_t)sizeof(val)) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] v3-owned: could not set socket option SO_KEEPALIVE (setsockopt: {})", csocket, pstrerror(errno));
			}
	
			unsigned int   raddr;
			unsigned short rport;
			{
				struct sockaddr_in rsaddr;
				socklen_t          rlen;
				std::memset(&rsaddr, 0, sizeof(rsaddr));
				rlen = sizeof(rsaddr);
				if (getsockname(csocket, (struct sockaddr *)&rsaddr, &rlen) < 0
				    || rsaddr.sin_family != AF_INET)
				{
					raddr = addr_get_ip(curr_laddr);
					rport = addr_get_port(curr_laddr);
				}
				else
				{
					raddr = ntohl(rsaddr.sin_addr.s_addr);
					rport = ntohs(rsaddr.sin_port);
				}
			}

			/* Asio drives reads/writes for this fd, so legacy MUST
			 * keep it in blocking mode-from-our-side -- we do NOT
			 * set O_NONBLOCK here. Asio sets its own mode on
			 * the underlying socket. */

			t_connection * c = conn_create(csocket, laddr_info->usocket,
			    raddr, rport,
			    addr_get_ip(curr_laddr), addr_get_port(curr_laddr),
			    ntohl(caddr->sin_addr.s_addr), ntohs(caddr->sin_port));
			if (!c) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] v3-owned: unable to create new connection", csocket);
				return NULL;
			}

			/* Mark BEFORE any teardown path can fire so that an
			 * early `conn_destroy` won't double-close our fd. */
			conn_set_v3_owns_socket(c, 1);

			/* Only the bnet listener type makes sense here -- v3
			 * TcpBridge only adopts bnet listeners. For safety we
			 * still attach the initkill timer used by the legacy
			 * bnet branch. */
			if (laddr_info->type == laddr_type_bnet) {
				int delay = prefs_v3::initkill_timer();
				if (delay) {
					t_timer_data data;
					data.p = NULL;
					timerlist_add_timer(c, std::time(NULL) + delay, conn_shutdown, data);
				}
			}

			return c;
		}

		extern void server_quit_delay(int delay)
		{
			/* getting negative delay is ok too because it will force immediate
			 * shutdown */
			if (delay)
				sigexittime = std::time(NULL) + delay;
			else
				sigexittime = 0;
		}


		extern void server_quit_wraper(void)
		{
			if (sigexittime)
				sigexittime -= prefs_v3::shutdown_decr();
			else
				sigexittime = std::time(NULL) + (std::time_t)prefs_v3::shutdown_delay();
		}

		extern void server_restart_wraper(int mode){
			do_restart = mode;
		}

		extern void server_save_wraper(void){
			do_save = 1;
		}

#ifdef DO_POSIXSIG
		static void quit_sig_handle(int unused)
		{
			server_quit_wraper();
		}


		static void restart_sig_handle(int unused)
		{
			do_restart = 1;
		}


		static void save_sig_handle(int unused)
		{
			do_save = 1;
		}

		static void pipe_sig_handle(int unused)
		{
			got_epipe = 1;
		}

#ifdef HAVE_SETITIMER
		static void timer_sig_handle(int unused)
		{
			std::time(&now);
		}
#endif

		static void forced_quit_sig_handle(int unused)
		{
			server_quit_delay(-1);	/* programs shutdown 1 second before now */
		}
#endif


		extern unsigned int server_get_uptime(void)
		{
			return (unsigned int)std::difftime(std::time(NULL), starttime);
		}


		extern unsigned int server_get_starttime(void)
		{
			return (unsigned int)starttime;
		}


		static char const * laddr_type_get_str(t_laddr_type laddr_type)
		{
			switch (laddr_type)
			{
			case laddr_type_bnet:
				return "bnet";
			case laddr_type_w3route:
				return "w3route";
			case laddr_type_irc:
				return "irc";
			case laddr_type_wolv1:
				return "wolv1";
			case laddr_type_wolv2:
				return "wolv2";
			case laddr_type_apireg:
				return "apireg";
			case laddr_type_wgameres:
				return "wgameres";
			case laddr_type_telnet:
				return "telnet";
			default:
				return "UNKNOWN";
			}
		}


		static int handle_accept(void *data, t_fdwatch_type rw);
		static int handle_tcp(void *data, t_fdwatch_type rw);
		static int handle_udp(void *data, t_fdwatch_type rw);

		/* Per-connection setup that runs after a TCP socket has
		 * been accepted (either via the legacy fdwatch loop or
		 * the v3 TcpAcceptor strangler-fig). Performs ipban check,
		 * SO_KEEPALIVE, getsockname, non-blocking flip, conn_create,
		 * conn_add_fdwatch, and listener-type specific setup.
		 * Returns 0 on success, -1 on failure (csocket is closed
		 * or its associated t_connection is marked for destruction
		 * before returning -1).
		 */
		static int sd_finalize_accepted(
		    t_addr const *             curr_laddr,
		    t_laddr_info const *       laddr_info,
		    int                        csocket,
		    struct sockaddr_in const & caddr)
		{
			unsigned int   raddr;
			unsigned short rport;
			char           tempa[32];

			if (!addr_get_addr_str(curr_laddr, tempa, sizeof(tempa)))
				std::strcpy(tempa, "x.x.x.x:x");

			/* dont accept new connections while shutting down */
			if (curr_exittime) {
				shutdown(csocket, SHUT_RDWR);
				close(csocket);
				return 0;
			}
	
			char addrstr[INET_ADDRSTRLEN] = { 0 };
			if (ipbanlist_check(inet_ntop(AF_INET, &(caddr.sin_addr), addrstr, sizeof(addrstr))) != 0)
			{
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] connection from banned address {} denied (closing connection)", csocket, addrstr);
				close(csocket);
				return -1;
			}

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] accepted connection from {} on {}", csocket, addr_num_to_addr_str(ntohl(caddr.sin_addr.s_addr), ntohs(caddr.sin_port)), tempa);

			if (prefs_v3::use_keepalive())
			{
				int val = 1;

				if (setsockopt(csocket, SOL_SOCKET, SO_KEEPALIVE, &val, (socklen_t)sizeof(val)) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not set socket option SO_KEEPALIVE (setsockopt: {})", csocket, pstrerror(errno));
				/* not a fatal error */
			}

			{
				struct sockaddr_in rsaddr;
				socklen_t          rlen;
	
				std::memset(&rsaddr, 0, sizeof(rsaddr));
				rlen = sizeof(rsaddr);
				if (getsockname(csocket, (struct sockaddr *)&rsaddr, &rlen) < 0)
				{
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] unable to determine real local port (getsockname: {})", csocket, pstrerror(errno));
					raddr = addr_get_ip(curr_laddr);
					rport = addr_get_port(curr_laddr);
				}
				else
				{
					if (rsaddr.sin_family != AF_INET)
					{
						eventlog(eventlog_level_error, __FUNCTION__, "local address returned with bad address family {}", (int)rsaddr.sin_family);
						raddr = addr_get_ip(curr_laddr);
						rport = addr_get_port(curr_laddr);
					}
					else
					{
						raddr = ntohl(rsaddr.sin_addr.s_addr);
						rport = ntohs(rsaddr.sin_port);
					}
				}
			}

			if (fcntl(csocket, F_SETFL, O_NONBLOCK) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not set TCP socket to non-blocking mode (closing connection) (fcntl: {})", csocket, pstrerror(errno));
				close(csocket);
				return -1;
			}

			{
				t_connection * c;

				if (!(c = conn_create(csocket, laddr_info->usocket, raddr, rport, addr_get_ip(curr_laddr), addr_get_port(curr_laddr), ntohl(caddr.sin_addr.s_addr), ntohs(caddr.sin_port))))
				{
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] unable to create new connection (closing connection)", csocket);
					close(csocket);
					return -1;
				}

				if (conn_add_fdwatch(c, handle_tcp) < 0) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] unable to add socket to fdwatch pool (max connections?)", csocket);
					conn_set_state(c, conn_state_destroy);
					return -1;
				}

				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] client connected to a {} listening address", csocket, laddr_type_get_str(laddr_info->type));
				switch (laddr_info->type)
				{
				case laddr_type_irc:
				case laddr_type_wolv1:
				case laddr_type_wolv2:
					conn_set_class(c, conn_class_ircinit);
					conn_set_state(c, conn_state_connected);
					break;
				case laddr_type_apireg:
					conn_set_class(c, conn_class_apireg);
					conn_set_state(c, conn_state_connected);
					break;
				case laddr_type_wgameres:
					conn_set_class(c, conn_class_wgameres);
					conn_set_state(c, conn_state_connected);
					break;
				case laddr_type_w3route:
					conn_set_class(c, conn_class_w3route);
					conn_set_state(c, conn_state_connected);
					break;
				case laddr_type_telnet:
					conn_set_class(c, conn_class_telnet);
					conn_set_state(c, conn_state_connected);
					break;
				case laddr_type_bnet:
				{	/* add a timer to close stale connections */
										int delay;
										t_timer_data data;

										data.p = NULL;
										delay = prefs_v3::initkill_timer();
										if (delay) timerlist_add_timer(c, std::time(NULL) + delay, conn_shutdown, data);
				}
				default:
					/* We have to wait for an initial "magic" byte on bnet connections to
						 * tell us exactly what connection class we are dealing with.
						 */
					break;
				}
			}

			return 0;
		}


		static int sd_accept(t_addr const * curr_laddr, t_laddr_info const * laddr_info, int ssocket, int usocket)
		{
			char               tempa[32];
			int                csocket;
			struct sockaddr_in caddr;
			socklen_t          caddr_len;

			if (!addr_get_addr_str(curr_laddr, tempa, sizeof(tempa)))
				std::strcpy(tempa, "x.x.x.x:x");

			/* accept the connection */
			std::memset(&caddr, 0, sizeof(caddr));
			caddr_len = sizeof(caddr);
			if ((csocket = accept(ssocket, (struct sockaddr *)&caddr, &caddr_len)) < 0)
			{
				/* BSD, POSIX error for aborted connections, SYSV often uses EAGAIN or EPROTO */
				if (
	#ifdef EWOULDBLOCK
					errno == EWOULDBLOCK ||
	#endif
	#ifdef ECONNABORTED
					errno == ECONNABORTED ||
	#endif
	#ifdef EPROTO
					errno == EPROTO ||
	#endif
					0)
					eventlog(eventlog_level_error, __FUNCTION__, "client aborted connection on {} (accept: {})", tempa, pstrerror(errno));
				else /* EAGAIN can mean out of resources _or_ connection aborted :( */
				if (
	#ifdef EINTR
					errno != EINTR &&
	#endif
					1)
					eventlog(eventlog_level_error, __FUNCTION__, "could not accept new connection on {} (accept: {})", tempa, pstrerror(errno));
				return -1;
			}

			return sd_finalize_accepted(curr_laddr, laddr_info, csocket, caddr);
		}


		static int sd_udpinput(t_addr * const curr_laddr, t_laddr_info const * laddr_info, int ssocket, int usocket)
		{
			int             err;
			socklen_t       errlen;
			t_packet *      upacket;
	
			err = 0;
			errlen = sizeof(err);
			if (getsockopt(usocket, SOL_SOCKET, SO_ERROR, &err, &errlen) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] unable to read socket error (getsockopt: {})", usocket, pstrerror(errno));
				return -1;
			}
			if (errlen && err) /* if it was an error, there is no packet to read */
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] async UDP socket error notification (psock_getsockopt: {})", usocket, pstrerror(err));
				return -1;
			}
			if (!(upacket = packet_create(packet_class_udp)))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not allocate raw packet for input");
				return -1;
			}

			{
				struct sockaddr_in fromaddr;
				socklen_t          fromlen;
				int                len;
	
				fromlen = sizeof(fromaddr);
				if ((len = recvfrom(usocket, packet_get_raw_data_build(upacket, 0), MAX_PACKET_SIZE, 0, (struct sockaddr *)&fromaddr, &fromlen)) < 0)
				{
					if (
	#ifdef EINTR
						errno != EINTR &&
	#endif
	#ifdef EAGAIN
						errno != EAGAIN &&
	#endif
	#ifdef EWOULDBLOCK
						errno != EWOULDBLOCK &&
	#endif
	#ifdef ECONNRESET
						errno != ECONNRESET &&	/* this is a win2k/winxp issue
									 * their socket implementation returns this value
									 * although it shouldn't
									 */
	#endif
									 1)
									 eventlog(eventlog_level_error, __FUNCTION__, "could not recv UDP datagram (recvfrom: {})", pstrerror(errno));
					packet_del_ref(upacket);
					return -1;
				}

				if (fromaddr.sin_family != AF_INET)
				{
					eventlog(eventlog_level_error, __FUNCTION__, "got UDP datagram with bad address family {}", (int)fromaddr.sin_family);
					packet_del_ref(upacket);
					return -1;
				}

				packet_set_size(upacket, len);

				if (hexstrm)
				{
					char tempa[32];

					if (!addr_get_addr_str(curr_laddr, tempa, sizeof(tempa)))
						std::strcpy(tempa, "x.x.x.x:x");
					std::fprintf(hexstrm, "%d: recv class=%s[0x%02x] type=%s[0x%04x] from=%s to=%s length=%u\n",
						usocket,
						packet_get_class_str(upacket), (unsigned int)packet_get_class(upacket),
						packet_get_type_str(upacket, packet_dir_from_client), packet_get_type(upacket),
						addr_num_to_addr_str(ntohl(fromaddr.sin_addr.s_addr), ntohs(fromaddr.sin_port)),
						tempa,
						packet_get_size(upacket));
					hexdump(hexstrm, packet_get_raw_data(upacket, 0), packet_get_size(upacket));
				}

				handle_udp_packet(usocket, ntohl(fromaddr.sin_addr.s_addr), ntohs(fromaddr.sin_port), upacket);
				packet_del_ref(upacket);
			}

			return 0;
		}


		static int sd_tcpinput(t_connection * c)
		{
			unsigned int currsize;
			t_packet *   packet;
			int		 csocket = conn_get_socket(c);
			bool	 skip;

			currsize = conn_get_in_size(c);

			if (!conn_get_in_queue(c))
			{
				switch (conn_get_class(c))
				{
				case conn_class_init:
					if (!(packet = packet_create(packet_class_init)))
					{
						eventlog(eventlog_level_error, __FUNCTION__, "could not allocate init packet for input");
						return -1;
					}
					break;
				case conn_class_d2cs_bnetd:
					if (!(packet = packet_create(packet_class_d2cs_bnetd)))
					{
						eventlog(eventlog_level_error, __FUNCTION__, "could not allocate d2cs_bnetd packet");
						return -1;
					}
					break;
				case conn_class_bnet:
					if (!(packet = packet_create(packet_class_bnet)))
					{
						eventlog(eventlog_level_error, __FUNCTION__, "could not allocate bnet packet for input");
						return -1;
					}
					break;
				case conn_class_file:
					switch (conn_get_state(c)) {
					case conn_state_pending_raw:
						if (!(packet = packet_create(packet_class_raw)))
						{
							eventlog(eventlog_level_error, __FUNCTION__, "could not allocate raw packet for input");
							return -1;
						}
						packet_set_size(packet, sizeof(t_client_file_req3));
						break;
					default:
						if (!(packet = packet_create(packet_class_file)))
						{
							eventlog(eventlog_level_error, __FUNCTION__, "could not allocate file packet for input");
							return -1;
						}
						break;
					}
					break;
				case conn_class_bot:
				case conn_class_ircinit:
				case conn_class_irc:
				case conn_class_wol:
				case conn_class_wserv:
				case conn_class_apireg:
				case conn_class_wladder:
				case conn_class_telnet:
					if (!(packet = packet_create(packet_class_raw)))
					{
						eventlog(eventlog_level_error, __FUNCTION__, "could not allocate raw packet for input");
						return -1;
					}
					packet_set_size(packet, 1); /* start by only reading one char */
					break;
				case conn_class_w3route:
					if (!(packet = packet_create(packet_class_w3route)))
					{
						eventlog(eventlog_level_error, __FUNCTION__, "could not allocate w3route packet for input");
						return -1;
					}
					break;
				case conn_class_wgameres:
					if (!(packet = packet_create(packet_class_wolgameres)))
					{
						eventlog(eventlog_level_error, __FUNCTION__, "could not allocate wolgameres packet for input");
						return -1;
					}
					break;
				default:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] connection has bad class (closing connection)", conn_get_socket(c));
					conn_close_read(c);
					return -2;
				}
				conn_put_in_queue(c, packet);
				currsize = 0;
			}

			packet = conn_get_in_queue(c);
			switch (net_recv_packet(csocket, packet, &currsize))
			{
			case -1:
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] read returned -1 (closing connection)", conn_get_socket(c));
				conn_close_read(c);
				return -2;

			case 0: /* still working on it */
				/* eventlog(eventlog_level_debug,__FUNCTION__,"[{}] still reading \"{}\" packet ({} of {} bytes so far)",conn_get_socket(c),packet_get_class_str(packet),conn_get_in_size(c),packet_get_size(packet)); */
				conn_set_in_size(c, currsize);
				break;

			case 1: /* done reading */
				switch (conn_get_class(c))
				{
				case conn_class_bot:
				case conn_class_telnet:
					if (currsize < MAX_PACKET_SIZE) /* if we overflow, we can't wait for the end of the line.
									 handle_*_packet() should take care of it */
					{
						char const * const temp = (char const * const)packet_get_raw_data_const(packet, 0);

						if ((temp[currsize - 1] == '\003') || (temp[currsize - 1] == '\004')) {
							/* we have to ignore these special characters, since
							 * some bots even send them after login (eg. UltimateBot)
							 */
							currsize--;
							skip = true;
							break;
						}

						if (temp[currsize - 1] != '\r' && temp[currsize - 1] != '\n')
						{
							conn_set_in_size(c, currsize);
							packet_set_size(packet, currsize + 1);
							skip = true;
							break; /* no end of line, get another char */
						}
					}
					skip = false;
					break;

				case conn_class_ircinit:
				case conn_class_irc:
				case conn_class_wol:
				case conn_class_wserv:
				case conn_class_wladder:
					if (currsize < MAX_PACKET_SIZE)
						/* if we overflow, we can't wait for the end of the line.
								 handle_*_packet() should take care of it */
					{
						char const * const temp = (char const * const)packet_get_raw_data_const(packet, 0);

						if ((temp[currsize - 1] == '\r') || (temp[currsize - 1] == '\0')) {
							/* kindly ignore \r and NUL ... */
							currsize--;
							skip = true;
							break;
						}

						if (temp[currsize - 1] != '\n')
						{
							conn_set_in_size(c, currsize);
							packet_set_size(packet, currsize + 1);
							skip = true;
							break; /* no end of line, get another char */
						}
					}
					skip = false;
					break;

				default:
					skip = false;
				}

				if (!skip) {
					conn_put_in_queue(c, NULL);

					if (hexstrm)
					{
						std::fprintf(hexstrm, "%d: recv class=%s[0x%02x] type=%s[0x%04x] length=%u\n",
							csocket,
							packet_get_class_str(packet), (unsigned int)packet_get_class(packet),
							packet_get_type_str(packet, packet_dir_from_client), packet_get_type(packet),
							packet_get_size(packet));
						hexdump(hexstrm, packet_get_raw_data_const(packet, 0), packet_get_size(packet));
					}

					if (conn_get_class(c) == conn_class_bot ||
						conn_get_class(c) == conn_class_telnet) /* NUL terminate the line to make life easier */
					{
						char * const temp = (char*)packet_get_raw_data(packet, 0);

						if (temp[currsize - 1] == '\r' || temp[currsize - 1] == '\n')
							temp[currsize - 1] = '\0'; /* have to do it here instead of above so everything
										is intact for the hexdump */
					}

					{
						int ret;

						switch (conn_get_class(c))
						{
						case conn_class_init:
							ret = handle_init_packet(c, packet);
							break;
						case conn_class_bnet:
							ret = handle_bnet_packet(c, packet);
							break;
						case conn_class_d2cs_bnetd:
							ret = handle_d2cs_packet(c, packet);
							break;
						case conn_class_bot:
							ret = handle_bot_packet(c, packet);
							break;
						case conn_class_telnet:
							ret = handle_telnet_packet(c, packet);
							break;
						case conn_class_file:
							// TODO(Phase3): handled by BnftpFsm — remove this call
							ret = 0;
							break;
						case conn_class_ircinit:
						case conn_class_irc:
						case conn_class_wol:
						case conn_class_wserv:
						case conn_class_wladder:
							ret = handle_irc_common_packet(c, packet);
							break;
						case conn_class_apireg:
							ret = handle_apireg_packet(c, packet);
							break;
						case conn_class_w3route:
							ret = handle_w3route_packet(c, packet);
							break;
						case conn_class_wgameres:
							// TODO(Phase3): handled by WolFsm — remove this call
							ret = 0;
							break;
						default:
							eventlog(eventlog_level_error, __FUNCTION__, "[{}] bad packet class {} (closing connection)", conn_get_socket(c), (int)packet_get_class(packet));
							ret = -1;
						}
						packet_del_ref(packet);
						if (ret < 0)
						{
							conn_close_read(c);
							return -2;
						}
					}

					conn_set_in_size(c, 0);
				}
			}

			return 0;
		}


		static int sd_tcpoutput(t_connection * c)
		{
			unsigned int currsize;
			unsigned int totsize;
			t_packet *   packet;
			int		 csocket = conn_get_socket(c);

			totsize = 0;
			for (;;)
			{
				currsize = conn_get_out_size(c);

				if ((packet = conn_peek_outqueue(c)) == NULL)
					return -2;

				switch (net_send_packet(csocket, packet, &currsize)) /* avoid warning */
				{
				case -1:
					/* marking connection as "destroyed", memory will be freed later */
					conn_clear_outqueue(c);
					conn_set_state(c, conn_state_destroy);
					return -2;

				case 0: /* still working on it */
					conn_set_out_size(c, currsize);
					return 0; /* bail out */

				case 1: /* done sending */
					if (hexstrm)
					{
						std::fprintf(hexstrm, "%d: send class=%s[0x%02x] type=%s[0x%04x] length=%u\n",
							csocket,
							packet_get_class_str(packet), (unsigned int)packet_get_class(packet),
							packet_get_type_str(packet, packet_dir_from_server), packet_get_type(packet),
							packet_get_size(packet));
						hexdump(hexstrm, packet_get_raw_data(packet, 0), packet_get_size(packet));
					}

					packet = conn_pull_outqueue(c);
					packet_del_ref(packet);
					conn_set_out_size(c, 0);

					/* stop at about BNETD_MAX_OUTBURST (or until out of packets or EWOULDBLOCK) */
					if (totsize > BNETD_MAX_OUTBURST || !conn_peek_outqueue(c))
						return 0;
					totsize += currsize;
					break;
				}
			}

			/* not reached */
		}

		void server_check_and_fix_hostname(char const * sname)
		{
			int ok = 1;
			char * tn = (char *)sname;
			char * sn = (char *)sname;

			if (!std::isalnum((int)*sn))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "hostname contains invalid first symbol (must be alphanumeric)");
				*tn = 'a';
			}
			tn++;
			sn++;

			for (; *sn != '\0'; sn++)
			{
				if (std::isalnum((int)*sn) || *sn == '-' || *sn == '.')
				{
					*tn = *sn;
					tn++;
				}
				else
					ok = 0;
			}
			*tn = '\0';

			if (!ok)
				eventlog(eventlog_level_error, __FUNCTION__, "hostname contains invalid symbol(s) (must be alphanumeric, '-' or '.') - skipped those symbols");
		}

		extern void server_set_hostname(void)
		{
			char temp[250];
			char const * hn;
#ifdef HAVE_GETHOSTBYNAME
			struct hostent *   hp;
#endif

			if (server_hostname) {
				delete[] const_cast<char*>(server_hostname); /* avoid warning */
				server_hostname = NULL;
			}

			hn = prefs_v3::hostname();
			if ((!hn) || (hn[0] == '\0')) {
				if (gethostname(temp, sizeof(temp)) < 0) {
#ifdef WIN32
					std::sprintf(temp, "localhost");
#else
					eventlog(eventlog_level_error, __FUNCTION__, "could not get hostname: {}", pstrerror(errno));
					return;
#endif
				}
#ifdef HAVE_GETHOSTBYNAME
				hp = gethostbyname(temp);
				if (!hp || !hp->h_name) {
#endif
					server_hostname = sv_strdup(temp);
#ifdef HAVE_GETHOSTBYNAME
				}
				else {
					int i = 0;

					if (std::strchr(hp->h_name, '.'))
						/* Default name is already a FQDN */
						server_hostname = sv_strdup(hp->h_name);
					/* ... if not we have to examine the aliases */
					while (!server_hostname && hp->h_aliases && hp->h_aliases[i]) {
						if (std::strchr(hp->h_aliases[i], '.'))
							server_hostname = sv_strdup(hp->h_aliases[i]);
						i++;
					}
					if (!server_hostname)
						/* Fall back to default name which might not be a FQDN */
						server_hostname = sv_strdup(hp->h_name);
				}
#endif
			}
			else {
				server_hostname = sv_strdup(hn);
			}
			server_check_and_fix_hostname(server_hostname);
			eventlog(eventlog_level_info, __FUNCTION__, "set hostname to \"{}\"", server_hostname);
		}

		extern char const * server_get_hostname(void)
		{
			if (!server_hostname)
				return "(null)"; /* avoid crashes */
			else
				return server_hostname;
		}


		extern void server_clear_hostname(void)
		{
			if (server_hostname) {
				delete[] const_cast<char*>(server_hostname); /* avoid warning */
				server_hostname = NULL;
			}
		}


		static int handle_accept(void *data, t_fdwatch_type rw)
		{
			t_laddr_info *laddr_info = (t_laddr_info*)addr_get_data((t_addr *)data).p;

			return sd_accept((t_addr *)data, laddr_info, laddr_info->ssocket, laddr_info->usocket);
		}


		static int handle_udp(void *data, t_fdwatch_type rw)
		{
			t_laddr_info *laddr_info = (t_laddr_info*)addr_get_data((t_addr *)data).p;

			return sd_udpinput((t_addr *)data, laddr_info, laddr_info->ssocket, laddr_info->usocket);
		}


		static int handle_tcp(void *data, t_fdwatch_type rw)
		{
			switch (rw) {
			case fdwatch_type_read: return sd_tcpinput((t_connection *)data);
			case fdwatch_type_write: return sd_tcpoutput((t_connection *)data);
			default:
				return -1;
			}
		}


		static int _setup_add_addrs(t_addrlist **pladdrs, const char *str, unsigned int defaddr, unsigned short defport, t_laddr_type type)
		{
			t_addr *        curr_laddr;
			t_addr_data     laddr_data;
			t_laddr_info *  laddr_info;
			t_elem const *  acurr;

			if (*pladdrs == NULL) {
				*pladdrs = addrlist_create(str, defaddr, defport);
				if (*pladdrs == NULL) return -1;
			}
			else if (addrlist_append(*pladdrs, str, defaddr, defport)) return -1;

			/* Mark all these laddrs for being classic Battle.net service */
			LIST_TRAVERSE_CONST(*pladdrs, acurr)
			{
				curr_laddr = (t_addr*)elem_get_data(acurr);
				if (addr_get_data(curr_laddr).p)
					continue;
				laddr_info = new t_laddr_info{};
				laddr_info->usocket = -1;
				laddr_info->ssocket = -1;
				laddr_info->type = type;
				laddr_data.p = laddr_info;
				if (addr_set_data(curr_laddr, laddr_data) < 0)
				{
					eventlog(eventlog_level_error, __FUNCTION__, "could not set address data");
					if (laddr_info->usocket != -1)
					{
						close(laddr_info->usocket);
						laddr_info->usocket = -1;
					}
					if (laddr_info->ssocket != -1)
					{
						close(laddr_info->ssocket);
						laddr_info->ssocket = -1;
					}
					return -1;
				}
			}

			return 0;
		}

		static int _set_reuseaddr(int sock)
		{
			int val = 1;

			return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, (socklen_t)sizeof(int));
		}

		static int _bind_socket(int sock, unsigned addr, short port)
		{
			struct sockaddr_in saddr;

			std::memset(&saddr, 0, sizeof(saddr));
			saddr.sin_family = AF_INET;
			saddr.sin_port = htons(port);
			saddr.sin_addr.s_addr = htonl(addr);
			return bind(sock, (struct sockaddr *)&saddr, (socklen_t)sizeof(saddr));
		}

		static int _setup_listensock(t_addrlist *laddrs)
		{
			t_addr *        curr_laddr;
			t_laddr_info *  laddr_info;
			t_elem const *  acurr;
			char            tempa[32];
			int		    fidx;

			LIST_TRAVERSE_CONST(laddrs, acurr)
			{
				curr_laddr = (t_addr*)elem_get_data(acurr);
				if (!(laddr_info = (t_laddr_info*)addr_get_data(curr_laddr).p))
				{
					eventlog(eventlog_level_error, __FUNCTION__, "NULL address info");
					goto err;
				}

				if (!addr_get_addr_str(curr_laddr, tempa, sizeof(tempa)))
					std::strcpy(tempa, "x.x.x.x:x");

				laddr_info->ssocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (laddr_info->ssocket < 0)
				{
					eventlog(eventlog_level_error, __FUNCTION__, "could not create a {} listening socket (socket: {})", laddr_type_get_str(laddr_info->type), pstrerror(errno));
					goto err;
				}
	
				if (_set_reuseaddr(laddr_info->ssocket) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not set option SO_REUSEADDR on {} socket {} (setsockopt: {})", laddr_type_get_str(laddr_info->type), laddr_info->ssocket, pstrerror(errno));
				/* not a fatal error... */
	
				if (_bind_socket(laddr_info->ssocket, addr_get_ip(curr_laddr), addr_get_port(curr_laddr)) < 0) {
					eventlog(eventlog_level_error, __FUNCTION__, "could not bind {} socket to address {} TCP (bind: {})", laddr_type_get_str(laddr_info->type), tempa, pstrerror(errno));
					goto errsock;
				}
	
				/* tell socket to listen for connections */
				if (listen(laddr_info->ssocket, LISTEN_QUEUE) < 0) {
					eventlog(eventlog_level_error, __FUNCTION__, "could not set {} socket {} to listen (listen: {})", laddr_type_get_str(laddr_info->type), laddr_info->ssocket, pstrerror(errno));
					goto errsock;
				}
	
				if (fcntl(laddr_info->ssocket, F_SETFL, O_NONBLOCK) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not set {} TCP listen socket to non-blocking mode (fcntl: {})", laddr_type_get_str(laddr_info->type), pstrerror(errno));

				/* v3 strangler-fig: for bnet TCP listeners, optionally
				 * skip the legacy fdwatch registration so the v3
				 * TcpAcceptor can adopt the listening fd. Other
				 * listener types always use the legacy accept loop. */
				const bool skip_tcp_for_this = (skip_legacy_tcp_fdwatch
					&& laddr_info->type == laddr_type_bnet);
				fidx = -1;
				if (!skip_tcp_for_this) {
					/* index not stored persisently because we dont need to refer to it later */
					fidx = fdwatch_add_fd(laddr_info->ssocket, fdwatch_type_read, handle_accept, curr_laddr);
					if (fidx < 0) {
						eventlog(eventlog_level_error, __FUNCTION__, "could not add listening socket {} to fdwatch pool (max sockets?)", laddr_info->ssocket);
						goto errsock;
					}
				} else {
					eventlog(eventlog_level_info, __FUNCTION__, "{} TCP fd {} reserved for v3 TcpAcceptor (skipped fdwatch)", laddr_type_get_str(laddr_info->type), laddr_info->ssocket);
				}

				eventlog(eventlog_level_info, __FUNCTION__, "listening for {} connections on {} TCP", laddr_type_get_str(laddr_info->type), tempa);

				if (laddr_info->type == laddr_type_bnet)
				{
					laddr_info->usocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
					if (laddr_info->usocket < 0)
					{
						eventlog(eventlog_level_error, __FUNCTION__, "could not create UDP socket (socket: {})", pstrerror(errno));
						goto errfdw;
					}
	
					if (_set_reuseaddr(laddr_info->usocket) < 0)
						eventlog(eventlog_level_error, __FUNCTION__, "could not set option SO_REUSEADDR on {} socket {} (setsockopt: {})", laddr_type_get_str(laddr_info->type), laddr_info->usocket, pstrerror(errno));
					/* not a fatal error... */
	
					if (_bind_socket(laddr_info->usocket, addr_get_ip(curr_laddr), addr_get_port(curr_laddr)) < 0) {
						eventlog(eventlog_level_error, __FUNCTION__, "could not bind {} socket to address {} UDP (bind: {})", laddr_type_get_str(laddr_info->type), tempa, pstrerror(errno));
						goto errusock;
					}
	
					if (fcntl(laddr_info->usocket, F_SETFL, O_NONBLOCK) < 0)
						eventlog(eventlog_level_error, __FUNCTION__, "could not set {} UDP socket to non-blocking mode (fcntl: {})", laddr_type_get_str(laddr_info->type), pstrerror(errno));

					if (skip_legacy_udp_fdwatch) {
						/* v3 strangler-fig: hand the fd to the v3
						 * UdpEndpoint instead of registering with
						 * the legacy fdwatch loop. */
						bnet_udp_fds.push_back(laddr_info->usocket);
						eventlog(eventlog_level_info, __FUNCTION__, "{} UDP fd {} reserved for v3 UdpEndpoint (skipped fdwatch)", laddr_type_get_str(laddr_info->type), laddr_info->usocket);
					} else {
						/* index ignored because we never need it after this */
						if (fdwatch_add_fd(laddr_info->usocket, fdwatch_type_read, handle_udp, curr_laddr) < 0) {
							eventlog(eventlog_level_error, __FUNCTION__, "could not add listening socket {} to fdwatch pool (max sockets?)", laddr_info->usocket);
							goto errusock;
						}
					}
				}

				/* v3 strangler-fig: record per-listener metadata for
				 * the TcpBridge whenever bnet TCP fdwatch is skipped. */
				if (skip_tcp_for_this) {
					bnet_tcp_listener_info info;
					info.ssocket       = laddr_info->ssocket;
					info.usocket       = (laddr_info->type == laddr_type_bnet)
					                       ? laddr_info->usocket : -1;
					info.laddr_ip      = addr_get_ip(curr_laddr);
					info.laddr_port    = addr_get_port(curr_laddr);
					info.type          = static_cast<int>(laddr_info->type);
					info.opaque_laddr  = const_cast<t_addr *>(curr_laddr);
					bnet_tcp_listeners.push_back(info);
				}
			}

			return 0;

		errusock:
			close(laddr_info->usocket);
			laddr_info->usocket = -1;

		errfdw:
			if (fidx >= 0) fdwatch_del_fd(fidx);

		errsock:
			close(laddr_info->ssocket);
			laddr_info->ssocket = -1;

		err:
			return -1;
		}

#ifdef DO_POSIXSIG
		static sigset_t        block_set;
		static sigset_t        save_set;

		static int _setup_posixsig(void)
		{
			if (sigemptyset(&save_set) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not initialize std::signal set (sigemptyset: {})", pstrerror(errno));
				return -1;
			}
			if (sigemptyset(&block_set) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not initialize std::signal set (sigemptyset: {})", pstrerror(errno));
				return -1;
			}
			if (sigaddset(&block_set, SIGINT) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not add std::signal to set (sigemptyset: {})", pstrerror(errno));
				return -1;
			}
			if (sigaddset(&block_set, SIGHUP) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not add std::signal to set (sigemptyset: {})", pstrerror(errno));
				return -1;
			}
			if (sigaddset(&block_set, SIGTERM) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not add std::signal to set (sigemptyset: {})", pstrerror(errno));
				return -1;
			}
			if (sigaddset(&block_set, SIGUSR1) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not add std::signal to set (sigemptyset: {})", pstrerror(errno));
				return -1;
			}
			if (sigaddset(&block_set, SIGUSR2) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not add std::signal to set (sigemptyset: {})", pstrerror(errno));
				return -1;
			}
			if (sigaddset(&block_set, SIGALRM) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not add std::signal to set (sigemptyset: {})", pstrerror(errno));
				return -1;
			}

			{
				struct sigaction quit_action;
				struct sigaction restart_action;
				struct sigaction save_action;
				struct sigaction pipe_action;
#ifdef	HAVE_SETITIMER
				struct sigaction timer_action;
#endif
				struct sigaction forced_quit_action;

				quit_action.sa_handler = quit_sig_handle;
				if (sigemptyset(&quit_action.sa_mask) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not initialize std::signal set (sigemptyset: {})", pstrerror(errno));
				quit_action.sa_flags = SA_RESTART;

				restart_action.sa_handler = restart_sig_handle;
				if (sigemptyset(&restart_action.sa_mask) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not initialize std::signal set (sigemptyset: {})", pstrerror(errno));
				restart_action.sa_flags = SA_RESTART;

				save_action.sa_handler = save_sig_handle;
				if (sigemptyset(&save_action.sa_mask) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not initialize std::signal set (sigemptyset: {})", pstrerror(errno));
				save_action.sa_flags = SA_RESTART;

				pipe_action.sa_handler = pipe_sig_handle;
				if (sigemptyset(&pipe_action.sa_mask) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not initialize std::signal set (sigemptyset: {})", pstrerror(errno));
				pipe_action.sa_flags = SA_RESTART;
#ifdef HAVE_SETITIMER
				timer_action.sa_handler = timer_sig_handle;
				if (sigemptyset(&timer_action.sa_mask) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not initialize std::signal set (sigemptyset: {})", pstrerror(errno));
				timer_action.sa_flags = SA_RESTART;
#endif /* HAVE_SETITIMER */
				forced_quit_action.sa_handler = forced_quit_sig_handle;
				if (sigemptyset(&forced_quit_action.sa_mask) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not initialize std::signal set (sigemptyset: {})", pstrerror(errno));
				forced_quit_action.sa_flags = SA_RESTART;

				if (sigaction(SIGINT, &quit_action, NULL) < 0) /* control-c */
					eventlog(eventlog_level_error, __FUNCTION__, "could not set SIGINT std::signal handler (sigaction: {})", pstrerror(errno));
				if (sigaction(SIGHUP, &restart_action, NULL) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not set SIGHUP std::signal handler (sigaction: {})", pstrerror(errno));
				if (sigaction(SIGTERM, &quit_action, NULL) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not set SIGTERM std::signal handler (sigaction: {})", pstrerror(errno));
				if (sigaction(SIGUSR1, &save_action, NULL) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not set SIGUSR1 std::signal handler (sigaction: {})", pstrerror(errno));
				if (sigaction(SIGPIPE, &pipe_action, NULL) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not set SIGPIPE std::signal handler (sigaction: {})", pstrerror(errno));
#ifdef HAVE_SETITIMER
				/* setup asynchronus timestamp update timer */
				if (sigaction(SIGALRM, &timer_action, NULL) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not set SIGALRM std::signal handler (sigaction: {})", pstrerror(errno));

				{
					struct itimerval it;

					it.it_interval.tv_sec = BNETD_JIFFIES / 1000;
					it.it_interval.tv_usec = BNETD_JIFFIES % 1000;
					it.it_value.tv_sec = BNETD_JIFFIES / 1000;
					it.it_value.tv_usec = BNETD_JIFFIES % 1000;

					if (setitimer(ITIMER_REAL, &it, NULL)) {
						eventlog(eventlog_level_fatal, __FUNCTION__, "failed to set timers (setitimer(): {})", pstrerror(errno));
						return -1;
					}
				}
#endif
				if (sigaction(SIGQUIT, &forced_quit_action, NULL) < 0) /* immediate shutdown */
					eventlog(eventlog_level_error, __FUNCTION__, "could not set SIGQUIT std::signal handler (sigaction: {})", pstrerror(errno));
			}

			return 0;
		}
#endif

#ifdef WIN32
# ifndef WIN32_GUI
		BOOL CtrlHandler(DWORD fdwCtrlType)
		{
			switch (fdwCtrlType)
			{
				// Handle the CTRL-C signal.
			case CTRL_C_EVENT:
				server_quit_wraper();
				return(TRUE);

				// CTRL-CLOSE: confirm that the user wants to exit.
				server_quit_wraper();
				return(TRUE);

				// Pass other signals to the next handler.
			case CTRL_BREAK_EVENT:
				server_quit_wraper();
				return FALSE;

			case CTRL_LOGOFF_EVENT:
				server_quit_wraper();
				return FALSE;

			case CTRL_SHUTDOWN_EVENT:
				server_quit_wraper();
				return FALSE;

			default:
				return FALSE;
			}
		}
# endif
#endif

		static std::time_t prev_exittime;

		static void _server_mainloop(t_addrlist *laddrs)
		{
			std::time_t          next_savetime, track_time;
			std::time_t          war3_ladder_updatetime;
			std::time_t          output_updatetime;
			std::time_t prev_time = 0;

			starttime = std::time(NULL);
			track_time = starttime - prefs_v3::track();
			next_savetime = starttime + prefs_v3::user_sync_timer();
			war3_ladder_updatetime = starttime - prefs_v3::war3_ladder_update_secs();
			output_updatetime = starttime - prefs_v3::output_update_secs();

			/* v3 strangler-fig (38g): open the main-loop post queue
			 * so `server_post_to_main` from worker threads is
			 * accepted. Closed below when the loop exits. */
			{
				std::lock_guard<std::mutex> g{pending_main_mu};
				pending_main_active = true;
			}

			for (;;)
			{
#ifdef WIN32
				if (g_ServiceStatus == 0) server_quit_wraper();

				while (g_ServiceStatus == 2) Sleep(1000);
#endif

#ifdef DO_POSIXSIG
				if (sigprocmask(SIG_SETMASK, &save_set, NULL) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not unblock signals");
				/* receive signals here */
				if (sigprocmask(SIG_SETMASK, &block_set, NULL) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not block signals");
#endif
				if (got_epipe)
				{
					got_epipe = 0;
					eventlog(eventlog_level_info, __FUNCTION__, "handled SIGPIPE");
				}

#if !defined(DO_POSIXSIG) || !defined(HAVE_SETITIMER)
				/* if no DO_POSIXSIG or no HAVE_SETITIMER so we must read timestamp each loop */
				std::time(&now);
#endif
				curr_exittime = sigexittime;
				if (curr_exittime && (curr_exittime <= now || connlist_login_get_length() < 1))
				{
					eventlog(eventlog_level_info, __FUNCTION__, "the server is shutting down ({} connections left)", connlist_get_length());
					clanlist_save();
					/* no need for accountlist_save() when using "force" */
					accountlist_save(FS_FORCE | FS_ALL);
					accountlist_flush(FS_FORCE | FS_ALL);
					break;
				}
				if (prev_exittime != curr_exittime)
				{
					t_message *  message;
					char const * tstr;

					if (curr_exittime != 0)
					{
						char text[29 + 256 + 2 + 32 + 24 + 1]; /* "The ... in " + std::time + " (" num + " connection ...)." + NUL */

						tstr = seconds_to_timestr(curr_exittime - now);
						std::sprintf(text, "The server will shut down in %s (%d connections remaining).", tstr, connlist_get_length());
						if ((message = message_create(message_type_error, NULL, text)))
						{
							message_send_all(message);
							message_destroy(message);
						}
						eventlog(eventlog_level_info, __FUNCTION__, "the server will shut down in {} ({} connections remaining)", tstr, connlist_get_length());
					}
					else
					{
						if ((message = message_create(message_type_error, NULL, "Server shutdown has been canceled.")))
						{
							message_send_all(message);
							message_destroy(message);
						}
						eventlog(eventlog_level_info, __FUNCTION__, "server shutdown canceled");
					}
				}
				prev_exittime = curr_exittime;

				if (next_savetime <= now) {
					/* do this stuff in usersync periods */
					clanlist_save();
					gamelist_check_voidgame();
					ladders.save();
					next_savetime += prefs_v3::user_sync_timer();
				}
				accountlist_save(FS_NONE);
				accountlist_flush(FS_NONE);

				if (prefs_v3::track() && track_time + (std::time_t)prefs_v3::track() <= now)
				{
					track_time = now;
					tracker_send_report(laddrs);
				}

				if (prefs_v3::war3_ladder_update_secs() && war3_ladder_updatetime + (std::time_t)prefs_v3::war3_ladder_update_secs() <= now)
				{
					war3_ladder_updatetime = now;
					ladders.status();
				}

				if (prefs_v3::output_update_secs() && output_updatetime + (std::time_t)prefs_v3::output_update_secs() <= now)
				{
					output_updatetime = now;
					output_write_to_file();
				}


				if (do_save)
				{
					eventlog(eventlog_level_info, __FUNCTION__, "saving accounts due to std::signal");
					clanlist_save();

					do_save = 0;
				}

				if (do_restart)
				{
					if (do_restart == restart_mode_all)
					{
						eventlog(eventlog_level_info, __FUNCTION__, "reading configuration files");
#ifdef PVPGN_V3_BNETD_INTEGRATION
						// R194: legacy `prefs_load()` was removed (R165). The v3
						// equivalent re-parses the TOML config in place.
						// `cmdline_get_preffile()` returns the .conf path so we
						// translate to the matching .toml in the same directory.
						auto _r194_reload_v3_prefs = [](char const* path_or_null) -> int {
							std::string p = (path_or_null && *path_or_null) ? path_or_null : BNETD_DEFAULT_CONF_FILE;
							auto dot = p.find_last_of('.');
							auto sep = p.find_last_of("/\\");
							if (dot != std::string::npos && (sep == std::string::npos || dot > sep))
								p.replace(dot, std::string::npos, ".toml");
							else
								p += ".toml";
							return pvpgn_v3_prefs_load_toml(p.c_str());
						};
						if (_r194_reload_v3_prefs(cmdline_get_preffile()) != 0)
							eventlog(eventlog_level_error, __FUNCTION__, "could not reload v3 TOML configuration");
#else
						if (cmdline_get_preffile())
						{
							if (prefs_load(cmdline_get_preffile()) < 0)
								eventlog(eventlog_level_error, __FUNCTION__, "could not parse configuration file");
						}
						else
						if (prefs_load(BNETD_DEFAULT_CONF_FILE) < 0)
							eventlog(eventlog_level_error, __FUNCTION__, "using default configuration");
#endif

						if (eventlog_open(prefs_v3::logfile()) < 0)
							eventlog(eventlog_level_error, __FUNCTION__, "could not use the file \"{}\" for the eventlog", prefs_v3::logfile());

						/* FIXME: load new network settings */

						/* reload server name */
						server_set_hostname();

						attrlayer_load_default();
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_i18n)
					{
						i18n_reload();
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_channels)
					{
						channellist_reload();
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_realms)
					{
						if (realmlist_reload(prefs_v3::realmfile()) < 0)
							eventlog(eventlog_level_error, __FUNCTION__, "could not reload realm list");
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_autoupdate)
					{
						autoupdate_unload();
						if (autoupdate_load(prefs_v3::mpqfile()) < 0)
							eventlog(eventlog_level_error, __FUNCTION__, "could not load autoupdate list");
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_news)
					{
						news_unload();
						if (news_load(prefs_v3::newsfile()) < 0)
							eventlog(eventlog_level_error, __FUNCTION__, "could not load news list");
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_versioncheck)
					{
						unload_versioncheck_conf();
						if (!load_versioncheck_conf(prefs_v3::versioncheck_file()))
						{
							eventlog(eventlog_level_error, __FUNCTION__, "could not load versioncheck list");
						}
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_ipbans)
					{
						if (ipbanlist_destroy() < 0)
							eventlog(eventlog_level_error, __FUNCTION__, "could not unload old IP ban list");
						ipbanlist_create();
						if (ipbanlist_load(prefs_v3::ipbanfile()) < 0)
							eventlog(eventlog_level_error, __FUNCTION__, "could not load new IP ban list");
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_helpfile)
					{
						helpfile_unload();
						if (helpfile_init(prefs_v3::helpfile()) < 0)
							eventlog(eventlog_level_error, __FUNCTION__, "could not load the helpfile");
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_banners)
					{
						try
						{
							if (AdBannerList.is_loaded())
							{
								AdBannerList.unload();
							}

							AdBannerList.load(prefs_v3::adfile());
						}
						catch (const std::exception& e)
						{
							eventlog(eventlog_level_error, __FUNCTION__, "{}", e.what());
						}
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_tracker)
					{
						if (prefs_v3::track())
							tracker_set_servers(prefs_v3::trackserv_addrs()); // R194: replaced legacy prefs_get_trackserv_addrs()
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_commandgroups)
					{
						if (command_groups_reload(prefs_v3::command_groups_file()) < 0)
							eventlog(eventlog_level_error, __FUNCTION__, "could not load new command_groups list");
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_aliasfile)
					{
						aliasfile_unload();
						aliasfile_load(prefs_v3::aliasfile());
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_transfile)
					{
						if (trans_reload(prefs_v3::transfile(), TRANS_BNETD) < 0)
							eventlog(eventlog_level_error, __FUNCTION__, "could not reload trans list");
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_tournament)
					{
						tournament_reload(prefs_v3::tournament_file());
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_icons)
					{
						customicons_unload();
						customicons_load(prefs_v3::customicons_file());
					}

					if (do_restart == restart_mode_all || do_restart == restart_mode_anongame)
					{
						anongame_infos_unload();
						anongame_infos_load(prefs_v3::anongame_infos_file());
					}

#ifdef WITH_LUA
					if (do_restart == restart_mode_all || do_restart == restart_mode_lua)
					{
						lua_handle_server(luaevent_server_rehash);

						lua_unload();
						lua_load(prefs_v3::scriptdir());
					}
#endif

					eventlog(eventlog_level_info, __FUNCTION__, "done reconfiguring");

					do_restart = 0;
				}

				/* only check timers once a second */
				if (now > prev_time) 
				{
					prev_time = now;
					timerlist_check_timers(now);
#ifdef WITH_LUA
					lua_handle_server(luaevent_server_mainloop);
#endif
				}

				/* v3 strangler-fig (38g): run any work posted by
				 * Asio worker threads (e.g. `teardown_session`)
				 * on this thread before fdwatch polling so it
				 * shares the same single-threaded invariants as
				 * `timerlist_check_timers` / `connlist_reap`. */
				drain_pending_main();

				/* no need to populate the fdwatch structures as they are populated on the fly
				 * by sd_accept, conn_push_outqueue, conn_pull_outqueue, conn_destory */

				/* find which sockets need servicing */
				switch (fdwatch(BNETD_POLL_INTERVAL))
				{
				case -1: /* error */
					if (
	#ifdef EINTR
						errno != EINTR &&
	#endif
						1)
						eventlog(eventlog_level_error, __FUNCTION__, "fdwatch() failed (errno: {})", pstrerror(errno));
				case 0: /* timeout... no sockets need checking */
					continue;
				}

				/* cycle through the ready sockets and handle them */
				fdwatch_handle();
	
				/* v3 strangler-fig (R139): pump the Asio io_context for
				 * up to 5 ms so v3 async work makes progress on every
				 * fdwatch iteration. No-op when PVPGN_V3_BNETD_INTEGRATION
				 * is not defined (legacy-only build). */
				server_tick_v3(5);
	
				/* reap dead connections */
				connlist_reap();

			}

			/* v3 strangler-fig (38g): close the main-loop post
			 * queue. Any callbacks posted after this point will be
			 * dropped on the floor (the process is shutting down
			 * and `_shutdown_conns` is about to walk connlist). */
			{
				std::lock_guard<std::mutex> g{pending_main_mu};
				pending_main_active = false;
				pending_main_q.clear();
			}
		}


		static void _shutdown_conns(void)
		{
			/* Snapshot the list — conn_destroy modifies conn_head */
			std::vector<t_connection*> to_destroy(connlist());
			for (t_connection * c : to_destroy)
				conn_destroy(c, DESTROY_FROM_CONNLIST);
		}


		static void _shutdown_addrs(t_addrlist *laddrs)
		{
			t_addr *        curr_laddr;
			t_laddr_info *  laddr_info;
			t_elem const *  acurr;

			LIST_TRAVERSE_CONST(laddrs, acurr)
			{
				curr_laddr = (t_addr*)elem_get_data(acurr);
				if ((laddr_info = (t_laddr_info*)addr_get_data(curr_laddr).p))
				{
					if (laddr_info->usocket != -1)
						close(laddr_info->usocket);
					if (laddr_info->ssocket != -1)
						close(laddr_info->ssocket);
					delete laddr_info;
				}
			}
			addrlist_destroy(laddrs);
		}

		extern int server_process(void)
		{
			t_addrlist *    laddrs;

#ifdef PVPGN_V3_BNETD_INTEGRATION
			(void)pvpgn_v3_server_dispatch_try(nullptr, "process");
			// `core::ILogger` adapter as the v3 default sink so any
			// application/integration code that calls
			// `pvpgn::core::log(...)` routes through a single seam.
			//
			// Default: `LegacyEventLogger` -- forwards to the
			// existing `eventlog()` pipeline (legacy logfile format
			// + rotation unchanged).
			//
			// Opt-in: set the environment variable
			//   PVPGN_LOG_FORMAT=json
			// to install `infra::log::JsonLineLogger` writing NDJSON
			// to stderr. Useful for piping into jq / logstash /
			// fluent-bit. The legacy `eventlog()` file output is
			// unaffected (it is written from a different code path);
			// this switch only redirects records that flow through
			// the new `core::ILogger` seam (bridges, use-cases).
			{
				const char* fmt = std::getenv("PVPGN_LOG_FORMAT");
				if (fmt != nullptr && std::strcmp(fmt, "json") == 0) {
					pvpgn::core::set_default_logger(
						std::make_shared<
							pvpgn::infra::log::JsonLineLogger>(
								std::cerr));
				} else {
					pvpgn::core::set_default_logger(
						std::make_shared<
							pvpgn::integration::legacy_bnetd::
								LegacyEventLogger>());
				}
			}

			// Composition root (Batch 25a / Batch 26a): route whisper
			// rejection replies through the v3 `IChatReplySink` by
			// default so the user-visible text is owned by an
			// injected `IStringTable` (enabling future translations
			// without touching the legacy `localize()` machinery).
			//
			// Default: ON. Set `PVPGN_V3_CHAT_REPLIES=0` to opt out
			// and fall back to the legacy whisper reply text. The
			// sink + table are leaked intentionally for process
			// lifetime (composition root); when the sink returns
			// `true` from `emit_whisper_reply`, the chat command
			// bridge reports the message handled and the legacy
			// path is short-circuited.
			{
				const char* off =
					std::getenv("PVPGN_V3_CHAT_REPLIES");
				const bool disabled =
					(off != nullptr && std::strcmp(off, "0") == 0);
				if (!disabled) {
					static pvpgn::application::i18n::MapStringTable
						s_tbl;
					pvpgn::integration::legacy_bnetd::
						LegacyChatReplySink::seed_default_strings(
							s_tbl);
					// Optional translations -- harmless if no
					// client requests deDE/ruRU.
					pvpgn::integration::legacy_bnetd::
						LegacyChatReplySink::seed_de_strings(s_tbl);
					pvpgn::integration::legacy_bnetd::
						LegacyChatReplySink::seed_ru_strings(s_tbl);
					static pvpgn::integration::legacy_bnetd::
						LegacyChatReplySink s_sink(s_tbl);
					pvpgn::integration::legacy_bnetd::
						set_chat_reply_sink_override(&s_sink);
				}
			}

			// Composition root (Batch 26c): optionally install a
			// process-singleton `InMemoryEventBus` and subscribe
			// `PasswordRotationObserver` to it. Emits a structured
			// audit-log line every time `must_change_password` flips.
			//
			// Opt-in: `PVPGN_V3_AUDIT_LOG=1`. Off by default because
			// no production emitter publishes onto the bus today --
			// the legacy code path mutates accounts directly and
			// bypasses the aggregate. The wiring is therefore a
			// future-ready seam: when the v3 `LoginUser` /
			// `ChangePassword` use cases start being driven from
			// bnetd, their drained events will land here without
			// further composition-root changes.
			{
				const char* on =
					std::getenv("PVPGN_V3_AUDIT_LOG");
				if (on != nullptr && std::strcmp(on, "1") == 0) {
					static pvpgn::infra::inmemory::InMemoryEventBus
						s_bus;
					static pvpgn::application::auth::
						PasswordRotationObserver
							s_obs(pvpgn::core::default_logger(),
								  s_bus);
					(void)s_obs;  // suppress unused-variable warning
				}
			}

			// Composition root (Batch 30a): optionally install the
			// real v3 ChangePassword handler. When `PVPGN_V3_CHANGEPW=1`
			// the legacy `_client_changepassreq` short-circuits into
			// `ChangePasswordUseCase` via the `BnetSessionHasher`
			// adapter; the use-case verifies the double-hash, rotates
			// the password, and writes the reply packet. The legacy
			// arm only runs when the env var is unset.
			{
				const char* on =
					std::getenv("PVPGN_V3_CHANGEPW");
				if (on != nullptr && std::strcmp(on, "1") == 0) {
					pvpgn::integration::legacy_bnetd::
						install_change_password_handler();
				}
			}

			// Composition root (Batch 30b): optionally install the
			// telemetry-only v3 LoginUser handler. Always returns
			// "not handled" today; the wiring exists so we can
			// measure how often the slot would be exercised before
			// committing to a real implementation that owns the
			// loginreq1 / loginreq2 / loginreqw3 packet layouts.
			{
				const char* on =
					std::getenv("PVPGN_V3_LOGIN");
				if (on != nullptr && std::strcmp(on, "1") == 0) {
					pvpgn::integration::legacy_bnetd::
						install_login_user_handler();
				}
			}

			// Step 4 / E.2: install the legacy-bnetd sink for the
			// v3 `pvpgn_v3_send_packet_try` ABI. Unconditional --
			// the bridge has no observable side effect until a
			// ported handler actually calls the C entry-point, and
			// keeping the install gate-free means later strangler
			// call sites don't each need a separate env var.
			pvpgn::integration::legacy_bnetd::
				install_send_packet_handler();

			// Step 4 / E.3: install the legacy-bnetd apply-side of
			// the byte-1 connection-class dispatch. With this
			// handler registered, `handle_init_packet` delegates
			// the per-class state transitions to v3. Without it
			// the legacy switch in `handle_init.cpp` continues to
			// run as before (the bridge returns 0 and the legacy
			// branches handle the request).
			pvpgn::integration::legacy_bnetd::
				install_init_conn_apply_handler();

			// Step 4 / E.4 (R188.b recovery): install the v3
			// authoritative apply-side handlers for the ads
			// pick/click and realm-list dispatch ABIs. These
			// are *required* under PVPGN_V3_BNETD_INTEGRATION
			// because R189 promoted `_client_adreq`,
			// `_client_adclick2`, `_client_realmlistreq` and
			// `_client_realmlistreq110` to the R168.a mandatory
			// contract -- the `rc < 0` branch in those handlers
			// surfaces a missing installer as a hard error.
			// Without these calls every ad / realm-list request
			// would fail at runtime.
			pvpgn::integration::legacy_bnetd::
				install_ads_handlers();
			pvpgn::integration::legacy_bnetd::
				install_realm_list_handler();
#endif

			laddrs = NULL;
			/* Start with the Battle.net address list */
			if (_setup_add_addrs(&laddrs, prefs_v3::bnetdserv_addrs(), INADDR_ANY, BNETD_SERV_PORT, laddr_type_bnet))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not create {} server address list from \"{}\"", laddr_type_get_str(laddr_type_bnet), prefs_v3::bnetdserv_addrs());
				return -1;
			}

			/* Append list of addresses to listen for IRC connections */
			if (_setup_add_addrs(&laddrs, prefs_v3::irc_addrs(), INADDR_ANY, BNETD_IRC_PORT, laddr_type_irc))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not create {} server address list from \"{}\"", laddr_type_get_str(laddr_type_irc), prefs_v3::irc_addrs());
				_shutdown_addrs(laddrs);
				return -1;
			}

			/* Append list of addresses to listen for WOLv1 connections */
			if (_setup_add_addrs(&laddrs, prefs_v3::wolv1_addrs(), INADDR_ANY, BNETD_WOLV1_PORT, laddr_type_wolv1))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not create {} server address list from \"{}\"", laddr_type_get_str(laddr_type_wolv1), prefs_v3::wolv1_addrs());
				_shutdown_addrs(laddrs);
				return -1;
			}

			/* Append list of addresses to listen for WOLv2 connections */
			if (_setup_add_addrs(&laddrs, prefs_v3::wolv2_addrs(), INADDR_ANY, BNETD_WOLV2_PORT, laddr_type_wolv2))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not create {} server address list from \"{}\"", laddr_type_get_str(laddr_type_wolv2), prefs_v3::wolv2_addrs());
				_shutdown_addrs(laddrs);
				return -1;
			}

			/* Append list of addresses to listen for APIREGISER connections */
			if (_setup_add_addrs(&laddrs, prefs_v3::apireg_addrs(), INADDR_ANY, BNETD_APIREG_PORT, laddr_type_apireg))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not create {} server address list from \"{}\"", laddr_type_get_str(laddr_type_apireg), prefs_v3::apireg_addrs());
				_shutdown_addrs(laddrs);
				return -1;
			}

			/* Append list of addresses to listen for WGAMERES connections */
			if (_setup_add_addrs(&laddrs, prefs_v3::wgameres_addrs(), INADDR_ANY, BNETD_WGAMERES_PORT, laddr_type_wgameres))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not create {} server address list from \"{}\"", laddr_type_get_str(laddr_type_wgameres), prefs_v3::wgameres_addrs());
				_shutdown_addrs(laddrs);
				return -1;
			}

			/* Append list of addresses to listen for W3ROUTE connections */
			if (_setup_add_addrs(&laddrs, prefs_v3::w3route_addr(), INADDR_ANY, BNETD_W3ROUTE_PORT, laddr_type_w3route))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not create {} server address list from \"{}\"", laddr_type_get_str(laddr_type_w3route), prefs_v3::w3route_addr());
				_shutdown_addrs(laddrs);
				return -1;
			}

			/* Append list of addresses to listen for telnet connections */
			if (_setup_add_addrs(&laddrs, prefs_v3::telnet_addrs(), INADDR_ANY, BNETD_TELNET_PORT, laddr_type_telnet))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not create {} server address list from \"{}\"", laddr_type_get_str(laddr_type_telnet), prefs_v3::telnet_addrs());
				_shutdown_addrs(laddrs);
				return -1;
			}

			if (_setup_listensock(laddrs)) {
				_shutdown_addrs(laddrs);
				return -1;
			}

			/* v3 strangler-fig: invoke after-setup hook now that
			 * all listen sockets exist (and bnet_udp_fds is
			 * populated when skip_legacy_udp_fdwatch is on). */
			if (after_setup_hook) after_setup_hook();

			/* setup std::signal handlers */
			prev_exittime = sigexittime;
#ifdef DO_POSIXSIG
			_setup_posixsig();
#endif

#ifdef WIN32
# ifndef WIN32_GUI
			SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
# endif
#endif

			_server_mainloop(laddrs);

#ifdef WIN32
# ifndef WIN32_GUI
			SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, FALSE);
# endif
#endif


			/* cleanup for server shutdown */
			_shutdown_conns();
			if (before_shutdown_hook) before_shutdown_hook();
			_shutdown_addrs(laddrs);

			return 0;
		}

	}

}
