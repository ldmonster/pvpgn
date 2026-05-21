/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000  Ross Combs (rocombs@cs.nmsu.edu)
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
#include "common/network.h"

#include <cerrno>
#include <cstring>

#include "compat/socket.h"
#include "compat/recv.h"
#include "compat/send.h"
#include "compat/netinet_in.h"
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#else
#  include <sys/socket.h>
#  include <cerrno>
#endif
#include "common/packet.h"
#include "common/eventlog.h"
#include "common/field_sizes.h"
#include "common/setup_after.h"


namespace pvpgn
{

	extern int net_recv(int sock, void *buff, int len)
	{
		int res;

		res = recv(sock, buff, len, 0);
		if (res > 0) return res;
		if (!res) return -1;	/* connection closed */

#ifdef _WIN32
		int const sock_err = WSAGetLastError();
#else
		int const sock_err = errno;
#endif

		if (
#ifdef EINTR
			sock_err == EINTR ||
#endif
#ifdef EAGAIN
			sock_err == EAGAIN ||
#endif
#ifdef EWOULDBLOCK
			sock_err == EWOULDBLOCK ||
#endif
#ifdef ENOMEM
			sock_err == ENOMEM ||
#endif
			0) /* try later */
			return 0;

		if (
#ifdef ENOTCONN
			sock_err == ENOTCONN ||
#endif
#ifdef ECONNRESET
			sock_err == ECONNRESET ||
#endif
			0) {
			/*	    eventlog(eventlog_level_debug,__FUNCTION__,"[{}] remote host closed connection (recv: {})",sock,std::strerror(sock_err)); */
			return -1; /* common error: close connection, but no message */
		}

		eventlog(eventlog_level_debug, __FUNCTION__, "[{}] receive error (closing connection) (recv: {})", sock, std::strerror(sock_err));
		return -1;
	}

	extern int net_recv_packet(int sock, t_packet * packet, unsigned int * currsize)
	{
		int          addlen;
		unsigned int header_size;
		void *       temp;

		if (!packet) {
			eventlog(eventlog_level_error, __FUNCTION__, "[{}] got NULL packet (closing connection)", sock);
			return -1;
		}

		if (!currsize) {
			eventlog(eventlog_level_error, __FUNCTION__, "[{}] got NULL currsize (closing connection)", sock);
			return -1;
		}

		if ((header_size = packet_get_header_size(packet)) >= MAX_PACKET_SIZE) {
			eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not determine header size (closing connection)", sock);
			return -1;
		}

		if (!(temp = packet_get_raw_data_build(packet, *currsize))) {
			eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not obtain raw data pointer at offset {} (closing connection)", sock, *currsize);
			return -1;
		}

		if (*currsize < header_size)
			addlen = net_recv(sock, temp, header_size - *currsize);
		else {
			unsigned int total_size = packet_get_size(packet);

			if (total_size < header_size) {
				eventlog(eventlog_level_warn, __FUNCTION__, "[{}] corrupted packet received (total_size={} currsize={}) (closing connection)", sock, total_size, *currsize);
				return -1;
			}

			if (*currsize >= total_size) {
				eventlog(eventlog_level_warn, __FUNCTION__, "[{}] more data requested for already complete packet (total_size={} currsize={}) (closing connection)", sock, total_size, *currsize);
				return -1;
			}

			addlen = net_recv(sock, temp, total_size - *currsize);
		}

		if (addlen <= 0) return addlen;

		*currsize += addlen;

		if (*currsize >= header_size && *currsize == packet_get_size(packet))
			return 1;

		return 0;
	}

	extern int net_send(int sock, const void *buff, int len)
	{
		int res;

		res = send(sock, buff, len, 0);

		if (res > 0) return res;
		if (!res) return -1;

#ifdef _WIN32
		int const sock_err = WSAGetLastError();
#else
		int const sock_err = errno;
#endif

		if (
#ifdef EINTR
			sock_err == EINTR ||
#endif
#ifdef EAGAIN
			sock_err == EAGAIN ||
#endif
#ifdef EWOULDBLOCK
			sock_err == EWOULDBLOCK ||
#endif
#ifdef ENOBUFS
			sock_err == ENOBUFS ||
#endif
#ifdef ENOMEM
			sock_err == ENOMEM ||
#endif
			0)
			return 0; /* try again later */

		if (
#ifdef EPIPE
			sock_err != EPIPE &&
#endif
#ifdef ECONNRESET
			sock_err != ECONNRESET &&
#endif
			1) eventlog(eventlog_level_debug, __FUNCTION__, "[{}] could not send data (closing connection) (send: {})", sock, std::strerror(sock_err));

		return -1;
	}

	extern int net_send_packet(int sock, t_packet const * packet, unsigned int * currsize)
	{
		unsigned int size;
		int          addlen;

		if (!packet) {
			eventlog(eventlog_level_error, __FUNCTION__, "[{}] got NULL packet (closing connection)", sock);
			return -1;
		}

		if (!currsize) {
			eventlog(eventlog_level_error, __FUNCTION__, "[{}] got NULL currsize (closing connection)", sock);
			return -1;
		}

		if ((size = packet_get_size(packet)) < 1) {
			eventlog(eventlog_level_error, __FUNCTION__, "[{}] packet to send is empty (skipping it)", sock);
			*currsize = 0;
			return 1;
		}

		addlen = net_send(sock, packet_get_raw_data_const(packet, *currsize), size - *currsize);

		if (addlen <= 0) return addlen;

		*currsize += addlen;
		/* sent all data in this packet? */
		if (size == *currsize)
		{
			*currsize = 0;
			return 1;
		}

		return 0;
	}

}
