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

HWND near			hwndMain;
const char near			WndProcClass[] = "GAME32JAM2025";
const char near			WndTitle[] = "Game";
HINSTANCE near			myInstance;

const POINT near		WndMinSizeClient = { 80, 60 };
const POINT near		WndMaxSizeClient = { 640, 480 };
const POINT near		WndDefSizeClient = { 480, 360 };
POINT near			WndMinSize = { 0, 0 };
POINT near			WndMaxSize = { 0, 0 };
POINT near			WndDefSize = { 0, 0 };

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
#ifdef WM_GETMINMAXINFO
	else if (message == WM_GETMINMAXINFO) {
		MINMAXINFO FAR *mmi = (MINMAXINFO FAR*)lparam;

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
			newBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);

			oldPen = SelectObject((HDC)wparam,newPen);
			oldBrush = SelectObject((HDC)wparam,newBrush);

			Rectangle((HDC)wparam,um.left,um.top,um.right+1,um.bottom+1);

			SelectObject((HDC)wparam,oldBrush);
			SelectObject((HDC)wparam,oldPen);
		}

		return 1; /* Important: Returning 1 signals to Windows that we processed the message. Windows 3.0 gets really screwed up if we don't! */
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
		const HMENU menu = LoadMenu(hInstance,MAKEINTRESOURCE(IDM_MAINMENU));
		const BOOL fMenu = menu!=NULL?TRUE:FALSE;
		const DWORD style = WS_OVERLAPPEDWINDOW;

		/* must be computed BEFORE creating the window */
		{
			RECT um;
			memset(&um,0,sizeof(um));
			um.right = WndMinSizeClient.x;
			um.bottom = WndMinSizeClient.y;
			AdjustWindowRect(&um,style,fMenu);
			WndMinSize.x = um.right - um.left;
			WndMinSize.y = um.bottom - um.top;
		}
		{
			RECT um;
			memset(&um,0,sizeof(um));
			um.right = WndMaxSizeClient.x;
			um.bottom = WndMaxSizeClient.y;
			AdjustWindowRect(&um,style,fMenu);
			WndMaxSize.x = um.right - um.left;
			WndMaxSize.y = um.bottom - um.top;
		}
		{
			RECT um;
			memset(&um,0,sizeof(um));
			um.right = WndDefSizeClient.x;
			um.bottom = WndDefSizeClient.y;
			AdjustWindowRect(&um,style,fMenu);
			WndDefSize.x = um.right - um.left;
			WndDefSize.y = um.bottom - um.top;
		}

		hwndMain = CreateWindow(WndProcClass,WndTitle,
			style,
			CW_USEDEFAULT,CW_USEDEFAULT,
			WndDefSize.x,WndDefSize.y,
			NULL,NULL,
			hInstance,NULL);
		if (!hwndMain) {
			MessageBox(NULL,"Unable to create window","Oops!",MB_OK);
			return 1;
		}

		SetMenu(hwndMain,menu);
	}

	ShowWindow(hwndMain,nCmdShow);
	UpdateWindow(hwndMain);

	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}

