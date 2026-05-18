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
#include "fileio.h"

#include <stack>

namespace pvpgn
{

	namespace bni
	{

		namespace
		{

			std::stack<std::FILE *> r_stack;
			std::stack<std::FILE *> w_stack;

		}

		/* ----------------------------------------------------------------- */

		extern void file_rpush(std::FILE *f) { r_stack.push(f); }
		extern void file_rpop(void) { r_stack.pop(); }
		extern void file_wpush(std::FILE *f) { w_stack.push(f); }
		extern void file_wpop(void) { w_stack.pop(); }

		/* ----------------------------------------------------------------- */

		extern std::uint8_t file_readb(void) {
			unsigned char buff[1];
			std::FILE *f = r_stack.top();
			if (std::fread(buff, 1, sizeof(buff), f) < sizeof(buff)) {
				if (std::ferror(f)) std::perror("file_readb: std::fread");
				return 0;
			}
			return static_cast<std::uint8_t>(buff[0]);
		}


		extern std::uint16_t file_readw_le(void) {
			unsigned char buff[2];
			std::FILE *f = r_stack.top();
			if (std::fread(buff, 1, sizeof(buff), f) < sizeof(buff)) {
				if (std::ferror(f)) std::perror("file_readw_le: std::fread");
				return 0;
			}
			return static_cast<std::uint16_t>(
				static_cast<std::uint16_t>(buff[0]) |
				static_cast<std::uint16_t>(static_cast<std::uint16_t>(buff[1]) << 8));
		}


		extern std::uint16_t file_readw_be(void) {
			unsigned char buff[2];
			std::FILE *f = r_stack.top();
			if (std::fread(buff, 1, sizeof(buff), f) < sizeof(buff)) {
				if (std::ferror(f)) std::perror("file_readw_be: std::fread");
				return 0;
			}
			return static_cast<std::uint16_t>(
				static_cast<std::uint16_t>(static_cast<std::uint16_t>(buff[0]) << 8) |
				static_cast<std::uint16_t>(buff[1]));
		}


		extern std::uint32_t file_readd_le(void) {
			unsigned char buff[4];
			std::FILE *f = r_stack.top();
			if (std::fread(buff, 1, sizeof(buff), f) < sizeof(buff)) {
				if (std::ferror(f)) std::perror("file_readd_le: std::fread");
				return 0;
			}
			return static_cast<std::uint32_t>(buff[0]) |
				(static_cast<std::uint32_t>(buff[1]) << 8) |
				(static_cast<std::uint32_t>(buff[2]) << 16) |
				(static_cast<std::uint32_t>(buff[3]) << 24);
		}


		extern std::uint32_t file_readd_be(void) {
			unsigned char buff[4];
			std::FILE *f = r_stack.top();
			if (std::fread(buff, 1, sizeof(buff), f) < sizeof(buff)) {
				if (std::ferror(f)) std::perror("file_readd_be: std::fread");
				return 0;
			}
			return (static_cast<std::uint32_t>(buff[0]) << 24) |
				(static_cast<std::uint32_t>(buff[1]) << 16) |
				(static_cast<std::uint32_t>(buff[2]) << 8) |
				static_cast<std::uint32_t>(buff[3]);
		}


		extern int file_writeb(std::uint8_t u) {
			unsigned char buff[1];
			std::FILE *f = w_stack.top();
			buff[0] = static_cast<unsigned char>(u);
			if (std::fwrite(buff, 1, sizeof(buff), f) < sizeof(buff)) {
				if (std::ferror(f)) std::perror("file_writeb: std::fwrite");
				return -1;
			}
			return 0;
		}


		extern int file_writew_le(std::uint16_t u) {
			unsigned char buff[2];
			std::FILE *f = w_stack.top();
			buff[0] = static_cast<unsigned char>(u);
			buff[1] = static_cast<unsigned char>(u >> 8);
			if (std::fwrite(buff, 1, sizeof(buff), f) < sizeof(buff)) {
				if (std::ferror(f)) std::perror("file_writew_le: std::fwrite");
				return -1;
			}
			return 0;
		}


		extern int file_writew_be(std::uint16_t u) {
			unsigned char buff[2];
			std::FILE *f = w_stack.top();
			buff[0] = static_cast<unsigned char>(u >> 8);
			buff[1] = static_cast<unsigned char>(u);
			if (std::fwrite(buff, 1, sizeof(buff), f) < sizeof(buff)) {
				if (std::ferror(f)) std::perror("file_writew_be: std::fwrite");
				return -1;
			}
			return 0;
		}


		extern int file_writed_le(std::uint32_t u) {
			unsigned char buff[4];
			std::FILE *f = w_stack.top();
			buff[0] = static_cast<unsigned char>(u);
			buff[1] = static_cast<unsigned char>(u >> 8);
			buff[2] = static_cast<unsigned char>(u >> 16);
			buff[3] = static_cast<unsigned char>(u >> 24);
			if (std::fwrite(buff, 1, sizeof(buff), f) < sizeof(buff)) {
				if (std::ferror(f)) std::perror("file_writed_le: std::fwrite");
				return -1;
			}
			return 0;
		}


		extern int file_writed_be(std::uint32_t u) {
			unsigned char buff[4];
			std::FILE *f = w_stack.top();
			buff[0] = static_cast<unsigned char>(u >> 24);
			buff[1] = static_cast<unsigned char>(u >> 16);
			buff[2] = static_cast<unsigned char>(u >> 8);
			buff[3] = static_cast<unsigned char>(u);
			if (std::fwrite(buff, 1, sizeof(buff), f) < sizeof(buff)) {
				if (std::ferror(f)) std::perror("file_writed_be: std::fwrite");
				return -1;
			}
			return 0;
		}

	}

}
