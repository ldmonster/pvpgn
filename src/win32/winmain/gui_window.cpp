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

// gui_window.cpp — Win32 window creation, message loop, painting, sizing,
//                  and mouse-interaction handlers.

#ifdef WIN32_GUI

#pragma warning(disable : 4047)

#include "common/setup_before.h"
#include "../winmain.h"

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

#include <windows.h>
#include <windowsx.h>
#include <winuser.h>
#include <process.h>
#include <richedit.h>
#include <commctrl.h>

#include "common/eventlog.h"
#include "common/version.h"

#include "../resource.h"
#include "../console_output.h"

#include "common/setup_after.h"

#include "gui_state.hpp"

namespace pvpgn
{

    HWND ghwndConsole = nullptr;

    namespace bnetd
    {

        struct gui_struc gui;
        char selected_item[255] = {};

        // ----------------------------------------------------------------
        // guiThread — registers the window class, creates the main window,
        //             and runs the Win32 message loop on a dedicated thread.
        // ----------------------------------------------------------------
        static void guiThread(void *param)
        {
            HMODULE hRichEd = LoadLibraryW(L"RichEd20.dll");
            if (hRichEd == nullptr)
                guiDEAD(L"Could not load RichEd20.dll");

            WNDCLASSEXW wc = {};
            wc.cbSize        = sizeof(WNDCLASSEXW);
            wc.style         = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc   = static_cast<WNDPROC>(guiWndProc);
            wc.cbClsExtra    = 0;
            wc.cbWndExtra    = 0;
            wc.hInstance     = static_cast<HINSTANCE>(param);
            wc.hIcon         = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(IDI_ICON1));
            wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            wc.lpszMenuName  = MAKEINTRESOURCEW(IDR_MENU);
            wc.lpszClassName = L"BnetdWndClass";
            wc.hIconSm       = nullptr;

            if (!RegisterClassExW(&wc))
            {
                FreeLibrary(hRichEd);
                guiDEAD(L"cant register WNDCLASS");
            }

            gui.hwnd = CreateWindowExW(
                0,
                wc.lpszClassName,
                L"Player -vs- Player Gaming Network Server",
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                nullptr,
                nullptr,
                static_cast<HINSTANCE>(param),
                nullptr);

            if (!gui.hwnd)
            {
                FreeLibrary(hRichEd);
                guiDEAD(L"cant create window");
            }

            ShowWindow(gui.hwnd, SW_SHOW);
            SetEvent(gui.event_ready);

            MSG msg = {};
            while (GetMessageW(&msg, nullptr, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            FreeLibrary(hRichEd);
        }

        // ----------------------------------------------------------------
        // guiWndProc — main window procedure; dispatches Win32 messages.
        // ----------------------------------------------------------------
        LRESULT CALLBACK guiWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            gui.wParam = wParam;
            gui.lParam = lParam;

            switch (message)
            {
                HANDLE_MSG(hwnd, WM_CREATE,      guiOnCreate);
                HANDLE_MSG(hwnd, WM_COMMAND,     guiOnCommand);
                HANDLE_MSG(hwnd, WM_MENUSELECT,  guiOnMenuSelect);
                HANDLE_MSG(hwnd, WM_SIZE,        guiOnSize);
                HANDLE_MSG(hwnd, WM_CLOSE,       guiOnClose);
                HANDLE_MSG(hwnd, WM_PAINT,       guiOnPaint);
                HANDLE_MSG(hwnd, WM_SETCURSOR,   guiOnSetCursor);
                HANDLE_MSG(hwnd, WM_LBUTTONDOWN, guiOnLButtonDown);
                HANDLE_MSG(hwnd, WM_LBUTTONUP,   guiOnLButtonUp);
                HANDLE_MSG(hwnd, WM_MOUSEMOVE,   guiOnMouseMove);

            case WM_CAPTURECHANGED:
                guiOnCaptureChanged(reinterpret_cast<HWND>(lParam));
                return 0;
            case WM_SHELLNOTIFY:
                return guiOnShellNotify(wParam, lParam);
            }

            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        // ----------------------------------------------------------------
        // guiOnCreate — creates all child windows (console, user list,
        //               edit button, user count label).
        // ----------------------------------------------------------------
        static BOOL guiOnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
        {
            ghwndConsole = CreateWindowExW(
                0,
                RICHEDIT_CLASS,
                nullptr,
                WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | WS_VSCROLL | WS_HSCROLL | ES_NOHIDESEL,
                0, 0,
                0, 0,
                hwnd,
                0,
                0,
                nullptr);

            if (!ghwndConsole)
                return FALSE;

            gui.hwndUsers = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"LISTBOX",
                nullptr,
                WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_NOINTEGRALHEIGHT,
                0, 0,
                0, 0,
                hwnd,
                0,
                0,
                nullptr);

            if (!gui.hwndUsers)
                return FALSE;

            // amadeo: temp. button for useredit until right-click is working
            gui.hwndUserEditButton = CreateWindowExW(
                0L,
                L"button",
                L"Edit User Status",
                WS_CHILD | WS_VISIBLE | ES_LEFT,
                0, 0,
                0, 0,
                hwnd,
                reinterpret_cast<HMENU>(881),
                0,
                nullptr);

            if (!gui.hwndUserEditButton)
                return FALSE;

            gui.hwndUserCount = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"edit",
                L" 0 user(s) online:",
                WS_CHILD | WS_VISIBLE | ES_CENTER | ES_READONLY,
                0, 0,
                0, 0,
                hwnd,
                0,
                0,
                nullptr);

