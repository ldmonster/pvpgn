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

/* Misc packets: AD*, READMEMORY, LADDERREQ, ECHO, PING */
#ifndef INCLUDED_BNET_PROTOCOL_MISC_H
#define INCLUDED_BNET_PROTOCOL_MISC_H

#ifdef JUST_NEED_TYPES
# include "common/bn_type.h"
#else
# define JUST_NEED_TYPES
# include "common/bn_type.h"
# undef JUST_NEED_TYPES
#endif

#include "bnet_protocol/bnet_protocol_common.h"

namespace pvpgn
{

#define CLIENT_ADREQ 0x15ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        archtag;
		bn_int        clienttag;
		bn_int        prev_adid; /* zero if first request */
		bn_int        ticks;     /* Unix-style time in seconds */
	} PACKED_ATTR() t_client_adreq;
	/******************************************************/


	/******************************************************/
	/*
	Sent in response to a CLIENT_ADREQ to tell the client which
	banner to display next

	FF 15 3A 00 72 00 00 00   2E 70 63 78 50 15 7A 1C    ..:.r....pcxP.z.
	CE 0F BD 01 61 64 30 30   30 30 37 32 2E 70 63 78    ....ad000072.pcx
	00 68 74 74 70 3A 2F 2F   77 77 77 2E 62 6C 69 7A    .http://www.bliz
	7A 61 72 64 2E 63 6F 6D   2F 00                      zard.com/.

	FF 15 3C 00 C3 00 00 00   2E 73 6D 6B 00 9B 36 A6    ..<......smk..6.
	8F 5B BE 01 61 64 30 30   30 30 63 33 2E 73 6D 6B    .[..ad0000c3.smk
	00 68 74 74 70 3A 2F 2F   77 77 77 2E 66 61 74 68    .http://www.fath
	65 72 68 6F 6F 64 2E 6F   72 67 2F 00                erhood.org/.

	FF 15 36 00 2B 51 02 00            ..6.+Q..
	2E 70 63 78 00 00 00 00   58 01 B2 00 61 64 30 32    .pcx....X...ad02
	35 31 32 62 2E 70 63 78   00 68 74 74 70 3A 2F 2F    512b.pcx.http://
	77 77 77 2E 66 73 67 73   2E 63 6F 6D 2F 00          www.fsgs.com/.
	*/
#define SERVER_ADREPLY 0x15ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        adid;
		bn_int        extensiontag; /* unlike other tags, this one is "forward" */
		bn_long       timestamp;    /* file modification time? */
		/* filename */
		/* link URL */
	} PACKED_ATTR() t_server_adreply;
	/******************************************************/


	/******************************************************/
#define CLIENT_ADACK 0x21ff
	/*
	Sent after client has displayed the banner

	0000:   FF 21 36 00 36 38 58 49   52 41 54 53 72 00 00 00    .!6.68XIRATSr...
	0010:   61 64 30 30 30 30 37 32   2E 70 63 78 00 68 74 74    ad000072.pcx.htt
	0020:   70 3A 2F 2F 77 77 77 2E   62 6C 69 7A 7A 61 72 64    p://www.blizzard
	0030:   2E 63 6F 6D 2F 00                                    .com/.

	0000:   FF 21 38 00 36 38 58 49   4C 54 52 44 C3 00 00 00    .!8.68XILTRD....
	0010:   61 64 30 30 30 30 63 33   2E 73 6D 6B 00 68 74 74    ad0000c3.smk.htt
	0020:   70 3A 2F 2F 77 77 77 2E   66 61 74 68 65 72 68 6F    p://www.fatherho
	0030:   6F 64 2E 6F 72 67 2F 00                              od.org/.
	*/
	typedef struct
	{
		t_bnet_header h;
		bn_int        archtag;
		bn_int        clienttag;
		bn_int        adid;
		/* adfile */
		/* adlink */
	} PACKED_ATTR() t_client_adack;
	/******************************************************/


	/******************************************************/
	/*
	Sent if the user clicks on the adbanner
	*/
#define CLIENT_ADCLICK 0x16ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        adid;
		bn_int        unknown1;
	} PACKED_ATTR() t_client_adclick;
	/******************************************************/


	/******************************************************/
	/* first seen in Diablo II? */
	/*
	FF 41 08 00 01 00 00 00                              .A......
	*/
