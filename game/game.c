#ifndef TARGET_WINDOWS
# error This is Windows code, not DOS
#endif

/* Windows programming notes:
 *
 *   - If you're writing your software to work only on Windows 3.1 or later, you can omit the
 *     use of MakeProcInstance(). Windows 3.0 however require your code to use it.
 *
 *   - The Window procedure must be exported at link time. Windows 3.0 demands it.
 *
 *   - If you want your code to run in Windows 3.0, everything must be MOVEABLE. Your code and
 *     data segments must be MOVEABLE. If you intend to load resources, the resources must be
 *     MOVEABLE. The constraints of real mode and fitting everything into 640KB or less require
 *     it.
 *
 *   - If you want to keep your sanity, never mark your data segment (DGROUP) as discardable.
 *     You can make your code discardable because the mechanisms of calling your code by Windows
 *     including the MakeProcInstance()-generated wrapper for your windows proc will pull your
 *     code segment back in on demand. There is no documented way to pull your data segment back
 *     in if discarded. Notice all the programs in Windows 2.0/3.0 do the same thing.
 */
/* FIXME: This code crashes when multiple instances are involved. Especially the win386 build. */

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>
#include "resource.h"

#if (TARGET_MSDOS == 32 && defined(WIN386))
# include <win386.h>
#endif

// WndConfigFlags
#define WndCFG_ShowMenu			0x00000001u /* show the menu bar */
#define WndCFG_Fullscreen		0x00000002u /* run fullscreen */
#define WndCFG_TopMost			0x00000004u /* run with "top most" status (overtop other applications) */
#define WndCFG_DeactivateMinimize	0x00000008u /* minimize if the user switches away from the application */

// WndStateFlags
#define WndState_Minimized	0x00000001u
#define WndState_Active		0x00000002u

struct WndStyle_t {
	DWORD			style;
	DWORD			styleEx;
};

// NTS: Please make this AS UNIQUE AS POSSIBLE to your game. You could use UUIDGEN and make this a GUID if uninspired or busy, even.
const char near			WndProcClass[] = "GAME32JAM2025";

// NTS: Change this, obviously.
const char near			WndTitle[] = "Game";

// NTS: This is the menu resource ID that defines what menu appears in your window.
const UINT near			WndMenu = IDM_MAINMENU;

// style warnings:
// - Do not combine WS_EX_DLGMODALFRAME with a menu, minimize or maximize buttons
// - Do not use WS_CHILD, this is a game, not a UI element

// NTS: Your game's main window style. Not all styles are compatible with menus or other styles.
//      For more information see Windows SDK documentation regarding CreateWindowEx().
//      Extended styles are not availeble when targeting Windows 1.x and Windows 2.x.
const struct WndStyle_t		WndStyle = { .style = WS_OVERLAPPEDWINDOW, .styleEx = 0 };
//const struct WndStyle_t		WndStyle = { .style = WS_POPUPWINDOW|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_CAPTION, .styleEx = 0 };
//const struct WndStyle_t		WndStyle = { .style = WS_DLGFRAME|WS_CAPTION|WS_SYSMENU|WS_BORDER, .styleEx = WS_EX_DLGMODALFRAME };

HINSTANCE near			myInstance = (HINSTANCE)NULL;
HWND near			hwndMain = (HWND)NULL;
HMENU near			SysMenu = (HMENU)NULL;

// Window config (WndCFG_...) bitfield
BYTE near			WndConfigFlags = WndCFG_ShowMenu;
//BYTE near			WndConfigFlags = WndCFG_ShowMenu | WndCFG_TopMost;
//BYTE near			WndConfigFlags = WndCFG_ShowMenu | WndCFG_Fullscreen;
//BYTE near			WndConfigFlags = WndCFG_ShowMenu | WndCFG_Fullscreen | WndCFG_TopMost;
//BYTE near			WndConfigFlags = 0;
//BYTE near			WndConfigFlags = WndCFG_ShowMenu | WndCFG_DeactivateMinimize;

