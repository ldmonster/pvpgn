/*
 * Copyright (C) 2004	Aaron
 * Copyright (C) 2004	CreepLord (creeplord@pvpgn.org)
 * Copyright (C) 2007,2008	Pelish (pelish@gmail.com)
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
// Client-tag conversion, validation, and human-readable title lookup (plan 15 §3 / SOLID-S).
// Included as a sub-TU by tag.cpp — do not compile directly.

namespace pvpgn
{

	/* fixme: have all functions call tag_str_to_uint() */
	extern t_clienttag clienttag_str_to_uint(char const * clienttag)
	{
		if (!clienttag)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL clienttag");
			return CLIENTTAG_UNKNOWN_UINT;
		}

		return tag_str_to_uint(clienttag);
	}

	/* fixme: have all fuctions call tag_uint_to_str() */
	extern char const * clienttag_uint_to_str(t_clienttag clienttag)
	{
		switch (clienttag)
		{
		case CLIENTTAG_BNCHATBOT_UINT:
			return CLIENTTAG_BNCHATBOT;
		case CLIENTTAG_STARCRAFT_UINT:
			return CLIENTTAG_STARCRAFT;
		case CLIENTTAG_BROODWARS_UINT:
			return CLIENTTAG_BROODWARS;
		case CLIENTTAG_SHAREWARE_UINT:
			return CLIENTTAG_SHAREWARE;
		case CLIENTTAG_DIABLORTL_UINT:
			return CLIENTTAG_DIABLORTL;
		case CLIENTTAG_DIABLOSHR_UINT:
			return CLIENTTAG_DIABLOSHR;
		case CLIENTTAG_WARCIIBNE_UINT:
			return CLIENTTAG_WARCIIBNE;
		case CLIENTTAG_DIABLO2DV_UINT:
			return CLIENTTAG_DIABLO2DV;
		case CLIENTTAG_STARJAPAN_UINT:
			return CLIENTTAG_STARJAPAN;
		case CLIENTTAG_DIABLO2ST_UINT:
			return CLIENTTAG_DIABLO2ST;
		case CLIENTTAG_DIABLO2XP_UINT:
			return CLIENTTAG_DIABLO2XP;
		case CLIENTTAG_WARCRAFT3_UINT:
			return CLIENTTAG_WARCRAFT3;
		case CLIENTTAG_WAR3XP_UINT:
			return CLIENTTAG_WAR3XP;
		case CLIENTTAG_IIRC_UINT:
			return CLIENTTAG_IIRC;
		case CLIENTTAG_WCHAT_UINT:
			return CLIENTTAG_WCHAT;
		case CLIENTTAG_TIBERNSUN_UINT:
			return CLIENTTAG_TIBERNSUN;
		case CLIENTTAG_TIBSUNXP_UINT:
			return CLIENTTAG_TIBSUNXP;
		case CLIENTTAG_REDALERT_UINT:
			return CLIENTTAG_REDALERT;
		case CLIENTTAG_REDALERT2_UINT:
			return CLIENTTAG_REDALERT2;
		case CLIENTTAG_DUNE2000_UINT:
			return CLIENTTAG_DUNE2000;
		case CLIENTTAG_NOX_UINT:
			return CLIENTTAG_NOX;
		case CLIENTTAG_NOXQUEST_UINT:
			return CLIENTTAG_NOXQUEST;
		case CLIENTTAG_RENEGADE_UINT:
			return CLIENTTAG_RENEGADE;
		case CLIENTTAG_RENGDFDS_UINT:
			return CLIENTTAG_RENGDFDS;
		case CLIENTTAG_YURISREV_UINT:
			return CLIENTTAG_YURISREV;
		case CLIENTTAG_EMPERORBD_UINT:
			return CLIENTTAG_EMPERORBD;
		case CLIENTTAG_LOFLORE3_UINT:
			return CLIENTTAG_LOFLORE3;
		case CLIENTTAG_WWOL_UINT:
			return CLIENTTAG_WWOL;
		default:
			return CLIENTTAG_UNKNOWN;
		}
	}

	extern int tag_check_client(t_tag tag_uint)
	{
		switch (tag_uint)
		{
		case CLIENTTAG_BNCHATBOT_UINT:
		case CLIENTTAG_STARCRAFT_UINT:
		case CLIENTTAG_BROODWARS_UINT:
		case CLIENTTAG_SHAREWARE_UINT:
		case CLIENTTAG_DIABLORTL_UINT:
		case CLIENTTAG_DIABLOSHR_UINT:
		case CLIENTTAG_WARCIIBNE_UINT:
		case CLIENTTAG_DIABLO2DV_UINT:
		case CLIENTTAG_STARJAPAN_UINT:
		case CLIENTTAG_DIABLO2ST_UINT:
		case CLIENTTAG_DIABLO2XP_UINT:
		case CLIENTTAG_WARCRAFT3_UINT:
		case CLIENTTAG_WAR3XP_UINT:
		case CLIENTTAG_IIRC_UINT:
		case CLIENTTAG_WCHAT_UINT:
		case CLIENTTAG_TIBERNSUN_UINT:
		case CLIENTTAG_TIBSUNXP_UINT:
		case CLIENTTAG_REDALERT_UINT:
		case CLIENTTAG_REDALERT2_UINT:
		case CLIENTTAG_DUNE2000_UINT:
		case CLIENTTAG_NOX_UINT:
		case CLIENTTAG_NOXQUEST_UINT:
		case CLIENTTAG_RENEGADE_UINT:
		case CLIENTTAG_RENGDFDS_UINT:
		case CLIENTTAG_YURISREV_UINT:
		case CLIENTTAG_EMPERORBD_UINT:
		case CLIENTTAG_LOFLORE3_UINT:
		case CLIENTTAG_WWOL_UINT:
			return 1;
		default:
			return 0;
		}
	}

	extern char const * clienttag_get_title(t_clienttag clienttag)
	{
		switch (clienttag)
		{
		case CLIENTTAG_WAR3XP_UINT:
			return "Warcraft III Frozen Throne";
		case CLIENTTAG_WARCRAFT3_UINT:
			return "Warcraft III";
		case CLIENTTAG_DIABLO2XP_UINT:
			return "Diablo II Lord of Destruction";
		case CLIENTTAG_DIABLO2DV_UINT:
			return "Diablo II";
		case CLIENTTAG_STARJAPAN_UINT:
			return "Starcraft (Japan)";
		case CLIENTTAG_WARCIIBNE_UINT:
			return "Warcraft II";
		case CLIENTTAG_DIABLOSHR_UINT:
			return "Diablo I (Shareware)";
		case CLIENTTAG_DIABLORTL_UINT:
			return "Diablo I";
		case CLIENTTAG_SHAREWARE_UINT:
			return "Starcraft (Shareware)";
		case CLIENTTAG_BROODWARS_UINT:
			return "Starcraft: Brood War";
		case CLIENTTAG_STARCRAFT_UINT:
			return "Starcraft";
		case CLIENTTAG_BNCHATBOT_UINT:
			return "Chat";
		case CLIENTTAG_IIRC_UINT:
			return "Internet Relay Chat";
		case CLIENTTAG_WCHAT_UINT:
			return "Westwood Chat";
		case CLIENTTAG_TIBERNSUN_UINT:
			return "Tiberian Sun";
		case CLIENTTAG_TIBSUNXP_UINT:
			return "Tiberian Sun: Firestorm";
		case CLIENTTAG_REDALERT_UINT:
			return "Red Alert";
		case CLIENTTAG_REDALERT2_UINT:
			return "Red Alert 2";
		case CLIENTTAG_DUNE2000_UINT:
			return "Dune 2000";
		case CLIENTTAG_NOX_UINT:
			return "Nox";
		case CLIENTTAG_NOXQUEST_UINT:
			return "Nox Quest";
		case CLIENTTAG_RENEGADE_UINT:
			return "Renegade";
		case CLIENTTAG_RENGDFDS_UINT:
			return "Renegade Free Dedicated Server";
		case CLIENTTAG_YURISREV_UINT:
			return "Yuri's Revenge";
		case CLIENTTAG_EMPERORBD_UINT:
			return "Emepror: Battle for Dune";
		case CLIENTTAG_LOFLORE3_UINT:
			return "Lands of Lore 3";
		case CLIENTTAG_WWOL_UINT:
			return "Westwood Online";
		default:
			return "Unknown";
		}
	}

	extern int tag_check_in_list(t_clienttag clienttag, char const * list)
	{
		/* checks if a clienttag is in the list
		 * @clienttag : clienttag integer to check
		 * if it's allowed returns 0
		 * if it's not allowed returns -1
		 */
		char *p, *q;
		std::string tmp;

		/* by default allow all */
		if (!list)
			return 0;

		/* this shortcut check should make server as fast as before if
		 * the configuration is left in default mode */
		if (!strcasecmp(list, "all"))
			return 0;

		tmp = list;
		p = tmp.empty() ? nullptr : &tmp[0];
		do {
			q = std::strchr(p, ',');
			if (q)
				*q = '\0';
			if (!strcasecmp(p, "all"))
				return 0;
			if (std::strlen(p) != 4)
				continue;
			if (clienttag == tag_case_str_to_uint(p))
				return 0;		/* client is in list */
			if (q)
				p = q + 1;
		} while (q);

		return -1;			/* client is NOT in list */
	}

	/* Convert clienttag to uppercase and check it in valid client list
	*   Return NULL of tag not found
	*/
	extern t_clienttag tag_validate_client(char const * client)
	{
		t_clienttag clienttag;
		if (!client || strlen(client) != 4)
			return 0;

		// toupper
		clienttag = tag_case_str_to_uint(client);
		
		if (!tag_check_client(clienttag))
			return 0;

		return clienttag;
	}

} // namespace pvpgn
