/*
	Copyright (C) 2000  Marco Ziech (mmz@gmx.net)
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
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <string>

#ifndef PVPGN_VERSION
#define PVPGN_VERSION "unknown"
#endif
#include "fileio.h"
#include "tga.h"
#include "bni.h"
/* setup_after.h dropped */

using namespace pvpgn::bni;
using namespace pvpgn;

namespace
{

	/* extract a portion of an image creating a new image */
	t_tgaimg * area2img(t_tgaimg *src, int x, int y, int width, int height, t_tgaimgtype type) {
		t_tgaimg *dst;
		int pixelsize;
		unsigned char *datap;
		unsigned char *destp;
		int i;

		if (src == NULL) return NULL;
		if ((x + width) > src->width) return NULL;
		if ((y + height) > src->height) return NULL;
		pixelsize = getpixelsize(src);
		if (pixelsize == 0) return NULL;

		dst = new_tgaimg(width, height, src->bpp, type);
		dst->data.resize(static_cast<std::size_t>(width)*height*pixelsize);

		datap = src->data.data();
		datap += y*src->width*pixelsize;
		destp = dst->data.data();
		for (i = 0; i < height; i++) {
			datap += x*pixelsize;
			std::memcpy(destp, datap, width*pixelsize);
			destp += width*pixelsize;
			datap += (src->width - x)*pixelsize;
		}
		return dst;
	}


	void usage(char const * progname)
	{
		std::fprintf(stderr,
			"usage: %s [<options>] [--] <BNI file> <output directory>\n"
			"    -h, --help, --usage  show this information and exit\n"
			"    -v, --version        print version number and exit\n", progname);

		std::exit(EXIT_FAILURE);
	}

}

