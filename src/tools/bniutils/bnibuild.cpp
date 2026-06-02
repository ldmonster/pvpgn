/*
	Copyright (C) 2000  Marco Ziech
	Copyright (C) 2000  Ross Combs (rocombs@cs.nmsu.edu)

	This program is xfree software; you can redistribute it and/or modify
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

#include <cerrno>
#include <print>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <string>

#include "fileio.h"
#include "tga.h"
#include "bni.h"
/* setup_after.h dropped */

#ifndef PVPGN_VERSION
#define PVPGN_VERSION "unknown"
#endif

#define BUFSIZE 1024

using namespace pvpgn;
using namespace pvpgn::bni;

namespace
{

	int read_list(char const * progname, t_bnifile * bnifile, char const * name) {
		std::FILE * f;
		char   line[BUFSIZE];

		f = std::fopen(name, "r");
		if (f == NULL) {
			std::println(stderr, "{}: could not open index file \"{}\" for reading (std::fopen: {})", progname, name, std::strerror(errno));
			return -1;
		}
		bnifile->unknown1 = 0x00000010; /* in case they are not set */
		bnifile->unknown2 = 0x00000001;
		bnifile->numicons = 0;
		bnifile->dataoffset = 16; /* size of header */
		bnifile->icons.clear();
		while (std::fgets(line, sizeof(line), f)) {
			char cmd[BUFSIZE];
			std::sscanf(line, "%s", cmd);
			if (std::strcmp(cmd, "unknown1") == 0) {
				std::sscanf(line, "unknown1 %08x", &bnifile->unknown1);
			}
			else if (std::strcmp(cmd, "unknown2") == 0) {
				std::sscanf(line, "unknown2 %08x", &bnifile->unknown2);
			}
			else if (std::strcmp(cmd, "icon") == 0) {
				char c;
				std::sscanf(line, "icon %c", &c);
				if (c == '!') {
					unsigned char tg[4];
					int tag;
					unsigned int x, y, unknown;
					std::sscanf(line, "icon !%c%c%c%c %u %u %08x", &tg[0], &tg[1], &tg[2], &tg[3], &x, &y, &unknown);
					tag = tg[3] + (tg[2] << 8) + (tg[1] << 16) + (tg[0] << 24);
				std::println(stderr, "Icon[{}]: id=0x{:x} x={} y={} unknown=0x{:x} tag=\"{}{}{}{}\"", bnifile->numicons, 0, x, y, unknown, static_cast<unsigned char>((tag >> 24) & 0xff), static_cast<unsigned char>((tag >> 16) & 0xff), static_cast<unsigned char>((tag >> 8) & 0xff), static_cast<unsigned char>(tag & 0xff));
				bnifile->icons.push_back(t_bniicon{0, x, y, static_cast<unsigned int>(tag), unknown});
					bnifile->numicons++;
					bnifile->dataoffset += 20;
				}
				else if (c == '#') {
					unsigned int id, x, y, unknown;
					std::sscanf(line, "icon #%08x %u %u %08x", &id, &x, &y, &unknown);
					std::println(stderr, "Icon[{}]: id=0x{:x} x={} y={} unknown=0x{:x} tag=0x00000000", bnifile->numicons, id, x, y, unknown);
					bnifile->icons.push_back(t_bniicon{id, x, y, 0, unknown});
					bnifile->numicons++;
					bnifile->dataoffset += 16;
				}
				else
					std::println(stderr, "Bad character '{}' in icon specifier for icon {} in index file \"{}\"", c, bnifile->numicons + 1, name);
			}
			else
				std::println(stderr, "Unknown command \"{}\" in index file \"{}\"", cmd, name);
		}
		if (std::fclose(f) < 0)
			std::println(stderr, "{}: could not close index file \"{}\" after reading (std::fclose: {})", progname, name, std::strerror(errno));
		return 0;
	}


	std::string geticonfilename(t_bnifile *bnifile, char const * indir, int i) {
		char buf[1024];
		if (bnifile->icons[static_cast<std::size_t>(i)].id == 0) {
			unsigned int tag = bnifile->icons[static_cast<std::size_t>(i)].tag;
			std::snprintf(buf, sizeof(buf), "%s/%c%c%c%c.tga", indir,
				static_cast<unsigned char>((tag >> 24) & 0xff),
				static_cast<unsigned char>((tag >> 16) & 0xff),
				static_cast<unsigned char>((tag >> 8) & 0xff),
				static_cast<unsigned char>(tag & 0xff));
		}
		else {
			std::snprintf(buf, sizeof(buf), "%s/%08x.tga", indir, bnifile->icons[static_cast<std::size_t>(i)].id);
		}
		return std::string(buf);
	}


	int img2area(t_tgaimg *dst, t_tgaimg *src, int x, int y) {
		unsigned char *sdp;
		unsigned char *ddp;
		int pixelsize;
		int i;

		pixelsize = getpixelsize(dst);
		if (getpixelsize(src) != pixelsize) {
			std::println(stderr, "Error: source pixelsize is {} should be {}!", getpixelsize(src), pixelsize);
			return -1;
		}
		if (src->width + x > dst->width) return -1;
		if (src->height + y > dst->height) return -1;
		sdp = src->data.data();
		ddp = dst->data.data() + (y * dst->width * pixelsize);
		for (i = 0; i < src->height; i++) {
			ddp += x*pixelsize;
			std::memcpy(ddp, sdp, static_cast<std::size_t>(src->width)*static_cast<std::size_t>(pixelsize));
			sdp += src->width*pixelsize;
			ddp += (dst->width - x)*pixelsize;
		}
		return 0;
	}


