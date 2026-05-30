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

/*
 * bnet_protocol_auth.h — umbrella header (plan 05 §3 / SOLID-S)
 *
 * All auth packet structs have been split into focused sub-headers by
 * message family.  This file exists only to preserve the original include
 * path used by bnet_protocol.h and any other consumers; it simply pulls in
 * all five sub-headers so callers need not change their #include directives.
 *
 * Sub-header layout:
 *   bnet_protocol_auth_version.h  — version-check / auth-request / ladder-search
 *                                   (CREATEACCTREQ1, UNKNOWN_2B, PROGIDENT,
 *                                    AUTHREQ1/109, AUTHREPLY1/109,
 *                                    REGSNOOPREQ/REPLY, ICONREQ/REPLY,
 *                                    LADDERSEARCHREQ/REPLY, t_cdkey_info)
 *   bnet_protocol_auth_cdkey.h    — CD-key validation
 *                                   (CDKEY, CDKEY2, CDKEY3, CDKEYREPLY/2/3)
 *   bnet_protocol_auth_realm.h    — realm-list exchange
 *                                   (REALMLISTREQ, REALMLISTREQ_110,
 *                                    REALMLISTREPLY, REALMLISTREPLY_110)
 *   bnet_protocol_auth_login.h    — login flow + D2 character list
 *                                   (PROFILEREQ/REPLY, UNKNOWN_37/39,
 *                                    LOGINREQ2, MOTD_W3, LOGINREQ_W3,
 *                                    LOGINREPLY_W3, LOGONPROOFREQ/REPLY,
 *                                    t_d2char_info)
 *   bnet_protocol_auth_account.h  — account management
 *                                   (PASSCHANGEREQ/REPLY,
 *                                    PASSCHANGEPROOFREQ/REPLY,
 *                                    CREATEACCOUNT_W3)
 */
#ifndef INCLUDED_BNET_PROTOCOL_AUTH_H
#define INCLUDED_BNET_PROTOCOL_AUTH_H

#include "bnet_protocol/bnet_protocol_auth_version.h"
#include "bnet_protocol/bnet_protocol_auth_cdkey.h"
#include "bnet_protocol/bnet_protocol_auth_realm.h"
#include "bnet_protocol/bnet_protocol_auth_login.h"
#include "bnet_protocol/bnet_protocol_auth_account.h"

#endif /* INCLUDED_BNET_PROTOCOL_AUTH_H */