// NOTE: To prevent resizing completely, set the min, max, and def sizes to the exact same value
const POINT near		WndMinSizeClient = { 80, 60 };
POINT near			WndMinSize = { 0, 0 };
const POINT near		WndMaxSizeClient = { 480, 360 };
POINT near			WndMaxSize = { 0, 0 };
const POINT near		WndDefSizeClient = { 320, 240 };
POINT near			WndDefSize = { 0, 0 };

RECT near			WndFullscreenSize = { 0, 0, 0, 0 };
POINT near			WndCurrentSizeClient = { 0, 0 };
POINT near			WndCurrentSize = { 0, 0 };
POINT near			WndScreenSize = { 0, 0 };

// Window state (WndState_...) bitfield
BYTE near			WndStateFlags = 0;

void WinClientSizeInFullScreen(RECT *d,const struct WndStyle_t *style,const BOOL fMenu) {
	(void)fMenu;

	d->left = 0;
	d->top = 0;
	d->right = GetSystemMetrics(SM_CXSCREEN);
	d->bottom = GetSystemMetrics(SM_CYSCREEN);
	/* calculate the rect so that the window caption, sysmenu, and borders are just off the
	 * edges of the screen, leaving the client area to cover the screen, BUT, disregard the
	 * fMenu flag so that the menu bar, if any, is visible at the top of the screen. */
#if WINVER >= 0x300
	AdjustWindowRectEx(d,style->style,FALSE,style->styleEx);
#else
	AdjustWindowRect(d,style->style,FALSE);
#endif
}

void WinClientSizeToWindowSize(POINT *d,const POINT *s,const struct WndStyle_t *style,const BOOL fMenu) {
	RECT um;
	memset(&um,0,sizeof(um));
	um.right = s->x;
	um.bottom = s->y;
#if WINVER >= 0x300
	AdjustWindowRectEx(&um,style->style,fMenu,style->styleEx);
#else
	AdjustWindowRect(&um,style->style,fMenu);
#endif
	d->x = um.right - um.left;
	d->y = um.bottom - um.top;
}

#if (WINVER >= 0x300)
# define DeleteMenuGF DeleteMenu
#else
/* NTS: DeleteMenu() does exist in Windows 2.x but it only supports MF_BYPOSITION. This code needs MF_BYCOMMAND. */
BOOL DeleteMenuGF(HMENU hMenu,UINT idItem,UINT fuFlags) {
	/* Microsoft's Windows 3.1 SDK has forgotten about MF_REMOVE and does not mention it */
	return ChangeMenu(hMenu,idItem,NULL,0,MF_DELETE | fuFlags);
}
#endif

#if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && defined(WIN386))
LRESULT PASCAL FAR WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
#else
LRESULT WINAPI WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
#endif
	if (message == WM_CREATE) {
		return 0; /* Success */
	}
	else if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0; /* OK */
	}
	else if (message == WM_SIZE) {
		if (wparam == SIZE_MINIMIZED) {
			WndStateFlags |= WndState_Minimized;

			if (WndConfigFlags & WndCFG_TopMost) {
#if WINVER >= 0x200
				SetWindowPos(hwndMain,HWND_NOTOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE); // take away topmost
#endif
			}
		}
		else {
			WndStateFlags &= ~WndState_Minimized;
			WndCurrentSizeClient.x = LOWORD(lparam);
			WndCurrentSizeClient.y = HIWORD(lparam);
			WinClientSizeToWindowSize(&WndCurrentSize,&WndCurrentSizeClient,&WndStyle,GetMenu(hwnd)!=NULL?TRUE:FALSE);

			if (WndConfigFlags & WndCFG_TopMost) {
#if WINVER >= 0x200
				SetWindowPos(hwndMain,HWND_TOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE);
#endif
			}
		}

		return 0;
	}
	else if (message == WM_ACTIVATE) {
		if (wparam == WA_CLICKACTIVE || wparam == WA_ACTIVE) {
			WndStateFlags |= WndState_Active;
		}
		else {
			WndStateFlags &= ~WndState_Active;

			if (WndConfigFlags & WndCFG_DeactivateMinimize) {
#if WINVER >= 0x200
				ShowWindow(hwnd,SW_SHOWMINNOACTIVE);
#else
				ShowWindow(hwnd,SHOW_ICONWINDOW); /* Windows 1.0 */
#endif
			}
		}

		return 0;
	}
	else if (message == WM_INITMENUPOPUP) {
		if ((HMENU)wparam == SysMenu) {
			EnableMenuItem(SysMenu,SC_MOVE,MF_BYCOMMAND|((WndStateFlags & WndState_Minimized)?MF_ENABLED:(MF_DISABLED|MF_GRAYED)));
			EnableMenuItem(SysMenu,SC_RESTORE,MF_BYCOMMAND|((WndStateFlags & WndState_Minimized)?MF_ENABLED:(MF_DISABLED|MF_GRAYED)));
		}

		return DefWindowProc(hwnd,message,wparam,lparam);
	}
