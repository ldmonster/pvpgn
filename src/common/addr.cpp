/*
 * Copyright (C) 1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
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
#define ADDR_INTERNAL_ACCESS
#include "common/setup_before.h"
#include "common/addr.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <string>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif
#include "common/eventlog.h"
#include "common/list.h"
#include "common/util.h"

#include "common/setup_after.h"

// addr.cpp split into focused sub-modules (plan 15 §3 / SOLID-S)
#include "addr_core.cpp"
#include "addr_netaddr.cpp"
#include "addr_list.cpp"
