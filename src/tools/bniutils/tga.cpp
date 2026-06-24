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
#include "tga.h"
#include <print>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#include "fileio.h"

namespace pvpgn
{

	namespace bni
	{

		namespace
		{

			int rotate_updown(t_tgaimg *img) {
				int pixelsize;
				int y;
				if (img == NULL) return -1;
				if (img->data.empty()) return -1;
				pixelsize = getpixelsize(img);
				if (pixelsize == 0) return -1;
				std::vector<std::uint8_t> ndata(static_cast<std::size_t>(img->width)*static_cast<std::size_t>(img->height)*static_cast<std::size_t>(pixelsize));
				for (y = 0; y < img->height; y++) {
					std::memcpy(ndata.data() + (y*img->width*pixelsize),
						img->data.data() + ((img->width*img->height*pixelsize) - ((y + 1)*img->width*pixelsize)),
					static_cast<std::size_t>(img->width)*static_cast<std::size_t>(pixelsize));
				}
				img->data = std::move(ndata);
				return 0;
			}

			int rotate_leftright(t_tgaimg *img) {
				unsigned char *datap;
				int pixelsize;
				int y, x;
				std::println(stderr, "WARNING: rotate_leftright: this function is untested!");
				if (img == NULL) return -1;
				if (img->data.empty()) return -1;
				pixelsize = getpixelsize(img);
				if (pixelsize == 0) return -1;
				std::vector<std::uint8_t> ndata(static_cast<std::size_t>(img->width)*static_cast<std::size_t>(img->height)*static_cast<std::size_t>(pixelsize));
				datap = img->data.data();
				for (y = 0; y < img->height; y++) {
					unsigned char *linep = (ndata.data() + (((y + 1)*img->width*pixelsize) - pixelsize));
					for (x = 0; x < img->width; x++) {
						std::memcpy(linep, datap, static_cast<std::size_t>(pixelsize));
						linep -= pixelsize;
						datap += pixelsize;
					}
				}
				img->data = std::move(ndata);

				return 0;
			}

			int RLE_decompress(std::FILE *f, void *buf, int bufsize, int pixelsize) {
				unsigned char pt;
				unsigned char *bufp;
				unsigned char temp[8]; /* MAXPIXELSIZE */
				int bufi;
				int count;
				bufp = static_cast<unsigned char*>(buf);
				for (bufi = 0; bufi<bufsize;) {
					pt = file_readb(f);
					if (std::feof(f)) {
						std::println(stderr, "RLE_decompress: after final packet only got {} of {} bytes", bufi, bufsize);
						return -1;
					}
					count = (pt & 0x7f) + 1;
					if (bufi + count*pixelsize>bufsize) {
						std::println(stderr, "RLE_decompress: buffer too short for next packet (need {} bytes, have {})", bufi + count*pixelsize, bufsize);
						return -1;
					}
					if ((pt & 0x80) == 0) {	/* RAW PACKET */
						if (std::fread(bufp, static_cast<std::size_t>(pixelsize), static_cast<std::size_t>(count), f) < static_cast<unsigned>(count)) {
							if (std::feof(f))
								std::println(stderr, "RLE_decompress: short RAW packet (expected {} bytes) (EOF)", pixelsize*count);
							else
								std::println(stderr, "RLE_decompress: short RAW packet (expected {} bytes) (std::fread: {})", pixelsize*count, std::strerror(errno));
						}
						bufp += count*pixelsize;
						bufi += count*pixelsize;
					}
					else { /* RLE PACKET */
						if (std::fread(temp, static_cast<std::size_t>(pixelsize), 1, f) < 1) {
							if (std::feof(f))
								std::println(stderr, "RLE_decompress: short RLE packet (expected {} bytes) (EOF)", pixelsize);
							else
								std::println(stderr, "RLE_decompress: short RLE packet (expected {} bytes) (std::fread: {})", pixelsize, std::strerror(errno));
						}
						if (count<2) {
							std::println(stderr, "RLE_decompress: suspicious RLE repetition count {}", count);
						}
						for (; count > 0; count--) {
							std::memcpy(bufp, temp, static_cast<std::size_t>(pixelsize));
							bufp += pixelsize;
							bufi += pixelsize;
						}
					}
				}
				return 0;
			}

