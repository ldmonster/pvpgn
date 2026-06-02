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
/* setup_before.h dropped: standalone v3 tool */
#include <cstdlib>
#include <print>
#include <cstring>
#include <cerrno>

/* PVPGN_VERSION supplied via target_compile_definitions */
#include "tga.h"
#include "fileio.h"
#ifndef PVPGN_VERSION
#define PVPGN_VERSION "unknown"
#endif

using namespace pvpgn::bni;

namespace
{

	void usage(char const * progname)
	{
		std::print(stderr,
			"usage: {} [<options>] [--] [<BNI file>]\n"
			"    -h, --help, --usage  show this information and exit\n"
			"    -v, --version        print version number and exit\n", progname);

		std::exit(EXIT_FAILURE);
	}

}

extern int main(int argc, char * argv[])
{
	char const * bnifile = NULL;
	std::FILE *       fp;
	int          a;
	int          forcefile = 0;
	char         dash[] = "-"; /* unique address used as flag */

	if (argc < 1 || !argv || !argv[0])
	{
		std::println(stderr, "bad arguments");
		return EXIT_FAILURE;
	}

	for (a = 1; a < argc; a++)
	if (forcefile && !bnifile)
		bnifile = argv[a];
	else if (std::strcmp(argv[a], "-") == 0 && !bnifile)
		bnifile = dash;
	else if (argv[a][0] != '-' && !bnifile)
		bnifile = argv[a];
	else if (forcefile || argv[a][0] != '-' || std::strcmp(argv[a], "-") == 0)
	{
		std::println(stderr, "{}: extra file argument \"{}\"", argv[0], argv[a]);
		usage(argv[0]);
	}
	else if (std::strcmp(argv[a], "--") == 0)
		forcefile = 1;
	else if (std::strcmp(argv[a], "-v") == 0 || std::strcmp(argv[a], "--version") == 0)
	{
		std::print("version " PVPGN_VERSION "\n");
		return EXIT_SUCCESS;
	}
	else if (std::strcmp(argv[a], "-h") == 0 || std::strcmp(argv[a], "--help") == 0 || std::strcmp(argv[a], "--usage")
		== 0)
		usage(argv[0]);
	else
	{
		std::println(stderr, "{}: unknown option \"{}\"", argv[0], argv[a]);
		usage(argv[0]);
	}

	if (!bnifile)
		bnifile = dash;

	if (bnifile == dash)
		fp = stdin;
	else
	if (!(fp = std::fopen(bnifile, "r")))
	{
		std::println(stderr, "{}: could not open BNI file \"{}\" for reading (std::fopen: {})", argv[0], bnifile, std::strerror(errno));
		std::exit(EXIT_FAILURE);
	}

	{
		t_tgaimg * tgaimg;
		int        i;
		int        bniid, unknown, icons, datastart;
		int        expected_width, expected_height;
		bniid = static_cast<int>(file_readd_le(fp));
		unknown = static_cast<int>(file_readd_le(fp));
		icons = static_cast<int>(file_readd_le(fp));
		datastart = static_cast<int>(file_readd_le(fp));
		std::println(stderr, "BNIHeader: id=0x{:08x} unknown=0x{:08x} icons=0x{:08x} datastart=0x{:08x}", bniid, unknown, icons, datastart);
		expected_width = 0;
		expected_height = 0;
		for (i = 0; i < icons; i++) {
			int id, x, y, flags, tag;
			id = static_cast<int>(file_readd_le(fp));
			x = static_cast<int>(file_readd_le(fp));
			y = static_cast<int>(file_readd_le(fp));
			if (id == 0) {
				tag = static_cast<int>(file_readd_le(fp));
			}
			else {
				tag = 0;
			}
			flags = static_cast<int>(file_readd_le(fp));
			std::println(stderr, "Icon[{}]: id=0x{:08x} x={} y={} tag=0x{:08x}(\"{}{}{}{}\") flags=0x{:08x}", i, id, x, y, tag,
				static_cast<unsigned char>((tag >> 24) & 0xff),
				static_cast<unsigned char>((tag >> 16) & 0xff),
				static_cast<unsigned char>((tag >> 8) & 0xff),
				static_cast<unsigned char>(tag & 0xff), flags);
			if (x > expected_width) expected_width = x;
			expected_height += y;
		}
		if (std::ftell(fp) != datastart) {
			std::println(stderr, "Warning: garbage after header (pos=0x{:x}-datastart=0x{:x}) = {} bytes of garbage! ", static_cast<unsigned long>(std::ftell(fp)), static_cast<unsigned long>(datastart), static_cast<long>(std::ftell(fp) - datastart));
		}
		tgaimg = load_tgaheader(fp);
		print_tga_info(tgaimg, stdout);
		std::println(stderr, "");
		std::println(stderr, "Check: Expected {}x{} TGA, got {}x{}. {}", expected_width, expected_height, tgaimg->width, tgaimg->height, ((tgaimg->width == expected_width) && (tgaimg->height == expected_height)) ? "OK." : "FAIL.");
		std::println(stderr, "Check: Expected 24bit color depth TGA, got {}bit. {}", tgaimg->bpp, (tgaimg->bpp == 24) ? "OK." : "FAIL.");
		std::println(stderr, "Check: Expected ImageType 10, got {}. {}", tgaimg->imgtype, (tgaimg->imgtype == 10) ? "OK." : "FAIL.");
	}

	if (bnifile != dash && std::fclose(fp) < 0)
		std::println(stderr, "{}: could not close BNI file \"{}\" after reading (std::fclose: {})", argv[0], bnifile, std::strerror(errno));
	return EXIT_SUCCESS;
}
