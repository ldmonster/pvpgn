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

// gui_dialogs.cpp — Modal dialog procedures:
//   AboutDlgProc   — simple "About" info box
//   AnnDlgProc     — server-wide announcement sender
//   KickDlgProc    — kick / ban / status-change user dialog

#ifdef WIN32_GUI

#pragma warning(disable : 4047)

#include "common/setup_before.h"
#include "../winmain.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>
#include <windowsx.h>
#include <winuser.h>

#include "common/addr.h"
#include "common/list.h"

#include "bnetd/connection.h"
#include "bnetd/account.h"
#include "bnetd/account_wrap.h"
#include "bnetd/ipban.h"
#include "bnetd/message.h"

#include "../resource.h"

#include "common/setup_after.h"

#include "gui_state.hpp"

namespace pvpgn
{

    namespace bnetd
    {

        // ----------------------------------------------------------------
        // AboutDlgProc — minimal About dialog; closes on IDOK.
        // ----------------------------------------------------------------
        INT_PTR AboutDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
        {
            switch (Message)
            {
            case WM_INITDIALOG:
                return TRUE;
            case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                case IDOK:
                    EndDialog(hwnd, IDOK);
                    break;
                }
                break;
            default:
                return FALSE;
            }
            return TRUE;
        }

        // ----------------------------------------------------------------
        // AnnDlgProc — reads text from IDC_EDIT1 and broadcasts it as a
        //              server-wide error message to all connected clients.
        // ----------------------------------------------------------------
        INT_PTR AnnDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
        {
            switch (Message)
            {
            case WM_INITDIALOG:
                return TRUE;
            case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                case IDOK:
                {
                    int len = GetWindowTextLengthA(GetDlgItem(hwnd, IDC_EDIT1));
                    if (len > 0)
                    {
                        std::string buff(len, '\0');
                        GetDlgItemTextA(hwnd, IDC_EDIT1, &buff[0], buff.capacity());

                        t_message* const message = message_create(message_type_error, nullptr, buff.c_str());
                        if (message)
                        {
                            message_send_all(message);
                            message_destroy(message);
                        }

                        SetDlgItemTextW(hwnd, IDC_EDIT1, L"");
                    }
                    else
                    {
                        MessageBoxW(hwnd, L"You didn't enter anything!", L"Warning", MB_OK);
                    }
                    break;
                }
                }
                break;
            case WM_CLOSE:
                EndDialog(hwnd, IDOK);
                break;
            default:
                return FALSE;
            }
            return TRUE;
        }

        // ----------------------------------------------------------------
        // KickDlgProc — allows the operator to kick, IP-ban, and/or change
        //               the status (admin / operator / announce) of a user.
        // ----------------------------------------------------------------
        INT_PTR KickDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
        {
            switch (Message)
            {
            case WM_INITDIALOG:
                if (selected_item[0] != 0)
                    SetDlgItemTextA(hwnd, IDC_EDITKICK, selected_item);
                return TRUE;

            case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                case IDC_KICK_EXECUTE:
                {
                    GetDlgItemTextA(hwnd, IDC_EDITKICK, selected_item, sizeof selected_item);

                    t_connection* const conngui    = connlist_find_connection_by_accountname(selected_item);
                    t_account*    const accountgui = accountlist_find_account(selected_item);

                    if (conngui == nullptr)
                    {
                        MessageBoxA(hwnd,
                            std::string(std::string(selected_item) + " could not be found in Userlist!").c_str(),
                            "Error", MB_OK);
                    }
                    else
                    {
                        HWND hButton  = GetDlgItem(hwnd, IDC_CHECKBAN);
                        HWND hButton1 = GetDlgItem(hwnd, IDC_CHECKKICK);
                        HWND hButton2 = GetDlgItem(hwnd, IDC_CHECKADMIN);
                        HWND hButton3 = GetDlgItem(hwnd, IDC_CHECKMOD);
                        HWND hButton4 = GetDlgItem(hwnd, IDC_CHECKANN);

                        BOOL messageq = FALSE;
                        BOOL kickq    = FALSE;

                        if (SendMessageW(hButton2, BM_GETCHECK, 0, 0) == BST_CHECKED)
                        {
                            account_set_admin(accountgui);
                            account_set_command_groups(accountgui, 255);
                            messageq = TRUE;
                        }

                        if (SendMessageW(hButton3, BM_GETCHECK, 0, 0) == BST_CHECKED)
                        {
                            account_set_auth_operator(accountgui, nullptr, 1);
                            messageq = TRUE;
                        }

                        if (SendMessageW(hButton4, BM_GETCHECK, 0, 0) == BST_CHECKED)
                        {
                            account_set_strattr(accountgui, "BNET\\auth\\announce", "true");
                            messageq = TRUE;
                        }

                        if (SendMessageW(hButton, BM_GETCHECK, 0, 0) == BST_CHECKED)
                        {
                            char temp[64];
                            std::snprintf(temp, sizeof temp, "%s",
                                          addr_num_to_addr_str(conn_get_addr(conngui), 0));

                            char ipadr[110];
                            unsigned int i_GUI;
                            for (i_GUI = 0; temp[i_GUI] != ':'; i_GUI++)
                                ipadr[i_GUI] = temp[i_GUI];
                            ipadr[i_GUI] = 0;

                            std::strcpy(temp, " a ");
                            std::strcat(temp, ipadr);
                            handle_ipban_command(nullptr, temp);

                            temp[0] = 0;
                            std::strcpy(temp, " has been added to IpBanList");
                            std::strcat(ipadr, temp);

                            if (messageq == TRUE)
                            {
                                std::strcat(ipadr, " and UserStatus changed");
                                MessageBoxA(hwnd, ipadr, "ipBan & StatusChange", MB_OK);
                                messageq = FALSE;
                                kickq    = FALSE;
                            }
                            else
                                MessageBoxA(hwnd, ipadr, "ipBan", MB_OK);
                        }

                        if (SendMessageW(hButton1, BM_GETCHECK, 0, 0) == BST_CHECKED)
                        {
                            conn_set_state(conngui, conn_state_destroy);
                            kickq = TRUE;
                        }

                        if ((messageq == TRUE) && (kickq == TRUE))
                        {
                            std::strcat(selected_item, "has been kicked and Status has changed");
                            MessageBoxA(hwnd, selected_item, "UserKick & StatusChange", MB_OK);
                        }

                        if ((kickq == TRUE) && (messageq == FALSE))
                        {
                            std::strcat(selected_item, " has been kicked from the server");
                            MessageBoxA(hwnd, selected_item, "UserKick", MB_OK);
                        }

                        if ((kickq == FALSE) && (messageq == TRUE))
                        {
                            std::strcat(selected_item, "'s Status has been changed");
                            MessageBoxA(hwnd, selected_item, "StatusChange", MB_OK);
                        }

                        selected_item[0] = 0;
                    }
                    break;
                }
                }
                break;

            case WM_CLOSE:
                EndDialog(hwnd, IDC_EDITKICK);
                break;
            default:
                return FALSE;
            }
            return TRUE;
        }

    } // namespace bnetd

} // namespace pvpgn

#endif // WIN32_GUI
