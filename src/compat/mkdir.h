/*
 * Copyright (C) 2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
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
#ifndef INCLUDED_MKDIR_PROTOS
#define INCLUDED_MKDIR_PROTOS

/*
 * Legacy bnetd / d2cs / d2dbs used to spell directory creation
 * either as POSIX `mkdir(path, mode)` or MSVC `_mkdir(path)`. This
 * header used to paper over the difference with a `p_mkdir` macro
 * gated on a battery of HAVE_* feature tests.
 *
 * Modern toolchains have C++17 `<filesystem>`, which abstracts the
 * platform difference natively. `p_mkdir` is now a thin inline
 * wrapper around `std::filesystem::create_directory` that keeps the
 * legacy 0/-1 return convention so call sites do not have to change.
 *
 * The `mode` argument is accepted for source compatibility but has
 * no effect: `<filesystem>` creates directories with the OS-default
 * mode and then the process umask is applied, which matches what
 * the legacy POSIX `mkdir(path, 0777)` calls produced once umask
 * was factored in. Sites that need a specific mode must call
 * `std::filesystem::permissions` after creation.
 */

#include <filesystem>
#include <string>
#include <system_error>

namespace pvpgn
{

	static inline int p_mkdir(const char * path)
	{
		std::error_code ec;
		std::filesystem::create_directory(path, ec);
		return ec ? -1 : 0;
	}

	static inline int p_mkdir(const std::string & path)
	{
		return p_mkdir(path.c_str());
	}

}

#endif
