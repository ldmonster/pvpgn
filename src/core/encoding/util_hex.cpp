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
// Hex encoding/decoding utilities.
// Included as a sub-TU by util.cpp — do not compile directly.

#include <cstdio>

namespace pvpgn
{

	extern void str_to_hex(char * target, char const * data, int datalen)
	{
		unsigned char c;
		int  i;
		for (i = 0; i < datalen; i++)
		{
			c = static_cast<unsigned char>(data[i] & 0xff);

			std::sprintf(target + i * 3, "%02X ", c);
			target[i * 3 + 3] = '\0';

			/* std::fprintf(stderr, "str_to_hex %d | '%02x' '%s'\n", i, c, target); */
		}
	}


	extern int hex_to_str(char const * source, char * data, int datalen)
	{
		/*
		 * TODO: We really need a more robust function here,
		 *       for now, I'll just use this hack for a quick evaluation
		 */
		char byte;
		char c;
		int  i;

		for (i = 0; i < datalen; i++)
		{
			byte = 0;

			/* std::fprintf(stderr, "hex_to_str %d | '%02x'", i, byte); */

			c = source[i * 3 + 0];
			byte = static_cast<char>(byte + 16 * (c > '9' ? (c - 'A' + 10) : (c - '0')));

			/* std::fprintf(stderr, " | '%c' '%02x'", c, byte); */

			c = source[i * 3 + 1];
			byte = static_cast<char>(byte + 1 * (c > '9' ? (c - 'A' + 10) : (c - '0')));

			/* std::fprintf(stderr, " | '%c' '%02x'", c, byte); */

			/* std::fprintf(stderr, "\n"); */

			data[i] = byte;
		}

		/* std::fprintf(stderr, "finished, returning %d from '%s'\n", i, source);  */

		return i;
	}

} // namespace pvpgn