#ifdef WM_WINDOWPOSCHANGING
	else if (message == WM_WINDOWPOSCHANGING) {
#if (TARGET_MSDOS == 32 && defined(WIN386))
		WINDOWPOS FAR *wpc = (WINDOWPOS FAR*)MK_FP32((void*)lparam);
#else
		WINDOWPOS FAR *wpc = (WINDOWPOS FAR*)lparam;
#endif

		/* we must track if the window is minimized or else on Windows 3.1 our minimized icon will
		 * stay stuck in the upper left hand corner of the screen if fullscreen mode is active */
		if (IsIconic(hwnd))
			WndStateFlags |= WndState_Minimized;
		else
			WndStateFlags &= ~WndState_Minimized;

		if ((WndConfigFlags & WndCFG_Fullscreen) && !(WndStateFlags & WndState_Minimized)) {
			wpc->x = WndFullscreenSize.left;
			wpc->y = WndFullscreenSize.top;
		}

		return DefWindowProc(hwnd,message,wparam,lparam);
	}
#endif
#ifdef WM_GETMINMAXINFO
	else if (message == WM_GETMINMAXINFO) {
#if (TARGET_MSDOS == 32 && defined(WIN386))
		MINMAXINFO FAR *mmi = (MINMAXINFO FAR*)MK_FP32((void*)lparam);
#else
		MINMAXINFO FAR *mmi = (MINMAXINFO FAR*)lparam;
#endif

		/* we must track if the window is minimized or else on Windows 3.1 our minimized icon will
		 * stay stuck in the upper left hand corner of the screen if fullscreen mode is active */
		if (IsIconic(hwnd))
			WndStateFlags |= WndState_Minimized;
		else
			WndStateFlags &= ~WndState_Minimized;

		if ((WndConfigFlags & WndCFG_Fullscreen) && !(WndStateFlags & WndState_Minimized)) {
			mmi->ptMaxSize.x = WndFullscreenSize.right - WndFullscreenSize.left;
			mmi->ptMaxSize.y = WndFullscreenSize.bottom - WndFullscreenSize.top;
			mmi->ptMaxPosition.x = WndFullscreenSize.left;
			mmi->ptMaxPosition.y = WndFullscreenSize.top;
			mmi->ptMaxTrackSize = mmi->ptMaxSize;
			mmi->ptMinTrackSize = mmi->ptMaxSize;
		}
		else {
			if (mmi->ptMaxSize.x > WndMaxSize.x)
				mmi->ptMaxSize.x = WndMaxSize.x;
			if (mmi->ptMaxSize.y > WndMaxSize.y)
				mmi->ptMaxSize.y = WndMaxSize.y;
			if (mmi->ptMaxTrackSize.x > WndMaxSize.x)
				mmi->ptMaxTrackSize.x = WndMaxSize.x;
			if (mmi->ptMaxTrackSize.y > WndMaxSize.y)
				mmi->ptMaxTrackSize.y = WndMaxSize.y;
			if (mmi->ptMinTrackSize.x < WndMinSize.x)
				mmi->ptMinTrackSize.x = WndMinSize.x;
			if (mmi->ptMinTrackSize.y < WndMinSize.y)
				mmi->ptMinTrackSize.y = WndMinSize.y;

			/* if Windows maximizes our window, we want it centered on screen, not
			 * put into the upper left hand corner like it does by default */
			mmi->ptMaxPosition.x = (WndScreenSize.x - mmi->ptMaxSize.x) / 2;
			mmi->ptMaxPosition.y = (WndScreenSize.y - mmi->ptMaxSize.y) / 2;
		}
	}
