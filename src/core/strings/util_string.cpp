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
// String parsing, formatting, and escaping utilities (plan 15 §3 / SOLID-S).
// Included as a sub-TU by util.cpp — do not compile directly.

namespace pvpgn
{

	extern int strstart(char const * full, char const * part)
	{
		std::size_t strlen_part;
		int compare_result;

		if (!full || !part)
			return 1;

		strlen_part = std::strlen(part);
		compare_result = strncasecmp(full, part, strlen_part);

		/* If there is more than the command, make sure it is separated */
		if (compare_result != 0)
			return compare_result;
		else if (full[strlen_part] != ' ' && full[strlen_part] != '\0')
			return 1;
		else return compare_result;
	}


	extern char * strreverse(char * str)
	{
		std::reverse(str, str + std::strlen(str));
		return str;
	}


	extern int str_to_uint(char const * str, unsigned int * num)
	{
		unsigned int val;
		unsigned int pval;
		char * pos;

		if (!str || !num)
			return -1;
		for (pos = (char *)str; *pos == ' ' || *pos == '\t'; pos++);
		if (*pos == '+')
			pos++;

		val = 0;
		for (; *pos != '\0'; pos++)
		{
			pval = val;
			val *= 10;
			if (val / 10 != pval) /* check for overflow */
				return -1;

			pval = val;
			if (std::isdigit(*pos))
				val += *pos - '0';
			else
				return -1;

			if (val < pval) /* check for overflow */
				return -1;
		}

		*num = val;
		return 0;
	}


	extern int str_to_ushort(char const * str, unsigned short * num)
	{
		unsigned short val;
		unsigned short pval;
		char * pos;

		if (!str || !num)
			return -1;
		for (pos = (char *)str; *pos == ' ' || *pos == '\t'; pos++);
		if (*pos == '+')
			pos++;

		val = 0;
		for (; *pos != '\0'; pos++)
		{
			pval = val;
			val *= 10;
			if (val / 10 != pval) /* check for overflow */
				return -1;

			pval = val;

			if (std::isdigit(*pos))
				val += *pos - '0';
			else
				return -1;

			if (val < pval) /* check for overflow */
				return -1;
		}

		*num = val;
		return 0;
	}


	/* This routine assumes ASCII like control chars.
	   If len is zero, it will print all characters up to the first NUL,
	   otherwise it will print exactly that many characters. */
	int str_print_term(std::FILE * fp, char const * str, unsigned int len, int allow_nl)
	{
		unsigned int i;

		if (!fp)
			return -1;
		if (!str)
			return -1;

		if (len == 0)
			len = std::strlen(str);
		for (i = 0; i < len; i++)
		if ((str[i] == '\177' || (str[i] >= '\000' && str[i] < '\040')) &&
			(!allow_nl || (str[i] != '\r' && str[i] != '\n')))
			std::fprintf(fp, "^%c", str[i] + 64);
		else
			std::fputc((int)str[i], fp);

		return 0;
	}


	extern int str_get_bool(char const * str)
	{
		if (!str)
			return -1;

		if (strcasecmp(str, "true") == 0 ||
			strcasecmp(str, "yes") == 0 ||
			strcasecmp(str, "on") == 0 ||
			std::strcmp(str, "1") == 0)
			return 1;

		if (strcasecmp(str, "false") == 0 ||
			strcasecmp(str, "no") == 0 ||
			strcasecmp(str, "off") == 0 ||
			std::strcmp(str, "0") == 0)
			return 0;

		return -1;
	}


	extern char * escape_fs_chars(char const * in, unsigned int len)
	{
		char *       out;
		unsigned int inpos;
		unsigned int outpos;

		if (!in)
			return NULL;
		out = new char[len * 3 + 1]{}; /* if all turn into %XX */

		for (inpos = 0, outpos = 0; inpos < len; inpos++)
		{
			if (in[inpos] == '\0' || in[inpos] == '%' || in[inpos] == '/' ||
				in[inpos] == '\\' || in[inpos] == ':') /* FIXME: what other characters does Windows not allow? */
			{
				out[outpos++] = '%';
				/* always 00 through FF hex */
				std::sprintf(&out[outpos], "%02X", (unsigned int)(unsigned char)in[inpos]);
				outpos += 2;
			}
			else
				out[outpos++] = in[inpos];
		}
		/*  if outpos >= len*3+1 then the buffer was overflowed */
		out[outpos] = '\0';

		return out;
	}


