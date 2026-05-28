/*
 * Copyright (C) 2001		sousou	(liupeng.cs@263.net)
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
#include "setup.h"
#include "dbserver.h"

#include <cstring>
#include <ctime>

#ifdef WIN32
# include <conio.h>
#endif

#include "compat/strerror.h"
#ifndef _WIN32
#  include <sys/socket.h>
#  include <sys/select.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
#  include <netinet/in.h>
#endif
#include "common/eventlog.h"
#include "common/addr.h"
#include "common/network.h"
#include "d2ladder.h"
#include "prefs_v3_shim.h"
#include "charlock.h"
#include "dbspacket.h"
#include "handle_signal.h"

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_WS2TCPIP_H
# include <Ws2tcpip.h>
#endif

#include "common/setup_after.h"

#ifdef WIN32
extern int g_ServiceStatus;
#endif

#ifdef PVPGN_V3_D2DBS_INTEGRATION
// R232(2): observation bridges for d2dbs server main + per-conn shutdown.
extern "C" int pvpgn_v3_d2dbs_server_main_try(void) noexcept;
extern "C" int pvpgn_v3_d2dbs_server_shutdown_connection_try(
    int sd,
    unsigned int serverid,
    unsigned int conn_type,
    unsigned int verified) noexcept;
#endif

namespace pvpgn
{

	namespace d2dbs
	{

		static int		dbs_packet_gs_id = 0;
		static t_preset_d2gsid	*preset_d2gsid_head = NULL;
		t_list * dbs_server_connection_list = NULL;
		int dbs_server_listen_socket = -1;

		/* dbs_server_main
		 * The module's driver function -- we just call other functions and
		 * interpret their results.
		 */

		static int dbs_handle_timed_events(void);
		static void dbs_on_exit(void);

		int dbs_server_init(void);
		void dbs_server_loop(int ListeningSocket);
		int dbs_server_setup_fdsets(fd_set * pReadFDs, fd_set * pWriteFDs,
			fd_set * pExceptFDs, int ListeningSocket);
		bool dbs_server_read_data(t_d2dbs_connection* conn);
		bool dbs_server_write_data(t_d2dbs_connection* conn);
		int dbs_server_list_add_socket(int sd, unsigned int ipaddr);
		static int setsockopt_keepalive(int sock);
		static unsigned int get_preset_d2gsid(unsigned int ipaddr);


		int dbs_server_main(void)
		{
#ifdef PVPGN_V3_D2DBS_INTEGRATION
			(void)pvpgn_v3_d2dbs_server_main_try();
#endif
			eventlog(eventlog_level_info, __FUNCTION__, "establishing the listener...");
			dbs_server_listen_socket = dbs_server_init();
			if (dbs_server_listen_socket < 0) {
				eventlog(eventlog_level_error, __FUNCTION__, "dbs_server_init error ");
				return 3;
			}
			eventlog(eventlog_level_info, __FUNCTION__, "waiting for connections...");
			dbs_server_loop(dbs_server_listen_socket);
			dbs_on_exit();
			return 0;
		}

		/* dbs_server_init
		 * Sets up a listener on the given interface and port, returning the
		 * listening socket if successful; if not, returns -1.
		 */
		/* FIXME: No it doesn't!  pcAddress is not ever referenced in this
		 * function.
		 * CreepLord: Fixed much better way (will accept dns hostnames)
		 */
		int dbs_server_init(void)
		{
			int sd;
			struct sockaddr_in sinInterface;
			int val;
			t_addr	* servaddr;

			dbs_server_connection_list = list_create();

			if (d2dbs_d2ladder_init() == -1)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "d2ladder_init() failed");
				return -1;
			}

			if (cl_init(DEFAULT_HASHTBL_LEN, DEFAULT_GS_MAX) == -1)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "cl_init() failed");
				return -1;
			}

			/* psock_init() is a no-op on POSIX */
	
			sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (sd == -1)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "socket() failed : {}", pstrerror(errno));
				return -1;
			}

			val = 1;
			if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "setsockopt() failed : {}", pstrerror(errno));
			}

			if (!(servaddr = addr_create_str(pvpgn::d2dbs::prefs_v3::servaddrs(), INADDR_ANY, DEFAULT_LISTEN_PORT)))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not get servaddr");
				return -1;
			}

			sinInterface.sin_family = AF_INET;
			sinInterface.sin_addr.s_addr = htonl(addr_get_ip(servaddr));
			sinInterface.sin_port = htons(addr_get_port(servaddr));
			if (bind(sd, (struct sockaddr*)&sinInterface, (socklen_t)sizeof(struct sockaddr_in)) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "bind() failed : {}", pstrerror(errno));
				return -1;
			}
			if (listen(sd, LISTEN_QUEUE) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "listen() failed : {}", pstrerror(errno));
				return -1;
			}
			addr_destroy(servaddr);
			return sd;
		}


		/* dbs_server_setup_fdsets
		 * Set up the three FD sets used with select() with the sockets in the
		 * connection list.  Also add one for the listener socket, if we have
		 * one.
		 */

		int dbs_server_setup_fdsets(fd_set * pReadFDs, fd_set * pWriteFDs, fd_set * pExceptFDs, int lsocket)
		{
			t_elem const * elem;
			t_d2dbs_connection* it;
			int highest_fd;

			FD_ZERO(pReadFDs);
			FD_ZERO(pWriteFDs);
			FD_ZERO(pExceptFDs); /* FIXME: don't check these... remove this code */
			/* Add the listener socket to the read and except FD sets, if there is one. */
			if (lsocket >= 0) {
				FD_SET(lsocket, pReadFDs);
				FD_SET(lsocket, pExceptFDs);
			}
			highest_fd = lsocket;

			LIST_TRAVERSE_CONST(dbs_server_connection_list, elem)
			{
				if (!(it = (t_d2dbs_connection*)elem_get_data(elem))) continue;
				if (it->nCharsInReadBuffer < (kBufferSize - kMaxPacketLength)) {
					/* There's space in the read buffer, so pay attention to incoming data. */
					FD_SET(it->sd, pReadFDs);
				}
				if (it->nCharsInWriteBuffer > 0) {
					FD_SET(it->sd, pWriteFDs);
				}
				FD_SET(it->sd, pExceptFDs);
				if (highest_fd < it->sd) highest_fd = it->sd;
			}
			return highest_fd;
		}

		/* dbs_server_read_data
		 * Data came in on a client socket, so read it into the buffer.  Returns
		 * false on failure, or when the client closes its half of the
		 * connection.  (EAGAIN doesn't count as a failure.)
		 */
		bool dbs_server_read_data(t_d2dbs_connection* conn)
		{
			int nBytes;

			nBytes = net_recv(conn->sd, conn->ReadBuf + conn->nCharsInReadBuffer,
				kBufferSize - conn->nCharsInReadBuffer);

			if (nBytes < 0) return false;
			conn->nCharsInReadBuffer += nBytes;
			return true;
		}


		/* dbs_server_write_data
		 * The connection is writable, so send any pending data.  Returns
		 * false on failure.  (EAGAIN doesn't count as a failure.)
		 */
		bool dbs_server_write_data(t_d2dbs_connection* conn)
		{
			int nBytes;

			nBytes = net_send(conn->sd, conn->WriteBuf,
				conn->nCharsInWriteBuffer > kMaxPacketLength ? kMaxPacketLength : conn->nCharsInWriteBuffer);

			if (nBytes < 0) return false;

			conn->nCharsInWriteBuffer -= nBytes;
			if (conn->nCharsInWriteBuffer)
				std::memmove(conn->WriteBuf, conn->WriteBuf + nBytes, conn->nCharsInWriteBuffer);

			return true;
		}

		int dbs_server_list_add_socket(int sd, unsigned int ipaddr)
		{
			t_d2dbs_connection	*it;
			struct in_addr		in;

			it = new t_d2dbs_connection{};
			std::memset(it, 0, sizeof(t_d2dbs_connection));
			it->sd = sd;
			it->ipaddr = ipaddr;
			it->major = 0;
			it->minor = 0;
			it->type = 0;
			it->stats = 0;
			it->verified = 0;
			it->serverid = get_preset_d2gsid(ipaddr);
			it->last_active = std::time(NULL);
			it->nCharsInReadBuffer = 0;
			it->nCharsInWriteBuffer = 0;
			list_append_data(dbs_server_connection_list, it);
			in.s_addr = htonl(ipaddr);
			char addrstr[INET_ADDRSTRLEN] = { 0 };
			inet_ntop(AF_INET, &(in), addrstr, sizeof(addrstr));
			std::strncpy((char*)it->serverip, addrstr, sizeof(it->serverip) - 1);

			return 1;
		}

		static int dbs_handle_timed_events(void)
		{
			static	std::time_t		prev_ladder_save_time = 0;
			static	std::time_t		prev_keepalive_save_time = 0;
			static  std::time_t		prev_timeout_checktime = 0;
			std::time_t			now;

			now = std::time(NULL);
			if (now - prev_ladder_save_time > (signed)pvpgn::d2dbs::prefs_v3::laddersave_interval()) {
				d2ladder_saveladder();
				prev_ladder_save_time = now;
			}
			if (now - prev_keepalive_save_time > (signed)pvpgn::d2dbs::prefs_v3::keepalive_interval()) {
				dbs_keepalive();
				prev_keepalive_save_time = now;
			}
			if (now - prev_timeout_checktime > (signed)pvpgn::d2dbs::prefs_v3::timeout_checkinterval()) {
				dbs_check_timeout();
				prev_timeout_checktime = now;
			}
			return 0;
		}

		void dbs_server_loop(int lsocket)
		{
			struct sockaddr_in sinRemote;
			int sd;
			fd_set ReadFDs, WriteFDs, ExceptFDs;
			t_elem * elem;
			t_d2dbs_connection* it;
			bool bOK;
			const char* pcErrorType;
			struct timeval         tv;
			int highest_fd;
			socklen_t nAddrSize = sizeof(sinRemote);

			while (1) {

#ifdef WIN32
				if (g_ServiceStatus < 0 && kbhit() && getch() == 'q')
					d2dbs_signal_quit_wrapper();
				if (g_ServiceStatus == 0) d2dbs_signal_quit_wrapper();

				while (g_ServiceStatus == 2) Sleep(1000);
#endif

				if (d2dbs_handle_signal() < 0) break;

				dbs_handle_timed_events();
				highest_fd = dbs_server_setup_fdsets(&ReadFDs, &WriteFDs, &ExceptFDs, lsocket);

				tv.tv_sec = 0;
				tv.tv_usec = SELECT_TIME_OUT;
				switch (select(highest_fd + 1, &ReadFDs, &WriteFDs, &ExceptFDs, &tv)) {
				case -1:
					eventlog(eventlog_level_error, __FUNCTION__, "select() failed : {}", pstrerror(errno));
					continue;
				case 0:
					continue;
				default:
					break;
				}

				if (FD_ISSET(lsocket, &ReadFDs)) {
					sd = accept(lsocket, (struct sockaddr*)&sinRemote, &nAddrSize);
					if (sd == -1) {
						eventlog(eventlog_level_error, __FUNCTION__, "accept() failed : {}", pstrerror(errno));
						return;
					}

					char addrstr[INET_ADDRSTRLEN] = { 0 };
					inet_ntop(AF_INET, &(sinRemote.sin_addr), addrstr, sizeof(addrstr));
					eventlog(eventlog_level_info, __FUNCTION__, "accepted connection from {}:{} , socket {} .",
						addrstr, ntohs(sinRemote.sin_port), sd);
					eventlog_step(pvpgn::d2dbs::prefs_v3::logfile_gs(), eventlog_level_info, __FUNCTION__, "accepted connection from %s:%d , socket %d .",
						addrstr, ntohs(sinRemote.sin_port), sd);
					setsockopt_keepalive(sd);
					dbs_server_list_add_socket(sd, ntohl(sinRemote.sin_addr.s_addr));
					if (fcntl(sd, F_SETFL, O_NONBLOCK) < 0) {
						eventlog(eventlog_level_error, __FUNCTION__, "could not set TCP socket [{}] to non-blocking mode (closing connection) (fcntl: {})", sd, pstrerror(errno));
						close(sd);
					}
				}
				else if (FD_ISSET(lsocket, &ExceptFDs)) {
					eventlog(eventlog_level_error, __FUNCTION__, "exception on listening socket");
					/* FIXME: exceptions are not errors with TCP, they are out-of-band data */
					return;
				}

				LIST_TRAVERSE(dbs_server_connection_list, elem)
				{
					bOK = true;
					pcErrorType = 0;

					if (!(it = (t_d2dbs_connection*)elem_get_data(elem))) continue;
					if (FD_ISSET(it->sd, &ExceptFDs)) {
						bOK = false;
						pcErrorType = "General socket error"; /* FIXME: no no no no no */
						FD_CLR(it->sd, &ExceptFDs);
					}
					else {
	
						if (FD_ISSET(it->sd, &ReadFDs)) {
							bOK = dbs_server_read_data(it);
							pcErrorType = "Read error";
							FD_CLR(it->sd, &ReadFDs);
						}
	
						if (FD_ISSET(it->sd, &WriteFDs)) {
							bOK = dbs_server_write_data(it);
							pcErrorType = "Write error";
							FD_CLR(it->sd, &WriteFDs);
						}
					}

					if (!bOK) {
						int	err, errno2;
						socklen_t	errlen;
	
						err = 0;
						errlen = sizeof(err);
						errno2 = errno;
	
						if (getsockopt(it->sd, SOL_SOCKET, SO_ERROR, &err, &errlen) == 0) {
							if (errlen && err != 0) {
								err = err ? err : errno2;
								eventlog(eventlog_level_error, __FUNCTION__, "data socket error : {}({})", pstrerror(err), err);
							}
						}
						dbs_server_shutdown_connection(it);
						list_remove_elem(dbs_server_connection_list, &elem);
					}
					else {
						if (dbs_packet_handle(it) == -1) {
							eventlog(eventlog_level_error, __FUNCTION__, "dbs_packet_handle() failed");
							dbs_server_shutdown_connection(it);
							list_remove_elem(dbs_server_connection_list, &elem);
						}
					}
				}
			}
		}

		static void dbs_on_exit(void)
		{
			t_elem * elem;
			t_d2dbs_connection * it;

			if (dbs_server_listen_socket >= 0)
				close(dbs_server_listen_socket);
			dbs_server_listen_socket = -1;

			LIST_TRAVERSE(dbs_server_connection_list, elem)
			{
				if (!(it = (t_d2dbs_connection*)elem_get_data(elem))) continue;
				dbs_server_shutdown_connection(it);
				list_remove_elem(dbs_server_connection_list, &elem);
			}
			cl_destroy();
			d2dbs_d2ladder_destroy();
			list_destroy(dbs_server_connection_list);
			if (preset_d2gsid_head)
			{
				t_preset_d2gsid * curr;
				t_preset_d2gsid * next;

				for (curr = preset_d2gsid_head; curr; curr = next)
				{
					next = curr->next;
					delete curr;
				}
			}
			eventlog(eventlog_level_info, __FUNCTION__, "dbserver stopped");
		}

		int dbs_server_shutdown_connection(t_d2dbs_connection* conn)
		{
#ifdef PVPGN_V3_D2DBS_INTEGRATION
			(void)pvpgn_v3_d2dbs_server_shutdown_connection_try(
				conn->sd,
				conn->serverid,
				static_cast<unsigned int>(conn->type),
				conn->verified);
#endif
			shutdown(conn->sd, SHUT_RDWR);
			close(conn->sd);
			if (conn->verified && conn->type == CONNECT_CLASS_D2GS_TO_D2DBS) {
				eventlog(eventlog_level_info, __FUNCTION__, "unlock all characters on gs {}({})", conn->serverip, conn->serverid);
				eventlog_step(pvpgn::d2dbs::prefs_v3::logfile_gs(), eventlog_level_info, __FUNCTION__, "unlock all characters on gs %s(%d)", conn->serverip, conn->serverid);
				eventlog_step(pvpgn::d2dbs::prefs_v3::logfile_gs(), eventlog_level_info, __FUNCTION__, "close connection to gs on socket %d", conn->sd);
				cl_unlock_all_char_by_gsid(conn->serverid);
			}
			delete conn;
			return 1;
		}

		static int setsockopt_keepalive(int sock)
		{
			int		optval;
			socklen_t	optlen;
	
			optval = 1;
			optlen = sizeof(optval);
			if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen)) {
				eventlog(eventlog_level_info, __FUNCTION__, "failed set KEEPALIVE for socket {}, errno={}", sock, errno);
				return -1;
			}
			else {
				eventlog(eventlog_level_info, __FUNCTION__, "set KEEPALIVE option for socket {}", sock);
				return 0;
			}
		}

		static unsigned int get_preset_d2gsid(unsigned int ipaddr)
		{
			t_preset_d2gsid		*pgsid;

			pgsid = preset_d2gsid_head;
			while (pgsid)
			{
				if (pgsid->ipaddr == ipaddr)
					return pgsid->d2gsid;
				pgsid = pgsid->next;
			}
			/* not found, build a new item */
			pgsid = new t_preset_d2gsid{};
			pgsid->ipaddr = ipaddr;
			pgsid->d2gsid = ++dbs_packet_gs_id;
			/* add to list */
			pgsid->next = preset_d2gsid_head;
			preset_d2gsid_head = pgsid;
			return preset_d2gsid_head->d2gsid;
		}

	}

}