	void usage(char const * progname)
	{
		std::print(stderr,
			"usage: {} [<options>] [--] <input directory> [<BNI file>]\n"
			"    -h, --help, --usage  show this information and exit\n"
			"    -v, --version        print version number and exit\n", progname);

		std::exit(EXIT_FAILURE);
	}

}

extern int main(int argc, char * argv[])
{
	char const * indir = NULL;
	char const * bnifile = NULL;
	std::FILE *       fbni;
	int          a;
	int          forcefile = 0;
	char         dash[] = "-"; /* unique address used as flag */

	if (argc < 1 || !argv || !argv[0])
	{
		std::println(stderr, "bad arguments");
		return EXIT_FAILURE;
	}

	for (a = 1; a < argc; a++)
	if (forcefile && !indir)
		indir = argv[a];
	else if (std::strcmp(argv[a], "-") == 0 && !indir)
		indir = dash;
	else if (argv[a][0] != '-' && !indir)
		indir = argv[a];
	else if (forcefile && !bnifile)
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

	if (!indir)
	{
		std::println(stderr, "{}: input directory not specified", argv[0]);
		usage(argv[0]);
	}
	if (!bnifile)
		bnifile = dash;

	if (indir == dash)
	{
		std::println(stderr, "{}: can not read directory from <stdin>", argv[0]);
		return EXIT_FAILURE;
	}
	{
		std::error_code ec;
		auto status = std::filesystem::status(indir, ec);
		if (ec) {
			std::println(stderr, "{}: could not stat input directory \"{}\" ({})",
				argv[0], indir, ec.message().c_str());
			return EXIT_FAILURE;
		}
		if (!std::filesystem::is_directory(status)) {
			std::println(stderr, "{}: \"{}\" is not a directory", argv[0], indir);
			return -1;
		}
	}

	if (bnifile == dash)
		fbni = stdout;
	else
	if (!(fbni = std::fopen(bnifile, "w")))
	{
		std::println(stderr, "{}: could not open BNI file \"{}\" for writing (std::fopen: {})", argv[0], bnifile, std::strerror(errno));
		return EXIT_FAILURE;
	}

	{
		unsigned int i;
		unsigned int yline;
		t_tgaimg *   img;
		t_bnifile    bni;

		std::string listfilename = std::string(indir) + "/bniindex.lst";
		std::println(stderr, "Info: Reading index from file \"{}\"...", listfilename.c_str());
		if (read_list(argv[0], &bni, listfilename.c_str()) < 0)
			return EXIT_FAILURE;
		std::println(stderr, "BNIHeader: unknown1={} unknown2={} numicons={} dataoffset={}", bni.unknown1, bni.unknown2, bni.numicons, bni.dataoffset);
		if (write_bni(fbni, &bni) < 0) {
			std::println(stderr, "Error: Failed to write BNI header.");
			return EXIT_FAILURE;
		}
		img = new_tgaimg(0, 0, 24, tgaimgtype_rlecompressed_truecolor);
		for (i = 0; i < bni.numicons; i++) {
			if (bni.icons[static_cast<std::size_t>(i)].x > img->width) img->width = static_cast<std::uint16_t>(bni.icons[static_cast<std::size_t>(i)].x);
			img->height = static_cast<std::uint16_t>(img->height + bni.icons[static_cast<std::size_t>(i)].y);
		}
		std::println(stderr, "Info: Creating TGA with {}x{}x{}bpp.", img->width, img->height, img->bpp);
		img->data.resize(static_cast<std::size_t>(img->width)*static_cast<std::size_t>(img->height)*static_cast<std::size_t>(getpixelsize(img)));
		yline = 0;
		for (i = 0; i < bni.numicons; i++) {
			t_tgaimg *icon;
			std::FILE *f;
			std::string name = geticonfilename(&bni, indir, static_cast<int>(i));
			f = std::fopen(name.c_str(), "r");
			if (f == NULL) {
				std::perror("std::fopen");
				return EXIT_FAILURE;
			}
			icon = load_tga(f);
			if (std::fclose(f) < 0)
				std::println(stderr, "Error: could not close TGA file \"{}\" after reading (std::fclose: {})", name.c_str(), std::strerror(errno));
			if (icon == NULL) {
				std::println(stderr, "Error: load_tga failed with data from TGA file \"{}\"", name.c_str());
				return EXIT_FAILURE;
			}
			if (img2area(img, icon, 0, static_cast<int>(yline)) < 0) {
				std::println(stderr, "Error: inserting icon from TGA file \"{}\" into big TGA failed", name.c_str());
				return EXIT_FAILURE;
			}
			yline += icon->height;
			destroy_img(icon);
		}
		if (write_tga(fbni, img) < 0) {
			std::println(stderr, "Error: Failed to write TGA to BNI file.");
			return EXIT_FAILURE;
		}
		if (bnifile != dash && std::fclose(fbni) < 0) {
			std::println(stderr, "{}: could not close BNI file \"{}\" after writing (std::fclose: {})", argv[0], bnifile, std::strerror(errno));
			return EXIT_FAILURE;
		}
	}
	std::println(stderr, "Info: Writing to \"{}\" finished.", bnifile);
	return EXIT_SUCCESS;
}