#define CLIENT_ADCLICK2 0x41ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        adid;
	} PACKED_ATTR() t_client_adclick2;
	/******************************************************/


	/******************************************************/
	/* first seen in Diablo II? */
	/*
					  FF 41 2C   00 0B 20 00 00 68 74   .qT....A,.. ..ht
					  74 70 3A 2F 2F 77 77 77 2E   62 6C 69 7A 7A 61 72   tp://www.blizzar
					  64 2E 63 6F 6D 2F 64 69 61   62 6C 6F 32 65 78 70   d.com/diablo2exp
					  2F 00                                               /.
					  */
#define SERVER_ADCLICKREPLY2 0x41ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        adid;
		/* link URL */
	} PACKED_ATTR() t_server_adclickreply2;
	/******************************************************/


	/******************************************************/
	/* seen in SC107a */
#define CLIENT_READMEMORY 0x17ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        request_id;
		/* Memory */
	} PACKED_ATTR() t_client_readmemory;

#define SERVER_READMEMORY 0x17ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        request_id;
		bn_int        address;
		bn_int        length;
	} PACKED_ATTR() t_server_readmemory;
	/******************************************************/


	/******************************************************/
	/* seen in SC107a */
#define CLIENT_UNKNOWN_24 0x24ff
	typedef struct
	{
		t_bnet_header h;
		/* FIXME: what is in here... is there a cooresponding
		   server packet? */
	} PACKED_ATTR() t_client_unknown_24;
	/******************************************************/


	/******************************************************/
	/*
	FF 2E 18 00 4E 42 32 57   01 00 00 00 00 00 00 00    ....NB2W........
	00 00 00 00 0A 00 00 00                              ........
	*/
#define CLIENT_LADDERREQ 0x2eff
	typedef struct
	{
		t_bnet_header h;
		bn_int        clienttag;
		bn_int        id; /* (AKA ladder type) 1==standard, 3==ironman */
		bn_int        type; /* (AKA ladder sort) */
		bn_int        startplace; /* start listing on this entries */
		bn_int        count; /* how many entries to list */
	} PACKED_ATTR() t_client_ladderreq;
