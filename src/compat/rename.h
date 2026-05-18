/*
 * Copyright (C) 2004 CreepLord (creeplord@pvpgn.org)
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
#ifndef INCLUDED_RENAME_PROTOS
#define INCLUDED_RENAME_PROTOS

/*
 * Legacy POSIX `rename` errors out on Windows when the destination
 * already exists; the historical workaround was to unlink the
 * destination first using `access()` + `std::remove()`.
 *
 * C++17 `std::filesystem::rename` is specified to overwrite the
 * destination atomically on all platforms, so the workaround is
 * gone.  `p_rename` keeps the legacy 0/-1 return convention so call
 * sites do not have to change.
 */

#include <filesystem>
#include <system_error>

namespace pvpgn
{

	static inline int p_rename(const char * oldpath, const char * newpath)
	{
		std::error_code ec;
		std::filesystem::rename(oldpath, newpath, ec);
		return ec ? -1 : 0;
	}

}

#endif