extern int main(int argc, char * argv[])
{
	char const * outdir = NULL;
	char const * bnifile = NULL;
	std::FILE *       fbni;
	int          a;
	int          forcefile = 0;
	char         dash[] = "-"; /* unique address used as flag */

	if (argc < 1 || !argv || !argv[0])
	{
		std::fprintf(stderr, "bad arguments\n");
		return EXIT_FAILURE;
	}

	for (a = 1; a < argc; a++)
	if (forcefile && !bnifile)
		bnifile = argv[a];
	else if (std::strcmp(argv[a], "-") == 0 && !bnifile)
		bnifile = dash;
	else if (argv[a][0] != '-' && !bnifile)
		bnifile = argv[a];
	else if (forcefile && !outdir)
		outdir = argv[a];
	else if (std::strcmp(argv[a], "-") == 0 && !outdir)
		outdir = dash;
	else if (argv[a][0] != '-' && !outdir)
		outdir = argv[a];
	else if (forcefile || argv[a][0] != '-' || std::strcmp(argv[a], "-") == 0)
	{
		std::fprintf(stderr, "%s: extra file argument \"%s\"\n", argv[0], argv[a]);
		usage(argv[0]);
	}
	else if (std::strcmp(argv[a], "--") == 0)
		forcefile = 1;
	else if (std::strcmp(argv[a], "-v") == 0 || std::strcmp(argv[a], "--version") == 0)
	{
		std::printf("version " PVPGN_VERSION "\n");
		return EXIT_SUCCESS;
	}
	else if (std::strcmp(argv[a], "-h") == 0 || std::strcmp(argv[a], "--help") == 0 || std::strcmp(argv[a], "--usage")
		== 0)
		usage(argv[0]);
	else
	{
		std::fprintf(stderr, "%s: unknown option \"%s\"\n", argv[0], argv[a]);
		usage(argv[0]);
	}

	if (!bnifile)
	{
		std::fprintf(stderr, "%s: BNI file not specified\n", argv[0]);
		usage(argv[0]);
	}
	if (!outdir)
	{
		std::fprintf(stderr, "%s: output directory not specified\n", argv[0]);
		usage(argv[0]);
	}

	if (bnifile == dash)
		fbni = stdin;
	else
	if (!(fbni = std::fopen(bnifile, "r")))
	{
		std::fprintf(stderr, "%s: could not open BNI file \"%s\" for reading (std::fopen: %s)\n", argv[0], bnifile, std::strerror(errno));
		return EXIT_FAILURE;
	}

	if (outdir == dash)
	{
		std::fprintf(stderr, "%s: can not write directory to <stdout>\n", argv[0]);
		if (bnifile != dash && std::fclose(fbni) < 0)
			std::fprintf(stderr, "%s: could not close BNI file \"%s\" after reading (std::fclose: %s)\n", argv[0], bnifile, std::strerror(errno));
		return EXIT_FAILURE;
	}
	{
		std::error_code ec;
		auto status = std::filesystem::status(outdir, ec);
		if (ec && ec == std::errc::no_such_file_or_directory) {
			std::fprintf(stderr, "Info: Creating directory \"%s\" ...\n", outdir);
			std::error_code mec;
			if (!std::filesystem::create_directory(outdir, mec) && mec) {
				std::fprintf(stderr, "%s: could not create output directory \"%s\" (%s)", argv[0], outdir, mec.message().c_str());
				if (bnifile != dash && std::fclose(fbni) < 0)
					std::fprintf(stderr, "%s: could not close BNI file \"%s\" after reading (std::fclose: %s)\n", argv[0], bnifile, std::strerror(errno));
				return EXIT_FAILURE;
			}
		}
		else if (ec) {
			std::fprintf(stderr, "%s: could not stat output directory \"%s\" (%s)\n", argv[0], outdir, ec.message().c_str());
			if (bnifile != dash && std::fclose(fbni) < 0)
				std::fprintf(stderr, "%s: could not close BNI file \"%s\" after reading (std::fclose: %s)\n", argv[0], bnifile, std::strerror(errno));
			return EXIT_FAILURE;
		}
		else if (!std::filesystem::is_directory(status)) {
			std::fprintf(stderr, "%s: \"%s\" is not a directory\n", argv[0], outdir);
			if (bnifile != dash && std::fclose(fbni) < 0)
				std::fprintf(stderr, "%s: could not close BNI file \"%s\" after reading (std::fclose: %s)\n", argv[0], bnifile, std::strerror(errno));
			return EXIT_FAILURE;
		}
	}

	{
		unsigned int i;
		int          curry;
		t_tgaimg *   iconimg;
		t_bnifile *  bni;
		std::FILE *       indexfile;
		std::string  indexfilename;

		std::fprintf(stderr, "Info: Loading \"%s\" ...\n", bnifile);
		bni = load_bni(fbni);
		if (bni == NULL) return EXIT_FAILURE;
		if (std::fseek(fbni, bni->dataoffset, SEEK_SET) < 0) {
			std::fprintf(stderr, "%s: could not seek to TGA data offset %lu (std::fseek: %s)\n", argv[0], static_cast<unsigned long>(bni->dataoffset), std::strerror(errno));
			if (bnifile != dash && std::fclose(fbni) < 0)
				std::fprintf(stderr, "%s: could not close BNI file \"%s\" after reading (std::fclose: %s)\n", argv[0], bnifile, std::strerror(errno));
			return EXIT_FAILURE;
		}
		std::fprintf(stderr, "Info: Loading image ...\n");
		iconimg = load_tga(fbni);
		if (iconimg == NULL) return EXIT_FAILURE;

		std::fprintf(stderr, "Info: Extracting icons ...\n");
		indexfilename = std::string(outdir) + "/bniindex.lst";
		std::fprintf(stderr, "Info: Writing Index to \"%s\" ... \n", indexfilename.c_str());
		indexfile = std::fopen(indexfilename.c_str(), "w");
		if (indexfile == NULL) {
			std::fprintf(stderr, "%s: could not open index file \"%s\" for writing (std::fopen: %s)\n", argv[0], indexfilename.c_str(), std::strerror(errno));
			if (bnifile != dash && std::fclose(fbni) < 0)
				std::fprintf(stderr, "%s: could not close BNI file \"%s\" after reading (std::fclose: %s)\n", argv[0], bnifile, std::strerror(errno));
			return EXIT_FAILURE;
		}
		std::fprintf(indexfile, "unknown1 %08x\n", bni->unknown1);
		std::fprintf(indexfile, "unknown2 %08x\n", bni->unknown2);
		curry = 0;
		for (i = 0; i < bni->numicons; i++) {
			std::FILE *dsttga;
			std::string name;
			t_tgaimg *icn;
			icn = area2img(iconimg, 0, curry, bni->icons[i].x, bni->icons[i].y, tgaimgtype_uncompressed_truecolor);
			if (icn == NULL) {
				std::fprintf(stderr, "Error: area2img failed!\n");
				return EXIT_FAILURE;
			}
			char buf[1024];
			if (bni->icons[i].id == 0) {
				int tag = bni->icons[i].tag;
				std::snprintf(buf, sizeof(buf), "%s/%c%c%c%c.tga", outdir,
					static_cast<unsigned char>((tag >> 24) & 0xff),
					static_cast<unsigned char>((tag >> 16) & 0xff),
					static_cast<unsigned char>((tag >> 8) & 0xff),
					static_cast<unsigned char>(tag & 0xff));
			}
			else {
				std::snprintf(buf, sizeof(buf), "%s/%08x.tga", outdir, bni->icons[i].id);
			}
			name = buf;
			std::fprintf(stderr, "Info: Writing icon %u(%ux%u) to file \"%s\" ... \n", i + 1, icn->width, icn->height, name.c_str());
			curry += icn->height;
			dsttga = std::fopen(name.c_str(), "w");
			if (dsttga == NULL) {
				std::fprintf(stderr, "%s: could not open ouptut TGA file \"%s\" for writing (std::fopen: %s)\n", argv[0], name.c_str(), std::strerror(errno));
			}
			else {
				if (write_tga(dsttga, icn) < 0) {
					std::fprintf(stderr, "Error: Writing to TGA failed.\n");
				}
				else {
					int tag = bni->icons[i].tag;
					if (bni->icons[i].id == 0) {
						std::fprintf(indexfile, "icon !%c%c%c%c %d %d %08x\n",
							static_cast<unsigned char>((tag >> 24) & 0xff),
							static_cast<unsigned char>((tag >> 16) & 0xff),
							static_cast<unsigned char>((tag >> 8) & 0xff),
							static_cast<unsigned char>(tag & 0xff),
							bni->icons[i].x, bni->icons[i].y, bni->icons[i].unknown);
					}
					else {
						std::fprintf(indexfile, "icon #%08x %d %d %08x\n", bni->icons[i].id, bni->icons[i].x, bni->icons[i].y, bni->icons[i].unknown);
					}
				}
				if (std::fclose(dsttga) < 0)
					std::fprintf(stderr, "%s: could not close TGA file \"%s\" after writing (std::fclose: %s)\n", argv[0], name.c_str(), std::strerror(errno));
			}
			destroy_img(icn);
		}
		destroy_img(iconimg);
		if (std::fclose(indexfile) < 0) {
			std::fprintf(stderr, "%s: could not close index file \"%s\" after writing (std::fclose: %s)\n", argv[0], indexfilename.c_str(), std::strerror(errno));
			return EXIT_FAILURE;
		}
	}
	if (bnifile != dash && std::fclose(fbni) < 0)
		std::fprintf(stderr, "%s: could not close BNI file \"%s\" after reading (std::fclose: %s)\n", argv[0], bnifile, std::strerror(errno));

	return EXIT_SUCCESS;
}
