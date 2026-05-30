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
 * anongame_protocol_profile.h — options 03/04/07/08: cancel, profile, tournament, clan
 *
 * Covers 0x44ff options:
 *   03 — server playgame cancel
 *   04 — client/server player profile request/reply
 *   07 — client/server tournament request/reply
 *   08 — client/server clan profile request/reply
 *
 * Structs:
 *   t_server_findanongame_playgame_cancel  — server cancel notification
 *   t_client_findanongame_profile          — client player profile request
 *   t_server_findanongame_profile2         — server player profile reply
 *   t_client_anongame_tournament_request   — client tournament info request
 *   t_server_anongame_tournament_reply     — server tournament info reply
 *   t_client_findanongame_profile_clan     — client clan profile request
 *   t_server_findanongame_profile_clan     — server clan profile reply
 */

#ifndef INCLUDED_ANONGAME_PROTOCOL_PROFILE
#define INCLUDED_ANONGAME_PROTOCOL_PROFILE

#ifdef JUST_NEED_TYPES
# include "common/bn_type.h"
#else
# define JUST_NEED_TYPES
# include "common/bn_type.h"
# undef JUST_NEED_TYPES
#endif

namespace pvpgn
{

	/***********************************************************************************/
	/* option 03 - playgame cancel */
#define SERVER_FINDANONGAME_PLAYGAME_CANCEL 	0x44ff
	typedef struct
	{
		t_bnet_header h; /* header */
		bn_byte cancel; /* Cancel byte always 03 */
		bn_int  count;
	} PACKED_ATTR() t_server_findanongame_playgame_cancel;


	/* option 04 - profile request */
	typedef struct
	{
		t_bnet_header h;
		bn_byte     option;
		bn_int          count;
		/* USERNAME TO LOOKUP
		 * CLIENT TAG */
	} PACKED_ATTR() t_client_findanongame_profile;

#define SERVER_FINDANONGAME_PROFILE		0x44ff
	typedef struct
	{
		t_bnet_header	h;
		bn_byte		option;
		bn_int		count;
		bn_int		icon;
		bn_byte		rescount;
		/* REST OF PROFILE STATS - THIS WILL BE SET IN HANDLE_BNET.C after
		 * SERVER LOOKS UP THE USER ACCOUNT */
	} PACKED_ATTR() t_server_findanongame_profile2;

#define SERVER_FINDANONGAME_PROFILE_UNKNOWN2    0x6E736865 /* Sheep */

	/***********************************************************************************/
	/* option 07 - tournament request */
#define CLIENT_FINDANONGAME_TOURNAMENT_REQUEST  0x44ff
	typedef struct
	{
		t_bnet_header       h;
		bn_byte             option; /* 07 */
		bn_int              count;  /* 01 00 00 00 */
	} PACKED_ATTR() t_client_anongame_tournament_request;

#define SERVER_FINDANONGAME_TOURNAMENT_REPLY    0x44ff
	typedef struct
	{
		t_bnet_header       h;
		bn_byte             option;     /* 07 */
		bn_int              count;      /* 00 00 00 01 reply with same number */
		bn_byte             type;	    /* type - 	01 = notice - time = prelim round begins
						 *		02 = signups - time = signups end
						 *		03 = signups over - time = prelim round ends
						 *		04 = prelim over - time = finals round 1 begins
						 */
		bn_byte             unknown;    /* 00 */
		bn_short            unknown4;   /* random ? might be part of time/date ? */
		bn_int              timestamp;
		bn_byte             unknown5;   /* 01 effects time/date */
		bn_short            countdown;  /* countdown until next timestamp (seconds) */
		bn_short            unknown2;   /* 00 00 */
		bn_byte		wins;	    /* during prelim */
		bn_byte		losses;     /* during prelim */
		bn_byte             ties;	    /* during prelim */
		bn_byte             unknown3;   /* 00 = notice.  08 = signups thru prelim over (02-04) */
		bn_byte             selection;  /* matches anongame_TY_section of DESC */
		bn_byte             descnum;    /* matches desc_count of DESC */
		bn_byte             nulltag;    /* 00 */
	} PACKED_ATTR() t_server_anongame_tournament_reply;

	/***********************************************************************************/
	/* option 08 - clan profile request */
	typedef struct
	{
		t_bnet_header h;
		bn_byte	option;
		bn_int	count;
		bn_int	clantag;
		bn_int	clienttag;
	} PACKED_ATTR() t_client_findanongame_profile_clan;

#define SERVER_FINDANONGAME_PROFILE_CLAN	0x44ff

	typedef struct
	{
		t_bnet_header	h;
		bn_byte		option;
		bn_int		count;
		bn_byte		rescount;
		/* REST OF PROFILE STATS - THIS WILL BE SET IN HANDLE_BNET.C after
		 * SERVER LOOKS UP THE USER ACCOUNT */
	} PACKED_ATTR() t_server_findanongame_profile_clan;

}

#endif /* INCLUDED_ANONGAME_PROTOCOL_PROFILE */