#endif
#ifdef WM_SETCURSOR
	else if (message == WM_SETCURSOR) {
		if (LOWORD(lparam) == HTCLIENT) {
			SetCursor(LoadCursor(NULL,IDC_ARROW));
			return 1;
		}
		else {
			return DefWindowProc(hwnd,message,wparam,lparam);
		}
	}
#endif
	else if (message == WM_ERASEBKGND) {
		RECT um;

		if (GetUpdateRect(hwnd,&um,FALSE)) {
			HBRUSH oldBrush,newBrush;
			HPEN oldPen,newPen;

			newPen = (HPEN)GetStockObject(NULL_PEN);
			newBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);

			oldPen = SelectObject((HDC)wparam,newPen);
			oldBrush = SelectObject((HDC)wparam,newBrush);

			Rectangle((HDC)wparam,um.left,um.top,um.right+1,um.bottom+1);

			SelectObject((HDC)wparam,oldBrush);
			SelectObject((HDC)wparam,oldPen);
		}

		return 1; /* Important: Returning 1 signals to Windows that we processed the message. Windows 3.0 gets really screwed up if we don't! */
	}
	else if (message == WM_SYSCOMMAND) {
		switch (wparam) {
			case SC_MOVE:
				if ((WndConfigFlags & WndCFG_Fullscreen) && !(WndStateFlags & WndState_Minimized)) return 0;
				break;
			case SC_MINIMIZE:
				if (WndConfigFlags & WndCFG_TopMost) {
#if WINVER >= 0x200
					SetWindowPos(hwndMain,HWND_NOTOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE); // take away topmost
#endif
				}
				break;
			case SC_MAXIMIZE:
				if (WndConfigFlags & WndCFG_Fullscreen) {
#if WINVER >= 0x200
					// NTS: It is very important to SW_SHOWNORMAL then SW_SHOWMAXIMIZED or else Windows 3.0
					//      will only show the window in normal maximized dimensions
					if (WndStateFlags & WndState_Minimized) ShowWindow(hwnd,SW_SHOWNORMAL);
					ShowWindow(hwnd,SW_SHOWMAXIMIZED);
#endif
					return 0;
				}
				break;
			case SC_RESTORE:
				if (WndConfigFlags & WndCFG_Fullscreen) {
#if WINVER >= 0x200
					// NTS: It is very important to SW_SHOWNORMAL then SW_SHOWMAXIMIZED or else Windows 3.0
					//      will only show the window in normal maximized dimensions
					if (WndStateFlags & WndState_Minimized) ShowWindow(hwnd,SW_SHOWNORMAL);
					ShowWindow(hwnd,SW_SHOWMAXIMIZED);
#endif
					return 0;
				}
				break;
			default:
				break;
		}

		return DefWindowProc(hwnd,message,wparam,lparam);
	}
	else if (message == WM_COMMAND) {
		switch (wparam) {
			case IDC_QUIT:
				PostMessage(hwnd,WM_CLOSE,0,0);
				break;
			default:
				return DefWindowProc(hwnd,message,wparam,lparam);
		}

		return 1;
	}
	else if (message == WM_PAINT) {
		RECT um;

		if (GetUpdateRect(hwnd,&um,TRUE)) {
			PAINTSTRUCT ps;

			BeginPaint(hwnd,&ps);
			EndPaint(hwnd,&ps);
		}

		return 0; /* Return 0 to signal we processed the message */
	}
	else {
		return DefWindowProc(hwnd,message,wparam,lparam);
	}

	return 0;
}