#define CLIENT_LADDERREQ_ID_STANDARD       0x00000001
#define CLIENT_LADDERREQ_ID_IRONMAN        0x00000003
#define CLIENT_LADDERREQ_TYPE_HIGHESTRATED 0x00000000
#define CLIENT_LADDERREQ_TYPE_MOSTWINS     0x00000002
#define CLIENT_LADDERREQ_TYPE_MOSTGAMES    0x00000003
	/******************************************************/


	/******************************************************/
	/*
	Sent in repsonse to CLIENT_LADDERREQ

	FF 2E AB 03 52 41 54 53   01 00 00 00 00 00 00 00    ....RATS........
	00 00 00 00 0A 00 00 00   27 00 00 00 01 00 00 00    ........'.......
	01 00 00 00 74 06 00 00   00 00 00 00 27 00 00 00    ....t.......'...
	01 00 00 00 01 00 00 00   74 06 00 00 00 00 00 00    ........t.......
	00 00 00 00 00 00 00 00   FF FF FF FF 74 06 00 00    ............t...
	00 00 00 00 00 00 00 00   12 27 4E FF 2D DD BE 01    .........'N.-...
	12 27 4E FF 2D DD BE 01   6E 65 6D 62 69 3A 29 6B    .'N.-...nembi:)k
	69 6C 6C 65 72 00 1D 00   00 00 03 00 00 00 00 00    iller...........
	00 00 67 06 00 00 00 00   00 00 1D 00 00 00 03 00    ..g.............
	00 00 00 00 00 00 67 06   00 00 00 00 00 00 01 00    ......g.........
	00 00 00 00 00 00 FF FF   FF FF 67 06 00 00 01 00    ..........g.....
	00 00 00 00 00 00 D8 5B   9E D0 32 DD BE 01 D8 5B    .......[..2....[
	9E D0 32 DD BE 01 53 4B   45 4C 54 4F 4E 00 1F 00    ..2...SKELTON...
	00 00 03 00 00 00 00 00   00 00 EF 05 00 00 00 00    ................
	00 00 1F 00 00 00 03 00   00 00 00 00 00 00 EF 05    ................
	00 00 00 00 00 00 02 00   00 00 00 00 00 00 FF FF    ................
	FF FF EF 05 00 00 02 00   00 00 00 00 00 00 62 26    ..............b&
	55 0C 51 DC BE 01 62 26   55 0C 51 DC BE 01 7A 69    U.Q...b&U.Q...zi
	7A 69 62 65 5E 2E 7E 00   19 00 00 00 02 00 00 00    zibe^.~.........
	00 00 00 00 FC 05 00 00   00 00 00 00 18 00 00 00    ................
	02 00 00 00 00 00 00 00   EE 05 00 00 00 00 00 00    ................
	03 00 00 00 00 00 00 00   FF FF FF FF FC 05 00 00    ................
	03 00 00 00 00 00 00 00   A0 25 4F 31 90 DE BE 01    .........%O1....
	8C F1 7F 9F 66 DD BE 01   59 4F 4F 4A 49 4E 27 53    ....f...YOOJIN'S
	00 1D 00 00 00 02 00 00   00 00 00 00 00 EC 05 00    ................
	00 00 00 00 00 1D 00 00   00 02 00 00 00 00 00 00    ................
	00 EC 05 00 00 00 00 00   00 04 00 00 00 00 00 00    ................
	00 FF FF FF FF EC 05 00   00 03 00 00 00 00 00 00    ................
	00 F4 58 78 82 2F D7 BE   01 F4 58 78 82 2F D7 BE    ..Xx./....Xx./..
	01 3D 7B 5F 7C 5F 7D 3D   00 1A 00 00 00 00 00 00    .={_|_}=........
	00 00 00 00 00 E2 05 00   00 00 00 00 00 1A 00 00    ................
	00 00 00 00 00 00 00 00   00 E2 05 00 00 00 00 00    ................
	00 05 00 00 00 00 00 00   00 FF FF FF FF E2 05 00    ................
	00 05 00 00 00 00 00 00   00 F2 DC 0D 1F 6C DD BE    .............l..
	01 F2 DC 0D 1F 6C DD BE   01 5B 53 50 41 43 45 5D    .....l...[SPACE]
	2D 31 2D 54 2E 53 2E 4A   00 15 00 00 00 02 00 00    -1-T.S.J........
	00 01 00 00 00 E0 05 00   00 00 00 00 00 15 00 00    ................
	00 02 00 00 00 01 00 00   00 E0 05 00 00 00 00 00    ................
	00 06 00 00 00 00 00 00   00 FF FF FF FF E0 05 00    ................
	00 06 00 00 00 00 00 00   00 7A C9 F6 6E 0B DE BE    .........z..n...
	01 7A C9 F6 6E 0B DE BE   01 5B 46 65 77 5D 2D 44    .z..n....[Few]-D
	2E 73 00 23 00 00 00 00   00 00 00 00 00 00 00 DF    .s.#............
	05 00 00 00 00 00 00 23   00 00 00 00 00 00 00 00    .......#........
	00 00 00 DF 05 00 00 00   00 00 00 07 00 00 00 00    ................
	00 00 00 FF FF FF FF E2   05 00 00 04 00 00 00 00    ................
	00 00 00 6E D9 DC 3A E2   DA BE 01 6E D9 DC 3A E2    ...n..:....n..:.
	DA BE 01 5B 4C 2E 73 5D   2D 43 6F 6F 6C 00 1F 00    ...[L.s]-Cool...
	00 00 07 00 00 00 02 00   00 00 DD 05 00 00 00 00    ................
	00 00 1F 00 00 00 07 00   00 00 02 00 00 00 DD 05    ................
	00 00 00 00 00 00 08 00   00 00 00 00 00 00 FF FF    ................
	FF FF 07 06 00 00 02 00   00 00 00 00 00 00 B6 CE    ................
	B1 D8 7A D8 BE 01 B6 CE   B1 D8 7A D8 BE 01 52 6F    ..z.......z...Ro
	60 4C 65 58 7E 50 72 4F   27 5A 65 4E 00 18 00 00    `LeX~PrO'ZeN....
	00 04 00 00 00 00 00 00   00 DD 05 00 00 00 00 00    ................
	00 18 00 00 00 04 00 00   00 00 00 00 00 DD 05 00    ................
	00 00 00 00 00 09 00 00   00 00 00 00 00 FF FF FF    ................
	FF DD 05 00 00 04 00 00   00 00 00 00 00 50 76 4C    .............PvL
	F7 AD D7 BE 01 50 76 4C   F7 AD D7 BE 01 48 61 6E    .....PvL.....Han
	5F 65 53 54 68 65 72 2E   27 27 00                   _eSTher.''.
	*/
#define SERVER_LADDERREPLY 0x2eff
	typedef struct
	{
		t_bnet_header h;
		bn_int        clienttag;
		bn_int        id; /* (AKA ladder type) 1==standard, 3==ironman */
		bn_int        type; /* (AKA ladder sort) */
		bn_int        startplace; /* start listing on this entries */
		bn_int        count; /* how many entries to list */
		/* ladder entry */
		/* player name */
	} PACKED_ATTR() t_server_ladderreply;
