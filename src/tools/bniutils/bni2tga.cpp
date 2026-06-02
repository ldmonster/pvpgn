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
#include <cstdlib>
#include <print>
#include <cstring>
#include <cerrno>

#include "fileio.h"
#include "bni.h"

#ifndef PVPGN_VERSION
#define PVPGN_VERSION "unknown"
#endif

#define BUFSIZE 1024

using namespace pvpgn::bni;

namespace
{

	void usage(char const * progname)
	{
		std::print(stderr,
			"usage: {} [<options>] [--] [<BNI file> [<TGA file>]]\n"
			"    -h, --help, --usage  show this information and exit\n"
			"    -v, --version        print version number and exit\n", progname);

		std::exit(EXIT_FAILURE);
	}

}

extern int main(int argc, char * argv[])
{
	char const *  bnifile = NULL;
	char const *  tgafile = NULL;
	std::FILE *        fbni;
	std::FILE *        ftga;
	int           a;
	int           forcefile = 0;
	char          dash[] = "-"; /* unique address used as flag */

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
	else if (forcefile && !tgafile)
		tgafile = argv[a];
	else if (std::strcmp(argv[a], "-") == 0 && !tgafile)
		tgafile = dash;
	else if (argv[a][0] != '-' && !tgafile)
		tgafile = argv[a];
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
	if (!tgafile)
		tgafile = dash;

	if (bnifile == dash)
		fbni = stdin;
	else
	if (!(fbni = std::fopen(bnifile, "r")))
	{
		std::println(stderr, "{}: could not open BNI file \"{}\" for reading (std::fopen: {})", argv[0], bnifile, std::strerror(errno));
		std::exit(EXIT_FAILURE);
	}
	if (tgafile == dash)
		ftga = stdout;
	else
	if (!(ftga = std::fopen(tgafile, "w")))
	{
		std::println(stderr, "{}: could not open TGA file \"{}\" for reading (std::fopen: {})", argv[0], tgafile, std::strerror(errno));
		std::exit(EXIT_FAILURE);
	}

	{
		unsigned char buf[BUFSIZE];
		std::size_t        rc;
		t_bnifile     bnih;
		bnih.unknown1 = file_readd_le(fbni);
		bnih.unknown2 = file_readd_le(fbni);
		bnih.numicons = file_readd_le(fbni);
		bnih.dataoffset = file_readd_le(fbni);
		std::println(stderr, "Info: numicons={} dataoffset=0x{:08x}({})", bnih.numicons, bnih.dataoffset, bnih.dataoffset);
		if (std::fseek(fbni, bnih.dataoffset, SEEK_SET)<0)
		{
			std::println(stderr, "{}: could not seek to offset {} in BNI file \"{}\" (std::fseek: {})", argv[0], bnih.dataoffset, bnifile, std::strerror(errno));
			return EXIT_FAILURE;
		}
		while ((rc = std::fread(buf, 1, sizeof(buf), fbni))>0) {
			if (std::fwrite(buf, rc, 1, ftga) < 1) {
				std::println(stderr, "{}: could not write data to TGA file \"{}\" (std::fwrite: {})", argv[0], tgafile, std::strerror(errno));
				return EXIT_FAILURE;
			}
		}
	}

	if (tgafile != dash && std::fclose(ftga) < 0)
		std::println(stderr, "{}: could not close TGA file \"{}\" after writing (std::fclose: {})", argv[0], tgafile, std::strerror(errno));
	if (bnifile != dash && std::fclose(fbni) < 0)
		std::println(stderr, "{}: could not close BNI file \"{}\" after reading (std::fclose: {})", argv[0], bnifile, std::strerror(errno));

	return EXIT_SUCCESS;
}
