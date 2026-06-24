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

#include <cerrno>
#include <print>
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

		dst = new_tgaimg(static_cast<unsigned int>(width), static_cast<unsigned int>(height), src->bpp, type);
		dst->data.resize(static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*static_cast<std::size_t>(pixelsize));

		datap = src->data.data();
		datap += y*src->width*pixelsize;
		destp = dst->data.data();
		for (i = 0; i < height; i++) {
			datap += x*pixelsize;
			std::memcpy(destp, datap, static_cast<std::size_t>(width)*static_cast<std::size_t>(pixelsize));
			destp += width*pixelsize;
			datap += (src->width - x)*pixelsize;
		}
		return dst;
	}


	void usage(char const * progname)
	{
		std::print(stderr,
			"usage: {} [<options>] [--] <BNI file> <output directory>\n"
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
	else if (forcefile && !outdir)
		outdir = argv[a];
	else if (std::strcmp(argv[a], "-") == 0 && !outdir)
		outdir = dash;
	else if (argv[a][0] != '-' && !outdir)
		outdir = argv[a];
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
	{
		std::println(stderr, "{}: BNI file not specified", argv[0]);
		usage(argv[0]);
	}
	if (!outdir)
	{
		std::println(stderr, "{}: output directory not specified", argv[0]);
		usage(argv[0]);
	}

	if (bnifile == dash)
		fbni = stdin;
	else
	if (!(fbni = std::fopen(bnifile, "r")))
	{
		std::println(stderr, "{}: could not open BNI file \"{}\" for reading (std::fopen: {})", argv[0], bnifile, std::strerror(errno));
		return EXIT_FAILURE;
	}

	if (outdir == dash)
	{
		std::println(stderr, "{}: can not write directory to <stdout>", argv[0]);
		if (bnifile != dash && std::fclose(fbni) < 0)
			std::println(stderr, "{}: could not close BNI file \"{}\" after reading (std::fclose: {})", argv[0], bnifile, std::strerror(errno));
		return EXIT_FAILURE;
	}
	{
		std::error_code ec;
		auto status = std::filesystem::status(outdir, ec);
		if (ec && ec == std::errc::no_such_file_or_directory) {
			std::println(stderr, "Info: Creating directory \"{}\" ...", outdir);
			std::error_code mec;
			if (!std::filesystem::create_directory(outdir, mec) && mec) {
				std::print(stderr, "{}: could not create output directory \"{}\" ({})", argv[0], outdir, mec.message().c_str());
				if (bnifile != dash && std::fclose(fbni) < 0)
					std::println(stderr, "{}: could not close BNI file \"{}\" after reading (std::fclose: {})", argv[0], bnifile, std::strerror(errno));
				return EXIT_FAILURE;
			}
		}
		else if (ec) {
			std::println(stderr, "{}: could not stat output directory \"{}\" ({})", argv[0], outdir, ec.message().c_str());
			if (bnifile != dash && std::fclose(fbni) < 0)
				std::println(stderr, "{}: could not close BNI file \"{}\" after reading (std::fclose: {})", argv[0], bnifile, std::strerror(errno));
			return EXIT_FAILURE;
		}
		else if (!std::filesystem::is_directory(status)) {
			std::println(stderr, "{}: \"{}\" is not a directory", argv[0], outdir);
			if (bnifile != dash && std::fclose(fbni) < 0)
				std::println(stderr, "{}: could not close BNI file \"{}\" after reading (std::fclose: {})", argv[0], bnifile, std::strerror(errno));
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

		std::println(stderr, "Info: Loading \"{}\" ...", bnifile);
		bni = load_bni(fbni);
		if (bni == NULL) return EXIT_FAILURE;
		if (std::fseek(fbni, bni->dataoffset, SEEK_SET) < 0) {
			std::println(stderr, "{}: could not seek to TGA data offset {} (std::fseek: {})", argv[0], static_cast<unsigned long>(bni->dataoffset), std::strerror(errno));
			if (bnifile != dash && std::fclose(fbni) < 0)
				std::println(stderr, "{}: could not close BNI file \"{}\" after reading (std::fclose: {})", argv[0], bnifile, std::strerror(errno));
			return EXIT_FAILURE;
		}
		std::println(stderr, "Info: Loading image ...");
		iconimg = load_tga(fbni);
		if (iconimg == NULL) return EXIT_FAILURE;

		std::println(stderr, "Info: Extracting icons ...");
		indexfilename = std::string(outdir) + "/bniindex.lst";
		std::println(stderr, "Info: Writing Index to \"{}\" ... ", indexfilename.c_str());
		indexfile = std::fopen(indexfilename.c_str(), "w");
		if (indexfile == NULL) {
			std::println(stderr, "{}: could not open index file \"{}\" for writing (std::fopen: {})", argv[0], indexfilename.c_str(), std::strerror(errno));
			if (bnifile != dash && std::fclose(fbni) < 0)
				std::println(stderr, "{}: could not close BNI file \"{}\" after reading (std::fclose: {})", argv[0], bnifile, std::strerror(errno));
			return EXIT_FAILURE;
		}
		std::println(indexfile, "unknown1 {:08x}", bni->unknown1);
		std::println(indexfile, "unknown2 {:08x}", bni->unknown2);
		curry = 0;
		for (i = 0; i < bni->numicons; i++) {
			std::FILE *dsttga;
			std::string name;
			t_tgaimg *icn;
			icn = area2img(iconimg, 0, curry, static_cast<int>(bni->icons[i].x), static_cast<int>(bni->icons[i].y), tgaimgtype_uncompressed_truecolor);
			if (icn == NULL) {
				std::println(stderr, "Error: area2img failed!");
				return EXIT_FAILURE;
			}
			char buf[1024];
			if (bni->icons[i].id == 0) {
				int tag = static_cast<int>(bni->icons[i].tag);
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
			std::println(stderr, "Info: Writing icon {}({}x{}) to file \"{}\" ... ", i + 1, icn->width, icn->height, name.c_str());
			curry += icn->height;
			dsttga = std::fopen(name.c_str(), "w");
			if (dsttga == NULL) {
				std::println(stderr, "{}: could not open ouptut TGA file \"{}\" for writing (std::fopen: {})", argv[0], name.c_str(), std::strerror(errno));
			}
			else {
				if (write_tga(dsttga, icn) < 0) {
					std::println(stderr, "Error: Writing to TGA failed.");
				}
				else {
					int tag = static_cast<int>(bni->icons[i].tag);
					if (bni->icons[i].id == 0) {
						std::println(indexfile, "icon !{}{}{}{} {} {} {:08x}",
							static_cast<unsigned char>((tag >> 24) & 0xff),
							static_cast<unsigned char>((tag >> 16) & 0xff),
							static_cast<unsigned char>((tag >> 8) & 0xff),
							static_cast<unsigned char>(tag & 0xff),
							bni->icons[i].x, bni->icons[i].y, bni->icons[i].unknown);
					}
					else {
						std::println(indexfile, "icon #{:08x} {} {} {:08x}", bni->icons[i].id, bni->icons[i].x, bni->icons[i].y, bni->icons[i].unknown);
					}
				}
				if (std::fclose(dsttga) < 0)
					std::println(stderr, "{}: could not close TGA file \"{}\" after writing (std::fclose: {})", argv[0], name.c_str(), std::strerror(errno));
			}
			destroy_img(icn);
		}
		destroy_img(iconimg);
		if (std::fclose(indexfile) < 0) {
			std::println(stderr, "{}: could not close index file \"{}\" after writing (std::fclose: {})", argv[0], indexfilename.c_str(), std::strerror(errno));
			return EXIT_FAILURE;
		}
	}
	if (bnifile != dash && std::fclose(fbni) < 0)
		std::println(stderr, "{}: could not close BNI file \"{}\" after reading (std::fclose: {})", argv[0], bnifile, std::strerror(errno));

	return EXIT_SUCCESS;
}