#define CLIENT_LADDERREPLY_ID_STANDARD 0x00000001
#define CLIENT_LADDERREPLY_ID_IRONMAN  0x00000003

	typedef struct
	{
		bn_int wins;
		bn_int loss;
		bn_int disconnect;
		bn_int rating;
		bn_int rank;
	} PACKED_ATTR() t_ladder_data;

	typedef struct
	{
		t_ladder_data current;
		t_ladder_data active;
		bn_int        ttest[6]; /* 00 00 00 00  00 00 00 00  FF FF FF FF  74 06 00 00  00 00 00 00 00  00 00 00 */
		bn_long       lastgame_current; /* timestamp */
		bn_long       lastgame_active;  /* timestamp */
	} PACKED_ATTR() t_ladder_entry;
	/******************************************************/


	/******************************************************/
	/*
										FF 25 08 00 EA 7F DB 02   .%......
										*/
#define CLIENT_ECHOREPLY 0x25ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        ticks;
	} PACKED_ATTR() t_client_echoreply;
	/******************************************************/


	/******************************************************/
#define SERVER_ECHOREQ 0x25ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        ticks;
	} PACKED_ATTR() t_server_echoreq;
	/******************************************************/



	/******************************************************/
	/*
	What I'm calling the ping happens every 90 seconds during gameplay.  I'm
	not exactly sure what it is, but it didn't hurt that we didn't respond up
	to now...  I went ahead and coded a response in.  This packet is sent
	at other times as well, even before login.

	This is probably a keepalive packet and the UDP is sent to make
	sure the UDP entry on the NAT gateway remains valid.

	Prolix calls these null packets and says they are sent every 60
	seconds.

	This seems to be associated with a UDP packet 7 from the client:
	7: cli class=bnet[0x01] type=CLIENT_PINGREQ[0x00ff] length=4
	0000:   FF 00 04 00                                          ....
	5: clt prot=udp[0x07] from=128.123.62.23:6112 to=128.123.62.23:6112 length=8
	0000:   07 00 00 00 2E 95 9D 00                              ........
	7: srv class=bnet[0x01] type=SERVER_PINGREPLY[0x00ff] length=4
	0000:   FF 00 04 00                                          ....

	FF 00 04 00                                          ....
	*/
#define CLIENT_PINGREQ 0x00ff
	typedef struct
	{
		t_bnet_header h;
	} PACKED_ATTR() t_client_pingreq;
	/******************************************************/


	/******************************************************/
	/*
	FF 00 04 00                                          ....
	*/
#define SERVER_PINGREPLY 0x00ff
	typedef struct
	{
		t_bnet_header h;
	} PACKED_ATTR() t_server_pingreply;
	/******************************************************/


	/******************************************************/
	/*
	FF 2C 3D 02 00 00 00 00   08 00 00 00 03 00 00 00    .,=.............
	00 00 00 00 00 00 00 00   00 00 00 00 00 00 00 00    ................
	00 00 00 00 00 00 00 00   00 00 00 00 52 6F 73 73    ............Ross
	5F 43 4D 00 00 00 00 00   00 00 00 4F 6E 20 6D 61    _CM........On ma
	70 20 22 43 68 61 6C 6C   65 6E 67 65 72 22 3A 0A    p "Challenger":.
	00 52 6F 73 73 5F 43 4D   20 77 61 73 20 50 72 6F    .Ross_CM was Pro
	74 6F 73 73 20 61 6E 64   20 70 6C 61 79 65 64 20    toss and played
	66 6F 72 20 33 31 20 6D   69 6E 75 74 65 73 0A 0A    for 31 minutes..
	20 20 4F 76 65 72 61 6C   6C 20 53 63 6F 72 65 20      Overall Score
	32 38 31 30 35 0A 20 20   20 20 20 20 20 20 20 31    28105.         1
	31 37 30 30 20 66 6F 72   20 55 6E 69 74 73 0A 20    1700 for Units.
	20 20 20 20 20 20 20 20   20 31 34 37 35 20 66 6F             1475 fo
	72 20 53 74 72 75 63 74   75 72 65 73 0A 20 20 20    r Structures.
	20 20 20 20 20 20 31 34   39 33 30 20 66 6F 72 20          14930 for
	52 65 73 6F 75 72 63 65   73 0A 0A 20 20 55 6E 69    Resources..  Uni
	74 73 20 53 63 6F 72 65   20 31 31 37 30 30 0A 20    ts Score 11700.
	20 20 20 20 20 20 20 20   20 20 20 37 30 20 55 6E               70 Pn
	*/

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_MISC_H */
