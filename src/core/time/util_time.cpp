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
// Time/clock string formatting and parsing utilities (plan 15 §3 / SOLID-S).
// Included as a sub-TU by util.cpp — do not compile directly.

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace pvpgn
{

	extern char const * seconds_to_timestr(unsigned int totsecs)
	{
		static char temp[256];
		int         days;
		int         hours;
		int         minutes;
		int         seconds;

		days = totsecs / (24 * 60 * 60);
		hours = (totsecs / (60 * 60)) % 24;
		minutes = (totsecs / 60) % 60;
		seconds = totsecs % 60;

		if (days > 0)
			std::sprintf(temp, "%d day%s %d hour%s %d minute%s %d second%s",
			days, days == 1 ? "" : "s",
			hours, hours == 1 ? "" : "s",
			minutes, minutes == 1 ? "" : "s",
			seconds, seconds == 1 ? "" : "s");
		else if (hours > 0)
			std::sprintf(temp, "%d hour%s %d minute%s %d second%s",
			hours, hours == 1 ? "" : "s",
			minutes, minutes == 1 ? "" : "s",
			seconds, seconds == 1 ? "" : "s");
		else if (minutes > 0)
			std::sprintf(temp, "%d minute%s %d second%s",
			minutes, minutes == 1 ? "" : "s",
			seconds, seconds == 1 ? "" : "s");
		else
			std::sprintf(temp, "%d second%s.",
			seconds, seconds == 1 ? "" : "s");

		return temp;
	}


	extern int clockstr_to_seconds(char const * clockstr, unsigned int * totsecs)
	{
		unsigned int i, j;
		unsigned int temp;

		if (!clockstr)
			return -1;
		if (!totsecs)
			return -1;

		for (i = j = temp = 0; j < std::strlen(clockstr); j++)
		{
			switch (clockstr[j])
			{
			case ':':
				temp *= 60;
				temp += std::strtoul(&clockstr[i], NULL, 10);
				i = j + 1;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				break;
			default:
				return -1;
			}
		}
		if (i < j)
		{
			temp *= 60;
			temp += std::strtoul(&clockstr[i], NULL, 10);
		}

		*totsecs = temp;
		return 0;
	}


	/* convert a time string to time_t
	time string format is:
	yyyy/mm/dd or yyyy-mm-dd or yyyy.mm.dd
	hh:mm:ss
	*/
	extern int timestr_to_time(char const * timestr, std::time_t* ptime)
	{
		char const * p;
		char ch;
		struct std::tm when;
		int day_s, time_s, last;

		if (!timestr) return -1;
		if (!timestr[0]) {
			*ptime = 0;
			return 0;
		}

		p = timestr;
		day_s = time_s = 0;
		last = 0;
		std::memset(&when, 0, sizeof(when));
		when.tm_mday = 1;
		when.tm_isdst = -1;
		while (1) {
			ch = *timestr;
			timestr++;
			switch (ch) {
			case '/':
			case '-':
			case '.':
				if (day_s == 0) {
					when.tm_year = std::atoi(p) - 1900;
				}
				else if (day_s == 1) {
					when.tm_mon = std::atoi(p) - 1;
				}
				else if (day_s == 2) {
					when.tm_mday = std::atoi(p);
				}
				time_s = 0;
				day_s++;
				p = timestr;
				last = 1;
				break;
			case ':':
				if (time_s == 0) {
					when.tm_hour = std::atoi(p);
				}
				else if (time_s == 1) {
					when.tm_min = std::atoi(p);
				}
				else if (time_s == 2) {
					when.tm_sec = std::atoi(p);
				}
				day_s = 0;
				time_s++;
				p = timestr;
				last = 2;
				break;
			case ' ':
			case '\t':
			case '\x0':
				if (last == 1) {
					if (day_s == 0) {
						when.tm_year = std::atoi(p) - 1900;
					}
					else if (day_s == 1) {
						when.tm_mon = std::atoi(p) - 1;
					}
					else if (day_s == 2) {
						when.tm_mday = std::atoi(p);
					}
				}
				else if (last == 2) {
					if (time_s == 0) {
						when.tm_hour = std::atoi(p);
					}
					else if (time_s == 1) {
						when.tm_min = std::atoi(p);
					}
					else if (time_s == 2) {
						when.tm_sec = std::atoi(p);
					}
				}
				time_s = day_s = 0;
				p = timestr;
				last = 0;
				break;
			default:
				break;
			}
			if (!ch) break;
		}

		*ptime = std::mktime(&when);
		return 0;
	}

} // namespace pvpgn