/* NOTES:
 *   For Win16, programmers generally use WINAPI WinMain(...) but WINAPI is defined in such a way
 *   that it always makes the function prolog return FAR. Unfortunately, when Watcom C's runtime
 *   calls this function in a memory model that's compact or small, the call is made as if NEAR,
 *   not FAR. To avoid a GPF or crash on return, we must simply declare it PASCAL instead. */
int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	WNDCLASS wnd;
	MSG msg;

	(void)lpCmdLine; /* unused */

	myInstance = hInstance;

	/* NTS: In the Windows 3.1 environment all handles are global. Registering a class window twice won't work.
	 *      It's only under 95 and later (win32 environment) where Windows always sets hPrevInstance to 0
	 *      and window classes are per-application.
	 *
	 *      Windows 3.1 allows you to directly specify the FAR pointer. Windows 3.0 however demands you
	 *      MakeProcInstance it to create a 'thunk' so that Windows can call you (ick). */
	if (!hPrevInstance) {
		wnd.style = CS_HREDRAW|CS_VREDRAW;
		wnd.lpfnWndProc = (WNDPROC)WndProc;
		wnd.cbClsExtra = 0;
		wnd.cbWndExtra = 0;
		wnd.hInstance = hInstance;
		wnd.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_APPICON));
		wnd.hCursor = NULL;
		wnd.hbrBackground = NULL;
		wnd.lpszMenuName = NULL;
		wnd.lpszClassName = WndProcClass;

		if (!RegisterClass(&wnd)) {
			MessageBox(NULL,"Unable to register Window class","Oops!",MB_OK);
			return 1;
		}
	}
	else {
#if defined(WIN386)
		/* FIXME: Win386 builds will CRASH if multiple instances try to run this way.
		 *        Somehow, creating a window with a class registered by another Win386 application
		 *        causes Windows to hang. */
		if (MessageBox(NULL,"Win386 builds may crash if you run multiple instances. Continue?","",MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON2) == IDNO)
			return 1;
#endif
	}

	{
		HMENU menu = WndMenu!=0u?LoadMenu(hInstance,MAKEINTRESOURCE(WndMenu)):((HMENU)NULL);
		BOOL fMenu = (menu!=NULL && (WndConfigFlags & WndCFG_ShowMenu))?TRUE:FALSE;
		struct WndStyle_t style = WndStyle;

#if WINVER < 0x200
		if (WndConfigFlags & WndCFG_Fullscreen) {
			/* Windows 1.0: WS_OVERLAPPED (aka WS_TILED) prevents fullscreen because we then
			 *              cannot control the position of our window, change it to WS_POPUP */
			if ((style.style & (WS_POPUP|WS_CHILD)) == WS_TILED/*which is zero--look at WINDOWS.H*/)
				style.style |= WS_POPUP; /* WS_TILED is zero, so no need to mask off anything */
		}
#endif

		/* must be computed BEFORE creating the window */
		WinClientSizeToWindowSize(&WndMinSize,&WndMinSizeClient,&style,fMenu);
		WinClientSizeToWindowSize(&WndMaxSize,&WndMaxSizeClient,&style,fMenu);
		WinClientSizeToWindowSize(&WndDefSize,&WndDefSizeClient,&style,fMenu);
		WinClientSizeInFullScreen(&WndFullscreenSize,&style,fMenu);

		/* assume current size == def size */
		WndCurrentSizeClient = WndDefSizeClient;
		WndCurrentSize = WndDefSize;

		/* screen size */
		WndScreenSize.x = GetSystemMetrics(SM_CXSCREEN);
		WndScreenSize.y = GetSystemMetrics(SM_CYSCREEN);

#if WINVER >= 0x300
		hwndMain = CreateWindowEx(style.styleEx,WndProcClass,WndTitle,style.style,
			CW_USEDEFAULT,CW_USEDEFAULT,
			WndDefSize.x,WndDefSize.y,
			NULL,NULL,
			hInstance,NULL);
#else
		hwndMain = CreateWindow(WndProcClass,WndTitle,style.style,
			CW_USEDEFAULT,CW_USEDEFAULT,
			WndDefSize.x,WndDefSize.y, // NTS: In Windows 1.0, unless the style is WS_POPUP, this is ignored
			NULL,NULL,
			hInstance,NULL);
#endif
		if (!hwndMain) {
			MessageBox(NULL,"Unable to create window","Oops!",MB_OK);
			return 1;
		}

		if (fMenu) SetMenu(hwndMain,menu);
	}

