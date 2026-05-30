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

/* Chat channel/messaging packets: PROGIDENT2, JOINCHANNEL, CHANNELLIST, SERVERLIST,
   SERVER_MESSAGE (with MF_/CF_ flags and W3 icon constants), CLIENT_MESSAGE */
#ifndef INCLUDED_BNET_PROTOCOL_CHAT_CHANNEL_H
#define INCLUDED_BNET_PROTOCOL_CHAT_CHANNEL_H

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

	/******************************************************/
#define CLIENT_PROGIDENT2 0x0bff
	typedef struct
	{
		t_bnet_header h;
		bn_int        clienttag;
	} PACKED_ATTR() t_client_progident2;
	/******************************************************/


	/******************************************************/
#define CLIENT_JOINCHANNEL 0x0cff
	typedef struct
	{
		t_bnet_header h;
		bn_int        channelflag;
	} PACKED_ATTR() t_client_joinchannel;
#define CLIENT_JOINCHANNEL_NORMAL  0x00000000
#define CLIENT_JOINCHANNEL_GENERIC 0x00000001
#define CLIENT_JOINCHANNEL_CREATE  0x00000002
	/******************************************************/


	/******************************************************/
#define SERVER_CHANNELLIST 0x0bff
	typedef struct
	{
		t_bnet_header h;
		/* channel names */
	} PACKED_ATTR() t_server_channellist;
	/******************************************************/


	/******************************************************/
#define SERVER_SERVERLIST 0x04ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown1; /* 00 00 00 00 */
		/* list */
	} PACKED_ATTR() t_server_serverlist;
#define SERVER_SERVERLIST_UNKNOWN1 0x00000000
	/******************************************************/


	/******************************************************/
#define SERVER_MESSAGE 0x0fff
	typedef struct
	{
		t_bnet_header h;
		bn_int        type;
		bn_int        flags;     /* player flags (or channel flags for MT_CHANNEL) */
		bn_int        latency;
		bn_int        player_ip;  /* always zero? */
		bn_int        account_num; /* player's IP (big endian), no longer used, always 0D F0 AD BA */
		bn_int        reg_auth;  /* server ip and/or reg auth? CD key and/or account number? */
		/* player name */
		/* text */
	} PACKED_ATTR() t_server_message;
#define SERVER_MESSAGE_PLAYER_IP_DUMMY 0x00000000
	/* nok */
#define SERVER_MESSAGE_REG_AUTH 0xBAADF00D /* 0D F0 AD BA */
	/* #define SERVER_MESSAGE_REG_AUTH 0x07f694d8 */
#define SERVER_MESSAGE_ACCOUNT_NUM 0x0df0adba

#define SERVER_MESSAGE_TYPE_ADDUSER             0x00000001 /* ADD,USER,SHOWUSER */
#define SERVER_MESSAGE_TYPE_JOIN                0x00000002
#define SERVER_MESSAGE_TYPE_PART                0x00000003 /* LEAVE */
#define SERVER_MESSAGE_TYPE_WHISPER             0x00000004
#define SERVER_MESSAGE_TYPE_TALK                0x00000005 /* MESSAGE */
#define SERVER_MESSAGE_TYPE_BROADCAST           0x00000006
#define SERVER_MESSAGE_TYPE_CHANNEL             0x00000007 /* JOINING */
	/* unused?                                      0x00000008 */
#define SERVER_MESSAGE_TYPE_USERFLAGS           0x00000009
#define SERVER_MESSAGE_TYPE_WHISPERACK          0x0000000a /* WHISPERSENT */
	/* unused?                                      0x0000000b */
	/* unused?                                      0x0000000c */
#define SERVER_MESSAGE_TYPE_CHANNELFULL         0x0000000d
#define SERVER_MESSAGE_TYPE_CHANNELDOESNOTEXIST 0x0000000e
#define SERVER_MESSAGE_TYPE_CHANNELRESTRICTED   0x0000000f
	/* unused?                                      0x00000010 */
	/* unused?                                      0x00000011 */
#define SERVER_MESSAGE_TYPE_INFO                0x00000012
#define SERVER_MESSAGE_TYPE_ERROR               0x00000013
	/* unused?                                      0x00000014 */
	/* unused?                                      0x00000015 */
	/* unused?                                      0x00000016 */
#define SERVER_MESSAGE_TYPE_EMOTE               0x00000017

	/****** Player Flags ******/
	/* flag bits for above struct */

	/* ADDED BY UNDYING SOULZZ 4/7/02 */
#define W3_ICON_SET					0x00000000
#define MAX_STR_RACELEN				20
#define MAX_STR_ACCTPASSLEN         10
#define W3_RACE_RANDOM				32
#define W3_RACE_HUMANS				1
#define W3_RACE_ORCS				2
#define W3_RACE_UNDEAD				8
#define W3_RACE_NIGHTELVES			4
#define W3_RACE_DEMONS				16

