/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
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
// File I/O utility: buffered line reader with continuation support.

#include "util.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pvpgn
{

#define DEF_LEN 64
#define INC_LEN 16

	extern char * file_get_line(std::FILE * fp)
	{
		static char *       line = NULL;
		static unsigned int	len = 0;
		unsigned int 	pos = 0;
		int          	prev_char, curr_char;

		// use file_get_line with NULL argument to clear the buffer
		if (!(fp))
		{
			len = 0;
			if ((line))
				delete[] line;
			line = NULL;
			return NULL;
		}

		if (!(line))
		{
			line = new char[DEF_LEN]{};
			len = DEF_LEN;
		}

		prev_char = '\0';
		while ((curr_char = std::fgetc(fp)) != EOF)
		{
			if (static_cast<char>(curr_char) == '\r')
				continue; /* make DOS line endings look Unix-like */
			if (static_cast<char>(curr_char) == '\n')
			{
				if (pos < 1 || static_cast<char>(prev_char) != '\\')
					break;
				pos--; /* throw away the backslash */
				prev_char = '\0';
				continue;
			}
			prev_char = curr_char;

			line[pos++] = static_cast<char>(curr_char);
			if ((pos + 1) >= len)
			{
				unsigned int newlen = len + INC_LEN;
				char* newline = new char[newlen]{};
				std::memcpy(newline, line, len);
				delete[] line;
				line = newline;
				len = newlen;
			}
		}

		if (curr_char == EOF && pos < 1) /* not even an empty line */
		{
			return NULL;
		}

		line[pos] = '\0';

		return line;
	}

#undef DEF_LEN
#undef INC_LEN

} // namespace pvpgn
