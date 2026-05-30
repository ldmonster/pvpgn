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

// gui_tray.cpp — System-tray icon management and shell-notification handler.

#ifdef WIN32_GUI

#pragma warning(disable : 4047)

#include "common/setup_before.h"
#include "../winmain.h"

#include <windows.h>
#include <windowsx.h>
#include <winuser.h>

#include "../resource.h"

#include "common/setup_after.h"

#include "gui_state.hpp"

namespace pvpgn
{

    namespace bnetd
    {

        // ----------------------------------------------------------------
        // guiOnMenuSelect — placeholder for status-bar menu-item text.
        //                   (Currently a no-op; kept for future use.)
        // ----------------------------------------------------------------
        static void guiOnMenuSelect(HWND hwnd, HMENU hmenu, int item, HMENU hmenuPopup, UINT flags)
        {
            // Reserved for future status-bar integration.
        }

        // ----------------------------------------------------------------
        // guiOnShellNotify — handles WM_SHELLNOTIFY tray-icon callbacks:
        //   double-click  → restore window from tray
        //   right-click   → show context menu at cursor position
        // ----------------------------------------------------------------
        static int guiOnShellNotify(int uID, int uMessage)
        {
            if (uID == IDI_TRAY)
            {
                if (uMessage == WM_LBUTTONDBLCLK)
                {
                    if (!IsWindowVisible(gui.hwnd))
                        ShowWindow(gui.hwnd, SW_RESTORE);
                    SetForegroundWindow(gui.hwnd);
                }
                else if (uMessage == WM_RBUTTONDOWN)
                {
                    POINT cp;
                    GetCursorPos(&cp);
                    SetForegroundWindow(gui.hwnd);
                    TrackPopupMenu(gui.hmenuTray, TPM_LEFTALIGN | TPM_LEFTBUTTON,
                                   cp.x, cp.y, 0, gui.hwnd, NULL);
                }
            }
            return 0;
        }

        // ----------------------------------------------------------------
        // guiKillTrayIcon — removes the tray icon via Shell_NotifyIconW.
        // ----------------------------------------------------------------
        static void guiKillTrayIcon()
        {
            NOTIFYICONDATAW dta = {};
            dta.cbSize = sizeof(NOTIFYICONDATAW);
            dta.hWnd   = gui.hwnd;
            dta.uID    = IDI_TRAY;
            dta.uFlags = 0;
            Shell_NotifyIconW(NIM_DELETE, &dta);
        }

    } // namespace bnetd

} // namespace pvpgn

#endif // WIN32_GUI