#if WINVER >= 0x200
	if (nCmdShow == SW_MINIMIZE || nCmdShow == SW_SHOWMINIMIZED || nCmdShow == SW_SHOWMINNOACTIVE) WndStateFlags |= WndState_Minimized;
#else
	if (nCmdShow == SHOW_ICONWINDOW) WndStateFlags |= WndState_Minimized; /* Windows 1.0 */
#endif

	/* NTS: Windows 3.1 will not send WM_WINDOWPOSCHANGING for window creation because,
	 *      well, the window was just created and therefore didn't change position!
	 *      For fullscreen to work whether or not the window is maximized, position
	 *      change is necessary! To make this work with any other Windows, also maximize
	 *      the window. */
	if ((WndConfigFlags & WndCFG_Fullscreen) && !(WndStateFlags & WndState_Minimized)) {
#if WINVER >= 0x200
		SetWindowPos(hwndMain,HWND_TOP,0,0,0,0,SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE);
#else
		/* NTS: Windows 1.0 WS_TILED aka WS_OVERLAPPED windows cannot be moved using MoveWindow.
		 *      The above code for fullscreen should have changed the style to WS_POPUP so that
		 *      we can control and place the window */
		MoveWindow(hwndMain,
			WndFullscreenSize.left,WndFullscreenSize.top,
			WndFullscreenSize.right - WndFullscreenSize.left,
			WndFullscreenSize.bottom - WndFullscreenSize.top,TRUE);
#endif

#if WINVER >= 0x200
		if (nCmdShow == SW_SHOW || nCmdShow == SW_SHOWNA || nCmdShow == SW_SHOWNORMAL) nCmdShow = SW_SHOWMAXIMIZED;
#else
		/* FIXME: Why is Windows 1.04 giving us nCmdShow == 0x3300?? */
		/* NTS: We could change nCmdShow to SHOW_FULLSCREEN but that seems to also take focus away from the window.
		 *      It's really only useful if we keep the window as WS_OVERLAPPED aka WS_TILED to full the screen as a window.
		 *      The WS_POPUP hack father up does a better job for what we are trying to do here. */
#endif
	}

	SysMenu = GetSystemMenu(hwndMain,FALSE);
	if (WndConfigFlags & WndCFG_Fullscreen) {
		// do not remove SC_MOVE, it makes it impossible in Windows 3.x to move the minimized icon.
		// do not remove SC_RESTORE, it makes it impossible in Windows 3.x to restore the window by double-clicking the minimized icon.
		DeleteMenuGF(SysMenu,SC_MAXIMIZE,MF_BYCOMMAND);
		DeleteMenuGF(SysMenu,SC_SIZE,MF_BYCOMMAND);
	}
	if (WndConfigFlags & WndCFG_TopMost) {
#if WINVER >= 0x200
		SetWindowPos(hwndMain,HWND_TOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE);
#endif
	}

	ShowWindow(hwndMain,nCmdShow);
	UpdateWindow(hwndMain);

	if (GetActiveWindow() == hwndMain) WndStateFlags |= WndState_Active;

	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}