			void RLE_write_pkt(std::FILE *f, t_tgapkttype pkttype, int len, void *data, int pixelsize) {
				unsigned char count;

				if (len<1 || len>128) {
					std::println(stderr, "RLE_write_pkt: packet has bad length ({} bytes)", len);
					return;
				}
				if (pkttype == RLE) {
					if (len < 2) {
						std::println(stderr, "RLE_write_pkt: RLE packet has bad length ({} bytes)", len);
						return;
					}
					count = static_cast<unsigned char>(0x80 | (len - 1));
					if (std::fwrite(&count, 1, 1, f) < 1)
						std::println(stderr, "RLE_write_pkt: could not write RLE pixel count (std::fwrite: {})", std::strerror(errno));
					if (std::fwrite(data, static_cast<std::size_t>(pixelsize), 1, f) < 1)
						std::println(stderr, "RLE_write_pkt: could not write RLE pixel value (std::fwrite: {})", std::strerror(errno));
				}
				else {
					count = static_cast<unsigned char>(len - 1);
					if (std::fwrite(&count, 1, 1, f) < 1)
						std::println(stderr, "RLE_write_pkt: could not write RAW pixel count (std::fwrite: {})", std::strerror(errno));
					if (std::fwrite(data, static_cast<std::size_t>(pixelsize), static_cast<std::size_t>(len), f) < static_cast<unsigned>(len))
						std::println(stderr, "RLE_write_pkt: could not write {} RAW pixels (std::fwrite: {})", len, std::strerror(errno));
				}
			}


			int RLE_compress(std::FILE *f, t_tgaimg const *img) {
				int pixelsize;
				unsigned char const *datap;
				unsigned char *pktdata;
				unsigned int pktlen;
				t_tgapkttype pkttype;
				unsigned char *pktdatap;
				unsigned int actual = 0, perceived = 0;
				int i;

				pkttype = RAW;
				pktdatap = NULL;

				if (img == NULL) return -1;
				if (img->data.empty()) return -1;
				pixelsize = getpixelsize(img);
				if (pixelsize == 0) return -1;

				datap = img->data.data();
				std::vector<unsigned char> pktdata_vec(static_cast<std::size_t>(img->width)*static_cast<std::size_t>(img->height)*static_cast<std::size_t>(pixelsize));
				pktdata = pktdata_vec.data();
				pktlen = 0;

				for (i = 0; i < img->width*img->height;) {
					if (pktlen == 0) {
						pktdatap = pktdata;
						std::memcpy(pktdatap, datap, static_cast<std::size_t>(pixelsize));
						pktlen++;
						i++;
						pktdatap += pixelsize;
						datap += pixelsize;
						pkttype = RAW;
						continue;
					}
					if (pktlen == 1) {
						if (std::memcmp(datap - pixelsize, datap, static_cast<std::size_t>(pixelsize)) == 0) {
							pkttype = RLE;
						}
					}
					if (pkttype == RLE) {
						if (std::memcmp(datap - pixelsize, datap, static_cast<std::size_t>(pixelsize)) != 0 || pktlen >= 128) {
							RLE_write_pkt(f, pkttype, static_cast<int>(pktlen), pktdata, pixelsize);
							actual += static_cast<unsigned int>(1 + pixelsize);
							perceived += static_cast<unsigned int>(pixelsize*static_cast<int>(pktlen));
							pktlen = 0;
						}
						else {
							pktlen++;
							i++;
							datap += pixelsize;
						}
					}
					else {
						if (std::memcmp(datap - pixelsize, datap, static_cast<std::size_t>(pixelsize)) == 0 || pktlen >= 129) {
							datap -= pixelsize; /* push back last pixel */
							i--;
							if (i < 0) std::println(stderr, "BUG!");
							pktlen--;
							RLE_write_pkt(f, pkttype, static_cast<int>(pktlen), pktdata, pixelsize);
							actual += static_cast<unsigned int>(1 + pixelsize*static_cast<int>(pktlen));
							perceived += static_cast<unsigned int>(pixelsize*static_cast<int>(pktlen));
							pktlen = 0;
						}
						else {
							std::memcpy(pktdatap, datap, static_cast<std::size_t>(pixelsize));
							pktlen++;
							i++;
							pktdatap += pixelsize;
							datap += pixelsize;
						}
					}
				}
				if (pktlen) {
					RLE_write_pkt(f, pkttype, static_cast<int>(pktlen), pktdata, pixelsize);
					if (pkttype == RLE) {
						actual += static_cast<unsigned int>(1 + pixelsize);
						perceived += static_cast<unsigned int>(pixelsize*static_cast<int>(pktlen));
					}
					else {
						actual += static_cast<unsigned int>(1 + pixelsize*static_cast<int>(pktlen));
						perceived += static_cast<unsigned int>(pixelsize*static_cast<int>(pktlen));
					}
					pktlen = 0;
				}
				std::println(stderr, "RLE_compress: wrote {} bytes ({} uncompressed)", actual, perceived);
				return 0;
			}

		}