            if (!gui.hwndUserCount)
                return FALSE;

            SendMessageW(gui.hwndUserCount,      WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), 0);
            SendMessageW(gui.hwndUsers,          WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), 0);
            SendMessageW(gui.hwndUserEditButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), 0);
            BringWindowToTop(gui.hwndUsers);
            std::snprintf(gui.szDefaultStatus, sizeof(gui.szDefaultStatus), "%s", "Void");

            gui.y_ratio = (100 << 10) / 100;
            gui.x_ratio = (0   << 10) / 100;

            return TRUE;
        }

        // ----------------------------------------------------------------
        // guiOnPaint — draws divider edges and refreshes child windows.
        // ----------------------------------------------------------------
        static void guiOnPaint(HWND hwnd)
        {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);

            DrawEdge(dc, &gui.rectHDivider,    BDR_SUNKEN, BF_MIDDLE);
            DrawEdge(dc, &gui.rectConsoleEdge, BDR_SUNKEN, BF_RECT);
            DrawEdge(dc, &gui.rectUsersEdge,   BDR_SUNKEN, BF_RECT);

            EndPaint(hwnd, &ps);

            UpdateWindow(ghwndConsole);
            UpdateWindow(gui.hwndUsers);
            UpdateWindow(gui.hwndUserCount);
            UpdateWindow(gui.hwndUserEditButton);
        }

        // ----------------------------------------------------------------
        // guiOnClose — kills tray icon and either exits or signals main.
        // ----------------------------------------------------------------
        static void guiOnClose(HWND hwnd)
        {
            guiKillTrayIcon();
            if (!gui.main_finished)
            {
                eventlog(eventlog_level_debug, __FUNCTION__, "GUI wants server dead...");
                std::exit(0);
            }
            else
            {
                eventlog(eventlog_level_debug, __FUNCTION__, "GUI wants to exit...");
                eventlog_close();
                SetEvent(gui.event_ready);
            }
        }

        // ----------------------------------------------------------------
        // guiOnSize — repositions child windows; handles minimize-to-tray
        //             and restore-from-tray transitions.
        // ----------------------------------------------------------------
        static void guiOnSize(HWND hwnd, UINT state, int cx, int cy)
        {
            if (state == SIZE_MINIMIZED)
            {
                NOTIFYICONDATAW dta = {};
                dta.cbSize          = sizeof(NOTIFYICONDATAW);
                dta.hWnd            = hwnd;
                dta.uID             = IDI_TRAY;
                dta.uFlags          = NIF_ICON | NIF_MESSAGE | NIF_TIP;
                dta.uCallbackMessage = WM_SHELLNOTIFY;
                dta.hIcon           = LoadIconW(GetWindowInstance(hwnd), MAKEINTRESOURCE(IDI_ICON1));
                std::swprintf(dta.szTip, sizeof dta.szTip / sizeof *dta.szTip,
                              L"%ls %ls", PVPGN_SOFTWAREW, PVPGN_VERSIONW);
                Shell_NotifyIconW(NIM_ADD, &dta);
                ShowWindow(hwnd, SW_HIDE);
                return;
            }

            if (state == SIZE_RESTORED)
            {
                NOTIFYICONDATAW dta = {};
                dta.hWnd = hwnd;
                dta.uID  = IDI_TRAY;
                Shell_NotifyIconW(NIM_DELETE, &dta);
            }

            int cy_status = 0;
            int cy_edge   = GetSystemMetrics(SM_CYEDGE);
            int cx_edge   = GetSystemMetrics(SM_CXEDGE);
            int cy_frame  = (cy_edge << 1) + GetSystemMetrics(SM_CYBORDER) + 1;
            int cy_console = ((cy - cy_status - cy_frame - cy_edge * 2) * gui.y_ratio) >> 10;

            gui.rectConsoleEdge.left   = 0;
            gui.rectConsoleEdge.right  = cx - 140;
            gui.rectConsoleEdge.top    = 0;
            gui.rectConsoleEdge.bottom = cy - cy_status;

            gui.rectConsole.left   = cx_edge;
            gui.rectConsole.right  = cx - 140 - cx_edge;
            gui.rectConsole.top    = cy_edge;
            gui.rectConsole.bottom = cy - cy_status;

            gui.rectUsersEdge.left   = cx - 140;
            gui.rectUsersEdge.top    = 18;
            gui.rectUsersEdge.right  = cx;
            gui.rectUsersEdge.bottom = cy - cy_status - 10;

            gui.rectUsers.left   = cx - 138;
            gui.rectUsers.right  = cx;
            gui.rectUsers.top    = 18 + cy_edge;
            gui.rectUsers.bottom = cy - cy_status - 20;

            guiMoveWindow(ghwndConsole,          &gui.rectConsole);
            guiMoveWindow(gui.hwndUsers,         &gui.rectUsers);
            MoveWindow(gui.hwndUserCount,      cx - 140, 0,                  140, 18, TRUE);
            MoveWindow(gui.hwndUserEditButton, cx - 140, cy - cy_status - 20, 140, 20, TRUE);
        }

        // ----------------------------------------------------------------
        // guiOnSetCursor — shows resize cursor over the horizontal divider.
        // ----------------------------------------------------------------
        static BOOL guiOnSetCursor(HWND hwnd, HWND hwndCursor, UINT codeHitTest, UINT msg)
        {
            POINT p;
            if (hwnd == hwndCursor && codeHitTest == HTCLIENT)
            {
                GetCursorPos(&p);
                ScreenToClient(hwnd, &p);
                if (PtInRect(&gui.rectHDivider, p))
                    SetCursor(LoadCursorW(0, IDC_SIZENS));
                return TRUE;
            }
            return FORWARD_WM_SETCURSOR(hwnd, hwndCursor, codeHitTest, msg, DefWindowProcW);
        }

        // ----------------------------------------------------------------
        // Mouse drag handlers — allow resizing the console/user-list split.
        // ----------------------------------------------------------------
        static void guiOnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
        {
            POINT p = { x, y };
            if (PtInRect(&gui.rectHDivider, p))
            {
                SetCapture(hwnd);
                gui.mode |= MODE_HDIVIDE;
            }
        }

        static void guiOnLButtonUp(HWND hwnd, int x, int y, UINT keyFlags)
        {
            ReleaseCapture();
            gui.mode &= ~(MODE_HDIVIDE | MODE_VDIVIDE);
        }

        static void guiOnCaptureChanged(HWND hwndNewCapture)
        {
            gui.mode &= ~(MODE_HDIVIDE | MODE_VDIVIDE);
        }

        static void guiOnMouseMove(HWND hwnd, int x, int y, UINT keyFlags)
        {
            if (gui.mode & MODE_HDIVIDE)
            {
                int offset    = y - gui.rectHDivider.top;
                if (!offset) return;

                int cy_console = gui.rectConsole.bottom - gui.rectConsole.top;
                int cy_users   = gui.rectUsers.bottom   - gui.rectUsers.top;

                if (cy_console + offset <= 0)
                    offset = -cy_console;
                else if (cy_users - offset <= 0)
                    offset = cy_users;

                cy_console += offset;
                cy_users   -= offset;
                if (cy_console + cy_users == 0) return;

                gui.y_ratio = (cy_console << 10) / (cy_console + cy_users);

                RECT r;
                GetClientRect(hwnd, &r);
                guiOnSize(hwnd, 0, r.right, r.bottom);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }

        // ----------------------------------------------------------------
        // guiMoveWindow — thin wrapper around MoveWindow for RECT structs.
        // ----------------------------------------------------------------
        static void guiMoveWindow(HWND hwnd, RECT* r)
        {
            MoveWindow(hwnd, r->left, r->top, r->right - r->left, r->bottom - r->top, TRUE);
        }

    } // namespace bnetd

} // namespace pvpgn

#endif // WIN32_GUI
