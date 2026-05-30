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

/* Umbrella header — includes all bnet_protocol_chat sub-headers.
 * Split by concern (plan 15 §3):
 *   bnet_protocol_chat_account.h — CHANGEGAMEPORT, CREATEACCOUNT_W3, CREATEACCTREQ2/REPLY2
 *   bnet_protocol_chat_login.h   — CDKEYREPLY2, LOGINREQ1/REPLY1/REPLY2, CHANGEPASSREQ/ACK
 *   bnet_protocol_chat_file.h    — UDPOK, FILEINFOREQ, FILEINFOREPLY
 *   bnet_protocol_chat_stats.h   — STATSREQ/REPLY, PLAYERINFOREQ/REPLY
 *   bnet_protocol_chat_channel.h — PROGIDENT2, JOINCHANNEL, CHANNELLIST, SERVERLIST,
 *                                  SERVER_MESSAGE (MF_/CF_ flags, W3 icon constants), CLIENT_MESSAGE
 *   bnet_protocol_chat_game.h    — GAMELISTREQ, GAMELISTREPLY
 */
#ifndef INCLUDED_BNET_PROTOCOL_CHAT_H
#define INCLUDED_BNET_PROTOCOL_CHAT_H

#include "bnet_protocol/bnet_protocol_chat_account.h"
#include "bnet_protocol/bnet_protocol_chat_login.h"
#include "bnet_protocol/bnet_protocol_chat_file.h"
#include "bnet_protocol/bnet_protocol_chat_stats.h"
#include "bnet_protocol/bnet_protocol_chat_channel.h"
#include "bnet_protocol/bnet_protocol_chat_game.h"

#endif /* INCLUDED_BNET_PROTOCOL_CHAT_H */