		extern int getpixelsize(t_tgaimg const *img) {
			switch (img->bpp) {
			case 8:
				return 1;
			case 15:
			case 16:
				return 2;
			case 24:
				return 3;
			case 32:
				return 4;
			default:
				std::println(stderr, "load_tga: color depth {} is not supported!", img->bpp);
				return 0;
			}
		}


		extern t_tgaimg * new_tgaimg(unsigned int width, unsigned int height, unsigned int bpp, t_tgaimgtype imgtype) {
			auto *img = new t_tgaimg{};
			img->idlen = 0;
			img->cmaptype = tgacmap_none;
			img->imgtype = imgtype;
			img->cmapfirst = 0;
			img->cmaplen = 0;
			img->cmapes = 0;
			img->xorigin = 0;
			img->yorigin = 0;
			img->width = static_cast<std::uint16_t>(width);
			img->height = static_cast<std::uint16_t>(height);
			img->bpp = static_cast<std::uint8_t>(bpp);
			img->desc = 0; /* no attribute bits, top, left, and zero reserved */
			img->extareaoff = 0;
			img->devareaoff = 0;

			return img;
		}

		extern t_tgaimg * load_tgaheader(std::FILE *f) {
			auto *img = new t_tgaimg{};
			img->idlen = file_readb(f);
			img->cmaptype = file_readb(f);
			img->imgtype = file_readb(f);
			img->cmapfirst = file_readw_le(f);
			img->cmaplen = file_readw_le(f);
			img->cmapes = file_readb(f);
			img->xorigin = file_readw_le(f);
			img->yorigin = file_readw_le(f);
			img->width = file_readw_le(f);
			img->height = file_readw_le(f);
			img->bpp = file_readb(f);
			img->desc = file_readb(f);
			img->extareaoff = 0; /* ignored when reading */
			img->devareaoff = 0; /* ignored when reading */

			return img;
		}