#define W3_ICON_RANDOM				0 /* - Although when client presses random in PG and it sends "32" its "0" for icon */
#define W3_ICON_HUMANS				1
#define W3_ICON_ORCS				2
#define W3_ICON_UNDEAD				3 /* - Although when client presses undead in PG and it sends "8" its "3" for icon */
#define W3_ICON_NIGHTELVES			4
#define W3_ICON_DEMONS				5

	/* Icon setup  3RAW then <accounts level> <Race> <race wins> */
	/* Races: 1 = human, 2 = orc, 8 = undead, 4 = nightelf  32 = random */
	/* Misc icons are 6-9 * - There might be some icons not defined*/
	/* If you find them let us know pse,  forums.cheatlist.com in War3 Hacking/Development */

	/*Human Icons*/
#define W3_ICON_HUMAN_FOOTMAN					"3RAW 1 1 11"
#define W3_ICON_HUMAN_KNIGHT					"3RAW 1 1 100"
#define W3_ICON_HUMAN_ARCHMAGE					"3RAW 1 1 250"
#define W3_ICON_HUMAN_HERO						"3RAW 1 1 1000"

	/*Orc Icons*/
#define W3_ICON_ORC_PEON						"3RAW 1 2 00" /* default icon , unless u change it in code*/
#define W3_ICON_ORC_GRUNT						"3RAW 1 2 10"
#define W3_ICON_ORC_TAUREN						"3RAW 1 2 100"
#define W3_ICON_ORC_FARSEER						"3RAW 1 2 250"
#define W3_ICON_ORC_HERO						"3RAW 1 2 1000"

	/*Undead Icons*/
#define W3_ICON_UNDEAD_GHOUL					"3RAW 1 3 10"
#define W3_ICON_UNDEAD_ABOM						"3RAW 1 3 100"
#define W3_ICON_UNDEAD_LICH						"3RAW 1 3 250"
#define W3_ICON_UNDEAD_HERO						"3RAW 1 3 1000"

	/*Night Elf Icons*/
#define W3_ICON_ELF_ARCHER						"3RAW 1 4 10"
#define W3_ICON_ELF_DRUIDCLAW					"3RAW 1 4 100"
#define W3_ICON_ELF_PRIESMOON					"3RAW 1 4 250"
#define W3_ICON_ELF_HERO						"3RAW 1 4 1000"

	/*Random Icons , like NPC's, Creeps*/
#define W3_ICON_RANDOM_GREENDRAGON				"3RAW 1 9 10"
#define W3_ICON_RANDOM_BLACKDRAGON				"3RAW 1 9 100"
#define W3_ICON_RANDOM_REDDRAGON				"3RAW 1 9 250"
#define W3_ICON_RANDOM_BLUEDRAGON				"3RAW 1 9 1000"

	/* End of Undying Edits */

	/*      Blizzard Entertainment employee */
#define MF_BLIZZARD 0x00000001 /* blue Blizzard logo */
	/*      Channel operator */
#define MF_GAVEL    0x00000002 /* gavel */
	/*      Speaker in moderated channel */
#define MF_VOICE    0x00000004 /* megaphone */
	/*      System operator */
#define MF_BNET     0x00000008 /* (old: blue Blizzard, new: green b.net) or red BNETD logo */
	/*      Chat bot or other user without UDP support */
#define MF_PLUG     0x00000010 /* tiny plug to right of icon, no UDP */
	/*      Squelched/Ignored user */
#define MF_X        0x00000020 /* big red X */
	/*      Special guest of Blizzard Entertainment */
#define MF_SHADES   0x00000040 /* sunglasses */
	/* unused           0x00000080 */
	/*      Use BEL character in error codes. Some bots use it as a flag. Battle.net */
	/*      stopped supporting it recently. */
#define MF_BEEP     0x00000100 /* no change in icon */
	/*      Registered Professional Gamers League player */
#define MF_PGLPLAY  0x00000200 /* PGL player logo */
	/*      Registered Professional Gamers League official */
#define MF_PGLOFFL  0x00000400 /* PGL official logo */
	/*      Registered KBK player */
#define MF_KBKPLAY  0x00000800 /* KBK player logo */
	/*      Official KBK Referee */
#define MF_KBKREF   0x00001000 /* KBK referee logo */ /* FIXME: this number may be wrong */
	/* unused... FIXME: how many bits work? 16 or 32? */

	/****** Channel Flags ******/
	/* flag bits for MT_CHANNEL message */
#define CF_PUBLIC     0x00000001 /* public channel */
#define CF_MODERATED  0x00000002 /* moderated channel */
#define CF_RESTRICTED 0x00000004 /* ? restricted channel ? */
#define CF_THEVOID    0x00000008 /* "The Void" */
#define CF_SYSTEM     0x00000020 /* system channel */
#define CF_OFFICIAL   0x00001000 /* official channel */
	/******************************************************/


	/******************************************************/
#define CLIENT_MESSAGE 0x0eff
	typedef struct
	{
		t_bnet_header h;
		/* text */
	} PACKED_ATTR() t_client_message;
	/******************************************************/

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_CHAT_CHANNEL_H */
