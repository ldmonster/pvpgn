/*
 * Copyright (C) 2000,2001	Onlyer	(onlyer@263.net)
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
#include "xstring.h"

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

#include <strings.h>

namespace pvpgn
{

	extern char * strtolower(char * str)
	{
		if (!str) return nullptr;
		for (std::size_t i = 0; str[i]; i++) {
			const unsigned char ch = static_cast<unsigned char>(str[i]);
			if (std::isupper(ch))
				str[i] = static_cast<char>(std::tolower(ch));
		}
		return str;
	}

	extern unsigned char xtoi(unsigned char ch)
	{
		unsigned char retval;

		if (std::isalpha(ch)) retval = static_cast<unsigned char>(std::tolower(ch));
		else retval = ch;
		if (retval < 'A') retval = static_cast<unsigned char>(retval - '0');
		else retval = static_cast<unsigned char>(retval - ('a' - 0xa));
		return retval;
	}

	extern char * str_strip_affix(char * str, char const * affix)
	{
		unsigned int i, j, n;
		int		match;

		if (!str) return NULL;
		if (!affix) return str;
		for (i = 0; str[i]; i++) {
			match = 0;
			for (n = 0; affix[n]; n++) {
				if (str[i] == affix[n]) {
					match = 1;
					break;
				}
			}
			if (!match) break;
		}
		for (j = static_cast<unsigned int>(std::strlen(str)) - 1; j >= i; j--) {
			match = 0;
			for (n = 0; affix[n]; n++) {
				if (str[j] == affix[n]) {
					match = 1;
					break;
				}
			}
			if (!match) break;
		}
		if (i > j) {
			str[0] = '\0';
		}
		else {
			std::memmove(str, str + i, j - i + 1);
			str[j - i + 1] = '\0';
		}
		return str;
	}

	extern char * hexstrdup(unsigned char const * src)
	{
		if (!src) return nullptr;
		const std::size_t slen = std::strlen(reinterpret_cast<const char*>(src));
		char * dest = new char[slen + 1];
		std::strcpy(dest, reinterpret_cast<const char*>(src));
		const unsigned int len = hexstrtoraw(src, dest, static_cast<unsigned int>(slen + 1));
		dest[len] = '\0';
		return dest;
	}

	extern unsigned int hexstrtoraw(unsigned char const * src, char * data, unsigned int datalen)
	{
		unsigned char	ch;
		unsigned int	i, j;

		for (i = 0, j = 0; j < datalen; i++) {
			ch = src[i];
			if (!ch) break;
			if (ch == '\\') {
				i++;
				ch = src[i];
				if (!ch) {
					break;
				}
				else if (ch == '\\') {
					data[j++] = static_cast<char>(ch);
				}
				else if (ch == 'x') {
					if (std::isxdigit(src[i + 1])) {
						if (std::isxdigit(src[i + 2])) {
							data[j++] = static_cast<char>(xtoi(src[i + 1]) * 0x10 + xtoi(src[i + 2]));
							i += 2;
						}
						else {
							data[j++] = static_cast<char>(xtoi(src[i + 1]));
							i++;
						}
					}
					else {
						data[j++] = static_cast<char>(ch);
					}
				}
				else if (ch == 'n') {
					data[j++] = '\n';
				}
				else if (ch == 'r') {
					data[j++] = '\r';
				}
				else if (ch == 'a') {
					data[j++] = '\a';
				}
				else if (ch == 't') {
					data[j++] = '\t';
				}
				else if (ch == 'b') {
					data[j++] = '\b';
				}
				else if (ch == 'f') {
					data[j++] = '\f';
				}
				else if (ch == 'v') {
					data[j++] = '\v';
				}
				else {
					data[j++] = static_cast<char>(ch);
				}
			}
			else {
				data[j++] = static_cast<char>(ch);
				continue;
			}
		}
		return j;
	}

#define SPLIT_STRING_INIT_COUNT		32
#define	SPLIT_STRING_INCREASEMENT	32
	extern char * * strtoargv(char const * str, unsigned int * count)
	{
		unsigned int	n, index_size;
		char		* temp;
		unsigned int	i;
		int		j;
		int		* pindex;
		void		** ptrindex;
		char		* result;

		if (!str || !count) return NULL;
		temp = new char[std::strlen(str) + 1]{};
		n = SPLIT_STRING_INIT_COUNT;
		pindex = new int[n]{};

		i = 0;
		j = 0;
		*count = 0;
		while (str[i]) {
			while (str[i] == ' ' || str[i] == '\t') i++;
			if (!str[i]) break;
			if (*count >= n) {
				unsigned int new_n = n + SPLIT_STRING_INCREASEMENT;
				int* new_pindex = new int[new_n]{};
				std::memcpy(new_pindex, pindex, n * sizeof(int));
				delete[] pindex;
				pindex = new_pindex;
				n = new_n;
			}
			pindex[*count] = j;
			(*count)++;
			if (str[i] == '"') {
				i++;
				while (str[i]) {
					if (str[i] == '\\') {
						i++;
						if (!str[i]) break;
					}
					else if (str[i] == '"') {
						i++;
						break;
					}
					temp[j++] = str[i++];
				}
			}
			else {
				while (str[i] && str[i] != ' ' && str[i] != '\t') {
					temp[j++] = str[i++];
				}
			}
			temp[j++] = '\0';
		}
		index_size = *count * sizeof(char *);
		if (!index_size) {
			delete[] temp;
			delete[] pindex;
			return NULL;
		}
		result = new char[static_cast<std::size_t>(j) + index_size]{};
		std::memcpy(result + index_size, temp, static_cast<std::size_t>(j));

		ptrindex = new void*[*count]{};
		for (i = 0; i < *count; i++) {
			ptrindex[i] = result + index_size + pindex[i];
		}
		std::memcpy(result, ptrindex, index_size);
		delete[] temp;
		delete[] pindex;
		delete[] ptrindex;
		return reinterpret_cast<char **>(result);
	}

#define COMBINE_STRING_INIT_LEN		1024
#define COMBINE_STRING_INCREASEMENT	1024
	extern char * arraytostr(char * * array, char const * delim, int count)
	{
		int	i;
		unsigned int n;
		char	* result;
		int	need_delim;

		if (!delim || !array) return NULL;

		n = COMBINE_STRING_INIT_LEN;
		result = new char[n]{};
		result[0] = '\0';

		need_delim = 0;
		for (i = 0; i < count; i++) {
			if (!array[i]) continue;
			if (std::strlen(result) + std::strlen(array[i]) + std::strlen(delim) >= n) {
				unsigned int new_n = n + COMBINE_STRING_INCREASEMENT;
				char* new_result = new char[new_n]{};
				std::memcpy(new_result, result, n);
				delete[] result;
				result = new_result;
				n = new_n;
			}
			if (need_delim) {
				std::strcat(result, delim);
			}
			std::strcat(result, array[i]);
			need_delim = 1;
		}
		/* shrink-fit final result */
		{
			std::size_t final_len = std::strlen(result) + 1;
			char* shrunk = new char[final_len];
			std::memcpy(shrunk, result, final_len);
			delete[] result;
			result = shrunk;
		}
		return result;
	}


	// You must free the result if result is non-NULL.
	extern char *str_replace(char *orig, char *rep, char *with)
	{
		if (!orig)
			return nullptr;

		const char *rep_s = rep ? rep : "";
		const std::size_t len_rep = std::strlen(rep_s);

		const char *with_s = with ? with : "";
		const std::size_t len_with = std::strlen(with_s);

		// str_replace requires a non-empty needle; an empty needle would
		// never advance and loop forever.
		if (len_rep == 0)
			return nullptr;

		// number of replacements
		std::size_t count = 0;

		// next insert point
		const char *ins = orig;
		for (const char *tmp; (tmp = std::strstr(ins, rep_s)); ++count)
			ins = tmp + len_rep;

		// the return string
		char *result = new char[std::strlen(orig) + (len_with - len_rep) * count + 1];
		char *out = result;

		// from here on,
		//    out points to the end of the result string
		//    orig points to the remainder of orig after "end of rep"
		while (count--)
		{
			const char *match = std::strstr(orig, rep_s);
			const std::size_t len_front = static_cast<std::size_t>(match - orig);
			std::memcpy(out, orig, len_front);
			out += len_front;
			std::memcpy(out, with_s, len_with);
			out += len_with;
			orig += len_front + len_rep; // move to next "end of rep"
		}
		std::strcpy(out, orig);
		return result;
	}


	/* Replace "\n" in string to a new line character '\n' */
	extern std::string str_replace_nl(char const * text)
	{
		std::string s(text);
		size_t pos = 0;
		while ((pos = s.find("\\n", pos)) != std::string::npos) {
			s.replace(pos, 2, "\n");
		}
		return s;
	}


	// search substring in input string
	// (case insensitive)
	extern bool find_substr(char * input, const char * find)
	{
		std::string str1(input);
		std::string str2(find);

		return std::lexicographical_compare(
			str1.begin(), str1.end(),
			str2.begin(), str2.end(),
			[](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
		);
	}
}