		extern t_tgaimg * load_tga(std::FILE *f) {
			t_tgaimg *img;
			int pixelsize;
			img = load_tgaheader(f);

			/* make sure we understand the header fields */
			if (img->cmaptype != tgacmap_none) {
				std::println(stderr, "load_tga: Color-mapped images are not (yet?) supported!");
				delete img;
				return NULL;
			}
			if (img->imgtype != tgaimgtype_uncompressed_truecolor && img->imgtype != tgaimgtype_rlecompressed_truecolor) {
				std::println(stderr, "load_tga: imagetype {} is not supported. (only 2 and 10 are supported)", img->imgtype);
				delete img;
				return NULL;
			}

			pixelsize = getpixelsize(img);
			if (pixelsize == 0) {
				delete img;
				return NULL;
			}
			/* Skip the ID if there is one */
			if (img->idlen > 0) {
				std::println(stderr, "load_tga: ID present, skipping {} bytes", img->idlen);
				if (std::fseek(f, img->idlen, SEEK_CUR) < 0)
					std::println(stderr, "load_tga: could not seek {} bytes forward (std::fseek: {})", img->idlen, std::strerror(errno));
			}

			/* Now, we can alloc img->data */
			img->data.resize(static_cast<std::size_t>(img->width)*static_cast<std::size_t>(img->height)*static_cast<std::size_t>(pixelsize));
			if (img->imgtype == tgaimgtype_uncompressed_truecolor) {
				if (std::fread(img->data.data(), static_cast<std::size_t>(pixelsize), static_cast<std::size_t>(img->width)*static_cast<std::size_t>(img->height), f) < static_cast<unsigned>(img->width*img->height)) {
					std::println(stderr, "load_tga: error while reading data!");
					delete img;
					return NULL;
				}
			}
			else { /* == tgaimgtype_rlecompressed_truecolor */
				if (RLE_decompress(f, img->data.data(), img->width*img->height*pixelsize, pixelsize) < 0) {
					std::println(stderr, "load_tga: error while decompressing data!");
					delete img;
					return NULL;
				}
			}
			if ((img->desc & tgadesc_horz) != 0) { /* right, want left */
				if (rotate_leftright(img) < 0) {
					std::println(stderr, "ERROR: rotate_leftright failed!");
				}
			}
			if ((img->desc & tgadesc_vert) == 0) { /* bottom, want top */
				if (rotate_updown(img) < 0) {
					std::println(stderr, "ERROR: rotate_updown failed!");
				}
			}
			return img;
		}

		extern int write_tga(std::FILE *f, t_tgaimg *img) {
			if (f == NULL) return -1;
			if (img == NULL) return -1;
			if (img->data.empty()) return -1;
			if (img->idlen != 0) return -1;
			if (img->cmaptype != tgacmap_none) return -1;
			if (img->imgtype != tgaimgtype_uncompressed_truecolor && img->imgtype != tgaimgtype_rlecompressed_truecolor) return -1;
			file_writeb(f, img->idlen);
			file_writeb(f, img->cmaptype);
			file_writeb(f, img->imgtype);
			file_writew_le(f, img->cmapfirst);
			file_writew_le(f, img->cmaplen);
			file_writeb(f, img->cmapes);
			file_writew_le(f, img->xorigin);
			file_writew_le(f, img->yorigin);
			file_writew_le(f, img->width);
			file_writew_le(f, img->height);
			file_writeb(f, img->bpp);
			file_writeb(f, img->desc);

			if ((img->desc&tgadesc_horz) != 0) { /* right, want left */
				std::println(stderr, "write_tga: flipping horizontally");
				if (rotate_leftright(img) < 0) {
					std::println(stderr, "ERROR: rotate_updown failed!");
				}
			}
			if ((img->desc&tgadesc_vert) == 0) { /* bottom, want top */
				std::println(stderr, "write_tga: flipping vertically");
				if (rotate_updown(img) < 0) {
					std::println(stderr, "ERROR: rotate_updown failed!");
				}
			}
			if (img->imgtype == tgaimgtype_uncompressed_truecolor) {
				int pixelsize;

				pixelsize = getpixelsize(img);
				if (pixelsize == 0) return -1;
				if (std::fwrite(img->data.data(), static_cast<std::size_t>(pixelsize), static_cast<std::size_t>(img->width)*static_cast<std::size_t>(img->height), f) < static_cast<unsigned>(img->width*img->height)) {
					std::println(stderr, "write_tga: could not write {} pixels (std::fwrite: {})", img->width*img->height, std::strerror(errno));
					return -1;
				}
			}
			else if (img->imgtype == tgaimgtype_rlecompressed_truecolor) {
				std::println(stderr, "write_tga: using RLE compression");
				if (RLE_compress(f, img) < 0) {
					std::println(stderr, "write_tga: RLE compression failed.");
				}
			}
			/* Write the file-footer */
			file_writed_le(f, img->extareaoff);
			file_writed_le(f, img->devareaoff);
			if (std::fwrite(TGAMAGIC, std::strlen(TGAMAGIC) + 1, 1, f) < 1)
				std::println(stderr, "write_tga: could not write TGA footer magic (std::fwrite: {})", std::strerror(errno));
			/* Ready */
			return 0;
		}

