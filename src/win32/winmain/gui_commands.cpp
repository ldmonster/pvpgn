/*
 * Copyright (C) 2001  Erik Latoshek [forester] (laterk@inbox.lv)
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

// gui_commands.cpp — Menu command dispatcher and user-list management.

#ifdef WIN32_GUI

#pragma warning(disable : 4047)

#include "common/setup_before.h"
#include "../winmain.h"

#include <cstdio>
#include <string>

#include <windows.h>
#include <windowsx.h>
#include <winuser.h>

#include "common/list.h"
#include "common/eventlog.h"

#include "bnetd/connection.h"
#include "bnetd/account.h"
#include "bnetd/server.h"

#include "../resource.h"

#include "common/setup_after.h"

#include "gui_state.hpp"

namespace pvpgn
{

    namespace bnetd
    {

        // ----------------------------------------------------------------
        // guiOnCommand — dispatches WM_COMMAND menu/button IDs to the
        //                appropriate action handlers.
        // ----------------------------------------------------------------
        static void guiOnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
        {
            switch (id)
            {
            case IDM_EXIT:
                guiOnClose(hwnd);
                break;
            case IDM_SAVE:
                server_save_wraper();
                break;
            case IDM_RESTART_LUA:
                server_restart_wraper(restart_mode_lua);
                break;
            case IDM_RESTART:
                server_restart_wraper(restart_mode_all);
                break;
            case IDM_SHUTDOWN:
                server_quit_wraper();
                break;
            case IDM_CLEAR:
                guiClearLogWindow();
                break;
            case IDM_RESTORE:
                guiOnShellNotify(IDI_TRAY, WM_LBUTTONDBLCLK);
                break;
            case IDM_USERLIST:
                guiOnUpdateUserList();
                break;
            case IDM_SERVERCONFIG:
                guiOnServerConfig();
                break;
            case IDM_ABOUT:
                guiOnAbout(hwnd);
                break;
            case ID_HELP_CHECKFORUPDATES:
                guiOnUpdates();
                break;
            case IDM_ANN:
                guiOnAnnounce(hwnd);
                break;
            case ID_USERACTIONS_KICKUSER:
            case 881:
                guiOnUserStatusChange(hwnd);
                break;
            }
        }

        // ----------------------------------------------------------------
        // guiOnUpdates — opens the pvpgn.pro website in the default browser.
        // ----------------------------------------------------------------
        static void guiOnUpdates()
        {
            ShellExecuteW(nullptr, L"open", L"http://pvpgn.pro/", nullptr, nullptr, SW_SHOW);
        }

        // ----------------------------------------------------------------
        // guiOnAnnounce — opens the announcement dialog.
        // ----------------------------------------------------------------
        static void guiOnAnnounce(HWND hwnd)
        {
            DialogBoxW(GetModuleHandleW(nullptr),
                       MAKEINTRESOURCEW(IDD_ANN),
                       hwnd,
                       reinterpret_cast<DLGPROC>(AnnDlgProc));
        }

        // ----------------------------------------------------------------
        // guiOnUserStatusChange — opens the kick/status-change dialog for
        //                         the currently selected user.
        // ----------------------------------------------------------------
        static void guiOnUserStatusChange(HWND hwnd)
        {
            int index = SendMessageW(gui.hwndUsers, LB_GETCURSEL, 0, 0);
            SendMessageA(gui.hwndUsers, LB_GETTEXT, index,
                         reinterpret_cast<LPARAM>(selected_item));
            DialogBoxW(GetModuleHandleW(nullptr),
                       MAKEINTRESOURCEW(IDD_KICKUSER),
                       hwnd,
                       reinterpret_cast<DLGPROC>(KickDlgProc));
            SendMessageW(gui.hwndUsers, LB_SETCURSEL, -1, 0);
        }

        // ----------------------------------------------------------------
        // guiOnAbout — opens the About dialog.
        // ----------------------------------------------------------------
        static void guiOnAbout(HWND hwnd)
        {
            DialogBoxW(GetModuleHandleW(nullptr),
                       MAKEINTRESOURCEW(IDD_ABOUT),
                       hwnd,
                       reinterpret_cast<DLGPROC>(AboutDlgProc));
        }

        // ----------------------------------------------------------------
        // guiOnServerConfig — opens bnetd.conf in the default text editor.
        // ----------------------------------------------------------------
        static void guiOnServerConfig()
        {
            ShellExecuteW(nullptr, nullptr, L"conf\\bnetd.conf", nullptr, nullptr, SW_SHOW);
        }

        // ----------------------------------------------------------------
        // guiOnUpdateUserList — rebuilds the user list box from the live
        //                       connection list and updates the count label.
        // ----------------------------------------------------------------
        extern void guiOnUpdateUserList()
        {
            t_connection*    c;
            t_elem const*    curr;
            t_account*       acc;

            SendMessageW(gui.hwndUsers, LB_RESETCONTENT, 0, 0);

            LIST_TRAVERSE_CONST(connlist(), curr)
            {
                if (!(c = (t_connection *)elem_get_data(curr)))
                    continue;
                if (!(acc = conn_get_account(c)))
                    continue;
                SendMessageA(gui.hwndUsers, LB_ADDSTRING, 0,
                             reinterpret_cast<LPARAM>(account_get_name(acc)));
            }

            std::wstring user_count(
                std::to_wstring(connlist_login_get_length()) + L" user(s) online:");
            SendMessageW(gui.hwndUserCount, WM_SETTEXT, 0,
                         reinterpret_cast<LPARAM>(user_count.c_str()));
        }

    } // namespace bnetd

} // namespace pvpgn

#endif // WIN32_GUI
