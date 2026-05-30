/*
 * Copyright (C) 2004	Aaron
 * Copyright (C) 2004	CreepLord (creeplord@pvpgn.org)
 * Copyright (C) 2007,2008	Pelish (pelish@gmail.com)
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
#include "common/setup_before.h"
#include "common/tag.h"

#include <cstring>
#include <cctype>
#include <string>

#include <strings.h>

#include "common/eventlog.h"
#include "common/xstring.h"
#include "common/setup_after.h"

// tag.cpp split into focused sub-modules (plan 15 §3 / SOLID-S)
#include "tag_core.cpp"
#include "tag_clienttag.cpp"
#include "tag_wol.cpp"