		extern void destroy_img(t_tgaimg * img) {
			delete img;
		}

		extern void print_tga_info(t_tgaimg const * img, std::FILE * fp) {
			unsigned int interleave;
			unsigned int attrbits;
			char const * typestr;
			char const * cmapstr;
			char const * horzstr;
			char const * vertstr;
			char const * intlstr;

			if (!img || !fp)
				return;

			interleave = static_cast<unsigned int>(((img->desc&tgadesc_interleave1) != 0) * 2 + ((img->desc&tgadesc_interleave2) != 0));
			attrbits = img->desc&(tgadesc_attrbits0 | tgadesc_attrbits1 | tgadesc_attrbits2 | tgadesc_attrbits3);
			switch (img->imgtype) {
			case tgaimgtype_empty:
				typestr = "No Image Data Included";
				break;
			case tgaimgtype_uncompressed_mapped:
				typestr = "Uncompressed, Color-mapped Image";
				break;
			case tgaimgtype_uncompressed_truecolor:
				typestr = "Uncompressed, True-color Image";
				break;
			case tgaimgtype_uncompressed_monochrome:
				typestr = "Uncompressed, Black-and-white image";
				break;
			case tgaimgtype_rlecompressed_mapped:
				typestr = "Run-length encoded, Color-mapped Image";
				break;
			case tgaimgtype_rlecompressed_truecolor:
				typestr = "Run-length encoded, True-color Image";
				break;
			case tgaimgtype_rlecompressed_monochrome:
				typestr = "Run-length encoded, Black-and-white image";
				break;
			case tgaimgtype_huffman_mapped:
				typestr = "Huffman encoded, Color-mapped image";
				break;
			case tgaimgtype_huffman_4pass_mapped:
				typestr = "Four-pass Huffman encoded, Color-mapped image";
				break;
			default:
				typestr = "unknown";
			}
			switch (img->cmaptype) {
			case tgacmap_none:
				cmapstr = "None";
				break;
			case tgacmap_included:
				cmapstr = "Included";
				break;
			default:
				cmapstr = "Unknown";
			}
			if ((img->desc&tgadesc_horz) == 0) {
				horzstr = "left";
			}
			else {
				horzstr = "right";
			}
			if ((img->desc&tgadesc_vert) == 0) {
				vertstr = "bottom";
			}
			else {
				vertstr = "top";
			}
			switch (interleave) {
			case 0:
				intlstr = "none";
				break;
			case 2:
				intlstr = "two way";
				break;
			case 3:
				intlstr = "four way";
				break;
			case 4:
			default:
				intlstr = "unknown";
				break;
			}

			std::println(fp, "TGAHeader: IDLength={} ColorMapType={}({})", img->idlen, img->cmaptype, cmapstr);
			std::println(fp, "TGAHeader: ImageType={}({})", img->imgtype, typestr);
			std::println(fp, "TGAHeader: ColorMap: FirstEntryIndex={} ColorMapLength={}", img->cmapfirst, img->cmaplen);
			std::println(fp, "TGAHeader: ColorMap: ColorMapEntrySize={}bits", img->cmapes);
			std::println(fp, "TGAHeader: X-origin={} Y-origin={} Width={}(0x{:x}) Height={}(0x{:x})", img->xorigin, img->yorigin, img->width, img->width, img->height, img->height);
			std::println(fp, "TGAHeader: PixelDepth={}bits ImageDescriptor=0x{:02x}({} attribute bits, origin is {} {}, interleave={})", img->bpp, img->desc, attrbits, vertstr, horzstr, intlstr);
		}

	}

}
