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

#pragma once

#ifdef WIN32_GUI

#include <windows.h>

#define WM_SHELLNOTIFY  (WM_USER+1)
#define MODE_HDIVIDE    1
#define MODE_VDIVIDE    1

namespace pvpgn
{

    extern HWND ghwndConsole;

    namespace bnetd
    {

        struct gui_struc {
            HWND    hwnd;
            HMENU   hmenuTray;
            HWND    hwndUsers;
            HWND    hwndUserCount;
            HWND    hwndUserEditButton;
            HWND    hwndTree;
            int     y_ratio;
            int     x_ratio;
            HANDLE  event_ready;
            BOOL    main_finished;
            int     mode;
            char    szDefaultStatus[128];
            RECT    rectHDivider,
                    rectVDivider,
                    rectConsole,
                    rectUsers,
                    rectConsoleEdge,
                    rectUsersEdge;
            WPARAM  wParam;
            LPARAM  lParam;
        };

        extern struct gui_struc gui;
        extern char selected_item[255];

        // Forward declarations — window thread & procedure
        static void guiThread(void*);
        LRESULT CALLBACK guiWndProc(HWND, UINT, WPARAM, LPARAM);

        // Window event handlers
        static BOOL guiOnCreate(HWND, LPCREATESTRUCT);
        static void guiOnCommand(HWND, int, HWND, UINT);
        static void guiOnMenuSelect(HWND, HMENU, int, HMENU, UINT);
        static int  guiOnShellNotify(int, int);
        static void guiOnClose(HWND);
        static void guiOnSize(HWND, UINT, int, int);
        static void guiOnPaint(HWND);
        static BOOL guiOnSetCursor(HWND, HWND, UINT, UINT);
        static void guiOnCaptureChanged(HWND);
        static void guiOnMouseMove(HWND, int, int, UINT);
        static void guiOnLButtonDown(HWND, BOOL, int, int, UINT);
        static void guiOnLButtonUp(HWND, int, int, UINT);

        // Command / action handlers
        static void guiOnServerConfig();
        static void guiOnAbout(HWND);
        static void guiOnUpdates();
        static void guiOnAnnounce(HWND);
        static void guiOnUserStatusChange(HWND);
        extern void guiOnUpdateUserList();

        // Log / utility helpers
        static void guiAddText(const char*, COLORREF);
        static void guiDEAD(const std::wstring& msg);
        static void guiMoveWindow(HWND, RECT*);
        static void guiClearLogWindow();
        static void guiKillTrayIcon();

        // Dialog procedures
        INT_PTR AboutDlgProc(HWND, UINT, WPARAM, LPARAM);
        INT_PTR AnnDlgProc(HWND, UINT, WPARAM, LPARAM);
        INT_PTR KickDlgProc(HWND, UINT, WPARAM, LPARAM);

    } // namespace bnetd

} // namespace pvpgn

#endif // WIN32_GUI
