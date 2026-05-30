/*
 * Copyright (C) 1998,1999,2000  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 1999,2000,2001  Marco Ziech (mmz@gmx.net)
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
// packet.cpp — thin coordinator (plan 15 §3 / SOLID-S)
//
// All implementation has been split into focused sub-modules:
//   packet_buffer.cpp  — lifecycle (create/destroy/add_ref/del_ref/duplicate),
//                        class/type/size/flags/header-size accessors
//   packet_writer.cpp  — append operations (string, ntstring, lstr, data)
//   packet_reader.cpp  — read operations (get_raw_data, get_str_const, get_data_const)
//
// This file is intentionally empty; it exists only to preserve the original
// filename in version-control history and to document the decomposition.
