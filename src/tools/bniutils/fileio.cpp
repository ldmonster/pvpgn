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

#include <array>
#include <cstdio>

#include "core/bytes.hpp"
#include "core/endian.hpp"

namespace pvpgn
{

	namespace bni
	{

		/* ----------------------------------------------------------------- *
		 * Stateless byte / little- / big-endian readers and writers.
		 *
		 * Byte-order conversion is delegated to `pvpgn::core::read_le` /
		 * `read_be` / `write_le` / `write_be` so the byte-fiddling lives
		 * in exactly one place.
		 * ----------------------------------------------------------------- */

		namespace
		{

			template <class T>
			T read_le_from_file(std::FILE *f, char const *who) {
				std::array<std::byte, sizeof(T)> buff{};
				if (std::fread(buff.data(), 1, buff.size(), f) < buff.size()) {
					if (std::ferror(f)) std::perror(who);
					return 0;
				}
				auto r = core::read_le<T>(core::ByteView{buff.data(), buff.size()});
				return r ? r.value() : T{0};
			}

			template <class T>
			T read_be_from_file(std::FILE *f, char const *who) {
				std::array<std::byte, sizeof(T)> buff{};
				if (std::fread(buff.data(), 1, buff.size(), f) < buff.size()) {
					if (std::ferror(f)) std::perror(who);
					return 0;
				}
				auto r = core::read_be<T>(core::ByteView{buff.data(), buff.size()});
				return r ? r.value() : T{0};
			}

			template <class T>
			int write_le_to_file(std::FILE *f, T v, char const *who) {
				std::array<std::byte, sizeof(T)> buff{};
				(void)core::write_le<T>(core::ByteSpan{buff.data(), buff.size()}, v);
				if (std::fwrite(buff.data(), 1, buff.size(), f) < buff.size()) {
					if (std::ferror(f)) std::perror(who);
					return -1;
				}
				return 0;
			}

			template <class T>
			int write_be_to_file(std::FILE *f, T v, char const *who) {
				std::array<std::byte, sizeof(T)> buff{};
				(void)core::write_be<T>(core::ByteSpan{buff.data(), buff.size()}, v);
				if (std::fwrite(buff.data(), 1, buff.size(), f) < buff.size()) {
					if (std::ferror(f)) std::perror(who);
					return -1;
				}
				return 0;
			}

		} // namespace

		extern std::uint8_t file_readb(std::FILE *f) {
			return read_le_from_file<std::uint8_t>(f, "file_readb: std::fread");
		}

		extern std::uint16_t file_readw_le(std::FILE *f) {
			return read_le_from_file<std::uint16_t>(f, "file_readw_le: std::fread");
		}

		extern std::uint16_t file_readw_be(std::FILE *f) {
			return read_be_from_file<std::uint16_t>(f, "file_readw_be: std::fread");
		}

		extern std::uint32_t file_readd_le(std::FILE *f) {
			return read_le_from_file<std::uint32_t>(f, "file_readd_le: std::fread");
		}

		extern std::uint32_t file_readd_be(std::FILE *f) {
			return read_be_from_file<std::uint32_t>(f, "file_readd_be: std::fread");
		}

		extern int file_writeb(std::FILE *f, std::uint8_t u) {
			return write_le_to_file<std::uint8_t>(f, u, "file_writeb: std::fwrite");
		}

		extern int file_writew_le(std::FILE *f, std::uint16_t u) {
			return write_le_to_file<std::uint16_t>(f, u, "file_writew_le: std::fwrite");
		}

		extern int file_writew_be(std::FILE *f, std::uint16_t u) {
			return write_be_to_file<std::uint16_t>(f, u, "file_writew_be: std::fwrite");
		}

		extern int file_writed_le(std::FILE *f, std::uint32_t u) {
			return write_le_to_file<std::uint32_t>(f, u, "file_writed_le: std::fwrite");
		}

		extern int file_writed_be(std::FILE *f, std::uint32_t u) {
			return write_be_to_file<std::uint32_t>(f, u, "file_writed_be: std::fwrite");
		}

	}

}