	extern char * escape_chars(char const * in, unsigned int len)
	{
		char *       out;
		unsigned int inpos;
		unsigned int outpos;

		if (!in)
			return NULL;
		out = new char[len * 4 + 1]{}; /* if all turn into \xxx */

		for (inpos = 0, outpos = 0; inpos < len; inpos++)
		{
			if (in[inpos] == '\\')
			{
				out[outpos++] = '\\';
				out[outpos++] = '\\';
			}
			else if (in[inpos] == '"')
			{
				out[outpos++] = '\\';
				out[outpos++] = '"';
			}
			else if (std::isprint((unsigned char)in[inpos]))
				out[outpos++] = in[inpos];
			else if (in[inpos] == '\a')
			{
				out[outpos++] = '\\';
				out[outpos++] = 'a';
			}
			else if (in[inpos] == '\b')
			{
				out[outpos++] = '\\';
				out[outpos++] = 'b';
			}
			else if (in[inpos] == '\t')
			{
				out[outpos++] = '\\';
				out[outpos++] = 't';
			}
			else if (in[inpos] == '\n')
			{
				out[outpos++] = '\\';
				out[outpos++] = 'n';
			}
			else if (in[inpos] == '\v')
			{
				out[outpos++] = '\\';
				out[outpos++] = 'v';
			}
			else if (in[inpos] == '\f')
			{
				out[outpos++] = '\\';
				out[outpos++] = 'f';
			}
			else if (in[inpos] == '\r')
			{
				out[outpos++] = '\\';
				out[outpos++] = 'r';
			}
			else
			{
				out[outpos++] = '\\';
				/* always 001 through 377 octal */
				std::sprintf(&out[outpos], "%03o", (unsigned int)(unsigned char)in[inpos]);
				outpos += 3;
			}
		}
		/*  if outpos >= len*4+1 then the buffer was overflowed */
		out[outpos] = '\0';

		return out;
	}


	extern char * unescape_chars(char const * in)
	{
		char *       out;
		unsigned int inpos;
		unsigned int outpos;
		unsigned int inlen;

		if (!in)
			return NULL;

		inlen = std::strlen(in);
		out = new char[inlen + 1]{};

		for (inpos = 0, outpos = 0; inpos < inlen; inpos++)
		{
			if (in[inpos] != '\\')
				out[outpos++] = in[inpos];
			else
				switch (in[++inpos])
			{
				case '\\':
					out[outpos++] = '\\';
					break;
				case '"':
					out[outpos++] = '"';
					break;
				case 'a':
					out[outpos++] = '\a';
					break;
				case 'b':
					out[outpos++] = '\b';
					break;
				case 't':
					out[outpos++] = '\t';
					break;
				case 'n':
					out[outpos++] = '\n';
					break;
				case 'v':
					out[outpos++] = '\v';
					break;
				case 'f':
					out[outpos++] = '\f';
					break;
				case 'r':
					out[outpos++] = '\r';
					break;
				default:
				{
						   char         temp[4];
						   unsigned int i;
						   unsigned int num;

						   for (i = 0; i < 3; i++)
						   {
							   if (in[inpos] != '0' &&
								   in[inpos] != '1' &&
								   in[inpos] != '2' &&
								   in[inpos] != '3' &&
								   in[inpos] != '4' &&
								   in[inpos] != '5' &&
								   in[inpos] != '6' &&
								   in[inpos] != '7')
								   break;
							   temp[i] = in[inpos++];
						   }
						   temp[i] = '\0';
						   inpos--;

						   num = std::strtoul(temp, NULL, 8);
						   if (i < 3 || num<1 || num>255) /* bad escape (including \000), leave it as-is */
						   {
							   out[outpos++] = '\\';
							   std::strcpy(&out[outpos], temp);
							   outpos += std::strlen(temp);
						   }
						   else
							   out[outpos++] = (unsigned char)num;
				}
			}
		}
		out[outpos] = '\0';

		return out;
	}

} // namespace pvpgn
