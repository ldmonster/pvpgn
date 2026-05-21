/*
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

#ifndef INCLUDED_FRIENDS_H
#define INCLUDED_FRIENDS_H

#define FRIEND_UNLOADEDMUTUAL -1
#define FRIEND_NOTMUTUAL 0
#define FRIEND_ISMUTUAL 1

#include <vector>
#include "account.h"

namespace pvpgn
{

	namespace bnetd
	{
		typedef struct friend_struct
		{
			/*	FRIEND_UNLOADEDMUTUAL:	unloaded(used to remove deleted elems when reload)
			*	FRIEND_NOTMUTUAL:		not mutual
			*	FRIEND_ISMUTUAL:		is mutual
			*/
			int mutual;
			t_account *friendacc = nullptr;
		} t_friend;

#ifndef JUST_NEED_TYPES

		extern t_account * friend_get_account(t_friend *);
		extern int friend_set_account(t_friend *, t_account *);
		extern char friend_get_mutual(t_friend *);
		extern int friend_set_mutual(t_friend *, int);

		extern int friendlist_unload(std::vector<t_friend*>&);
		extern int friendlist_close(std::vector<t_friend*>&);
		extern int friendlist_purge(std::vector<t_friend*>&);
		extern int friendlist_add_account(std::vector<t_friend*>&, t_account *, int);
		extern int friendlist_remove_friend(std::vector<t_friend*>&, t_friend *);
		extern int friendlist_remove_account(std::vector<t_friend*>&, t_account *);
		extern int friendlist_remove_username(std::vector<t_friend*>&, const char *);
		extern t_friend * friendlist_find_account(std::vector<t_friend*>&, t_account *);
		extern t_friend * friendlist_find_username(std::vector<t_friend*>&, const char *);
		extern t_friend * friendlist_find_uid(std::vector<t_friend*>&, unsigned int);

#endif

	}

}

#endif
