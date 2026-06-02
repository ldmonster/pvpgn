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
// Core 4-byte tag encoding/decoding and architecture tag validation (plan 15 §3 / SOLID-S).
// Split from tag.cpp (Plan 02 full-split: one TU per file).

#include "tag.h"
#include "xstring.h"

#include <cstring>
#include <cctype>

#include "core/format.hpp"

namespace pvpgn
{

	/*****/
	/* make all letters in string upper case - used in command.cpp*/
	extern t_tag tag_case_str_to_uint(char const * tag_str)
	{
		unsigned int i, len;
		char temp_str[5];

		len = static_cast<unsigned int>(std::strlen(tag_str));
		if (len != 4)
			LOG_WARN(__FUNCTION__, "got unusual sized clienttag '{}'", tag_str);

		for (i = 0; i < len && i < 4; i++)
			temp_str[i] = static_cast<char>(safe_toupper(tag_str[i]));

		temp_str[4] = '\0';

		return tag_str_to_uint(temp_str);
	}

	extern t_tag tag_str_to_uint(char const * tag_str)
	{
		t_tag	tag_uint;

		if (!tag_str) {
			LOG_ERROR(__FUNCTION__, "got NULL tag");
			return 0; /* unknown */
		}

		tag_uint  = static_cast<t_tag>(static_cast<unsigned char>(tag_str[0])) << 24;
		tag_uint |= static_cast<t_tag>(static_cast<unsigned char>(tag_str[1])) << 16;
		tag_uint |= static_cast<t_tag>(static_cast<unsigned char>(tag_str[2])) << 8;
		tag_uint |= static_cast<t_tag>(static_cast<unsigned char>(tag_str[3]));

		return tag_uint;
	}

	/* tag_uint_to_str()
	 *
	 * from calling function:
	 *
	 *    char tag_str[5]; // define first, then send into fuction
	 *    tag_uint_to_str(tag_str, tag_uint); // returns pointer to tag_str
	 *
	 * Nothing to malloc, nothing to free
	 */
	extern const char * tag_uint_to_str(char * tag_str, t_tag tag_uint)
	{
		if (!tag_uint) /* return "UNKN" if tag_uint = 0 */
			return TAG_UNKNOWN;

		tag_str[0] = static_cast<char>((tag_uint >> 24) & 0xff);
		tag_str[1] = static_cast<char>((tag_uint >> 16) & 0xff);
		tag_str[2] = static_cast<char>((tag_uint >> 8) & 0xff);
		tag_str[3] = static_cast<char>((tag_uint) & 0xff);
		tag_str[4] = '\0';
		return tag_str;
	}

	extern std::string tag_uint_to_str2(t_tag tag_uint)
	{
		if (!tag_uint) /* return "UNKN" if tag_uint = 0 */
			return std::string(TAG_UNKNOWN);

		char tag_str[5] = {};
		tag_str[0] = static_cast<char>((tag_uint >> 24) & 0xff);
		tag_str[1] = static_cast<char>((tag_uint >> 16) & 0xff);
		tag_str[2] = static_cast<char>((tag_uint >> 8) & 0xff);
		tag_str[3] = static_cast<char>((tag_uint) & 0xff);
		tag_str[4] = '\0';
		return std::string(tag_str);
	}

	extern const char * tag_uint_to_revstr(char * tag_str, t_tag tag_uint)
	{
		if (!tag_uint) /* return "UNKN" if tag_uint = 0 */
			return TAG_UNKNOWN;

		tag_str[0] = static_cast<char>((tag_uint) & 0xff);
		tag_str[1] = static_cast<char>((tag_uint >> 8) & 0xff);
		tag_str[2] = static_cast<char>((tag_uint >> 16) & 0xff);
		tag_str[3] = static_cast<char>((tag_uint >> 24) & 0xff);
		tag_str[4] = '\0';
		return tag_str;
	}

	extern int tag_check_arch(t_tag tag_uint)
	{
		switch (tag_uint)
		{
		case ARCHTAG_WINX86_UINT:
		case ARCHTAG_MACPPC_UINT:
		case ARCHTAG_OSXPPC_UINT:
			return 1;
		default:
			return 0;
		}
	}

} // namespace pvpgn
