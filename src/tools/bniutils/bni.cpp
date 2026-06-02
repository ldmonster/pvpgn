/*
	Copyright (C) 2000  Marco Ziech (mmz@gmx.net)
	Copyright (C) 2000  Ross Combs (rocombs@cs.nmsu.edu)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
	*/
#include "bni.h"

#include <cstdio>
#include <print>

#include "fileio.h"

namespace pvpgn
{

	namespace bni
	{

		/* ----------------------------------------------------------------- *
		 * BNI header reader / writer.
		 *
		 * The previous implementation used a fixed-size
		 * `t_bniicon icon[BNI_MAXICONS]` array tunneled through a
		 * separate `bni_iconlist_struct` and a global `std::stack<FILE*>`
		 * for I/O context.  Both are gone: the icon list now lives in
		 * `std::vector<t_bniicon>` and every read/write call takes the
		 * target `FILE *` explicitly.
		 * ----------------------------------------------------------------- */

		extern t_bnifile * load_bni(std::FILE *f) {
			if (f == NULL) return NULL;
			auto *b = new t_bnifile{};

			b->unknown1 = file_readd_le(f);
			if (b->unknown1 != 0x00000010)
				std::println(stderr, "load_bni: field 1 is not 0x00000010. Data may be invalid!");
			b->unknown2 = file_readd_le(f);
			if (b->unknown2 != 0x00000001)
				std::println(stderr, "load_bni: field 2 is not 0x00000001. Data may be invalid!");
			b->numicons = file_readd_le(f);
			b->dataoffset = file_readd_le(f);
			if (b->numicons < 1) {
				std::println(stderr, "load_bni: strange, no icons present in BNI file");
			}
			b->icons.resize(b->numicons);
			for (unsigned int i = 0; i < b->numicons; i++) {
				b->icons[i].id = file_readd_le(f);
				b->icons[i].x = file_readd_le(f);
				b->icons[i].y = file_readd_le(f);
				if (b->icons[i].id == 0) {
					b->icons[i].tag = file_readd_le(f);
				}
				else {
					b->icons[i].tag = 0;
				}
				b->icons[i].unknown = file_readd_le(f);
			}
			if (std::ftell(f) != static_cast<long>(b->dataoffset))
				std::println(stderr, "load_bni: Warning, {} bytes of garbage after BNI header",
					static_cast<unsigned long>(b->dataoffset - std::ftell(f)));
			return b;
		}

		extern int write_bni(std::FILE *f, t_bnifile *b) {
			if (f == NULL) return -1;
			if (b == NULL) return -1;
			file_writed_le(f, b->unknown1);
			if (b->unknown1 != 0x00000010)
				std::println(stderr, "write_bni: field 1 is not 0x00000010. Data may be invalid!");
			file_writed_le(f, b->unknown2);
			if (b->unknown2 != 0x00000001)
				std::println(stderr, "write_bni: field 2 is not 0x00000001. Data may be invalid!");
			file_writed_le(f, b->numicons);
			file_writed_le(f, b->dataoffset);
			for (unsigned int i = 0; i < b->numicons; i++) {
				file_writed_le(f, b->icons[i].id);
				file_writed_le(f, b->icons[i].x);
				file_writed_le(f, b->icons[i].y);
				if (b->icons[i].id == 0) {
					file_writed_le(f, b->icons[i].tag);
				}
				file_writed_le(f, b->icons[i].unknown);
			}
			if (std::ftell(f) != static_cast<long>(b->dataoffset))
				std::println(stderr, "Warning: dataoffset is incorrect! (=0x{:x} should be 0x{:x})",
					static_cast<unsigned long>(b->dataoffset),
					static_cast<unsigned long>(std::ftell(f)));
			return 0;
		}

		extern void destroy_bni(t_bnifile *b) {
			delete b;
		}

	}

}
