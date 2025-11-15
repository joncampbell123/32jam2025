#ifndef TARGET_WINDOWS
# error This is Windows code, not DOS
#endif

#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>
#include <io.h>
#include "resource.h"

#include <zlib.h>

////
#if GAMEDEBUG
# define DLP_TICKS 0x0001u
# define DLOG(...) debuglogprintf(0,__VA_ARGS__)
# define DLOGT(...) debuglogprintf(DLP_TICKS,__VA_ARGS__)
#else
# define DLOG(...) { }
# define DLOGT(...) { }
#endif


#if WINVER >= 0x30A
# ifndef SPI_GETWORKAREA
#  define SPI_GETWORKAREA 0x0030
# endif
#endif

// WndConfigFlags
#define WndCFG_ShowMenu			0x00000001u /* show the menu bar */
#define WndCFG_Fullscreen		0x00000002u /* run fullscreen */
#define WndCFG_TopMost			0x00000004u /* run with "top most" status (overtop other applications) */
#define WndCFG_DeactivateMinimize	0x00000008u /* minimize if the user switches away from the application */
#define WndCFG_MultiInstance		0x00000010u /* allow multiple instances of this game */
#define WndCFG_FullscreenWorkArea	0x00000020u /* limit fullscreen to the Windows 95 "work area", do not cover the task bar */
#define WndCFG_DebugLogging		0x00000040u /* write debug output to a log file (if #ifdef GAMEDEBUG) */

// WndStateFlags
#define WndState_Minimized		0x00000001u
#define WndState_Maximized		0x00000002u
#define WndState_Active			0x00000004u
#define WndState_NeedBkRedraw		0x00000008u /* redraw the background */
#define WndState_NotIdle		0x00000010u /* if set, PeekMessage() and manage game, else GetMessage() and use idle timer */

// WndGraphicsCaps.flags
#define WndGraphicsCaps_Flags_DIBTopDown	0x00000001u /* Windows 95/NT4 top-down DIBs are supported */

// WindowsVersionFlags
#define WindowsVersionFlags_NT			0x00000001u /* Windows NT */

// Init GDI bmp
#define GDIWantTransparencyMask			0x0001u

struct WndStyle_t {
	DWORD			style;
	DWORD			styleEx;
};

struct WndGraphicsCaps_t {
	BYTE			flags;
};

struct WndScreenInfo_t {
	BYTE			BitsPerPixel;
	BYTE			BitPlanes;
	BYTE			TotalBitsPerPixel; // combined bitplanes * bpp
	BYTE			ColorBitsPerPixel; // bits per color [1]
	BYTE			RenderBitsPerPixel; // normally TotalBitsPerPixel but can be 1 if the code cannot figure out the DDB format
	WORD			PaletteSize;
	WORD			PaletteReserved;
	struct {
		WORD		x,y; // forms a ratio x/y describing the shape of a pixel [2]
	} AspectRatio;
	BYTE			Flags;
};
// [1] ColorBitsPerPixel This is the total bits per color on the device. For example, VGA/SVGA drivers in Windows 3.1 will
//     typically report 18 because VGA palette registers are 6 bits per R/G/B, 6+6+6 = 18 bits. Presumably if a SVGA driver
//     can run the DAC at 8 bits per R/G/B then this would report 8+8+8 24 bits.
//
//     Windows does not report this if RC_PALETTE is not set, if the driver is not considered to have a color palette.
//     That means unfortunately the VGA driver which does use a color palette, does not report a GDI programmable color palette.
//
//     Windows 3.1 S3 drivers in 256-color mode: 18
//
// [2] The VGA driver (640x480) and most SVGA drivers provide the same value in both (10x10) because
//     640x480, 800x600, and 1024x768 modes have square pixels
//
//     Windows 2.0 VGA driver: 36:36
//
//     Windows 3.0 CGA driver: 5:12 (makes sense, it's using 640x200 2-color mode)
//
//     Windows 3.1 VGA driver: 36:36 (640x480 16-color mode)
//
//     Windows 3.1 EGA driver: 38:48 (640x350 16-color mode)
//
//     The aspect x/y values from Windows seem to represent the relative sizes of a pixel.
//     Another way to refer to it, is a "pixel aspect ratio".
//
//     VGA 10:10             VGA 36:36
//
//       x=10                  x=36
//     +-----+               +-----+
//     |     | y=10          |     | y=36
//     |     |               |     |
//     +-----+               +-----+
//
//     EGA 38:48 in 640x350 mode the pixels are taller than wide as the monitor stretches it out to a 4:3 display
//
//       x=38
//     +-----+
//     |     |
//     |     | y=48
//     |     |
//     +-----+
//
//     CGA 5:12 in 640x200 mode the pixels are much taller than wide, again stretching out to a 4:3 display
//
//       x=5
//     +-----+
//     |     |
//     |     |
//     |     | y=12
//     |     |
//     |     |
//     +-----+
//
//
// COMMON FORMATS REPORTED:
//
// Windows 3.1 S3 SVGA driver:
//     BitsPerPixel=8
//     BitPlanes=1
//     TotalBitsPerPixel=8
//     ColorBitsPerPixel=18
//     PaletteSize=256
//     PaletteReserved=20
//     AspectRatio=10:10
//     Palettte | BitmapBig
//
// Windows 3.1 VGA driver (16-color):
//     BitsPerPixel=1
//     BitPlanes=4
//     TotalBitsPerPixel=4
//     ColorBitsPerPixel=24 (guessed)
//     PaletteSize=16 (guessed)
//     PaletteReserved=16 (guessed)
//     AspectRatio=36:36
//     BitmapBig
//
// Windows 3.1 VGA driver (monochrome):
//     BitsPerPixel=1
//     BitPlanes=1
//     TotalBitsPerPixel=1
//     ColorBitsPerPixel=24 (guessed)
//     PaletteSize=2 (guessed)
//     PaletteReserved=2 (guessed)
//     AspectRatio=36:36
//     BitmapBig
//
// Windows 2.03 VGA driver (16-color):
// * Notice that despite using 16-color VGA mode, Windows 2.x uses only 8 colors (hence, BitPlanes=3)!
// * The colors are only the high intensity versions of RGB: [Black, Red, Green, Yellow, Blue, Purple, Cyan, White]
// * Obviously doing this makes RGB to VGA mapping easier without having to worry about the intensity bit.
//     BitsPerPixel=1
//     BitPlanes=3
//     TotalBitsPerPixel=3
//     ColorBitsPerPixel=24 (guessed)
//     PaletteSize=8 (guessed)
//     PaletteReserved=8 (guessed)
//     AspectRatio=36:36
//     BitmapBig
//
// Windows 1.04 EGA driver (16-color):
// * Notice that despite using 16-color EGA mode, Windows 1.x uses only 8 colors (hence, BitPlanes=3)!
// * The colors are only the high intensity versions of RGB: [Black, Red, Green, Yellow, Blue, Purple, Cyan, White]
// * Obviously doing this makes RGB to EGA mapping easier without having to worry about the intensity bit.
//     BitsPerPixel=1
//     BitPlanes=3
//     TotalBitsPerPixel=3
//     ColorBitsPerPixel=24 (guessed)
//     PaletteSize=8 (guessed)
//     PaletteReserved=8 (guessed)
//     AspectRatio=38:48
//     BitmapBig
//
// Windows 2.03 Tandy 1000 driver (4-color 640x200):
// * Why the 4-color mode? Was Microsoft unable to handle the extra 16KB of video RAM needed for the full 16-colors?
// * The 4 colors chosen are [Blue, Red, Green, White]. It's hardly better than plain 4-color CGA, why bother?
//     BitsPerPixel=1
//     BitPlanes=2
//     TotalBitsPerPixel=2
//     ColorBitsPerPixel=24 (guessed)
//     PaletteSize=4 (guessed)
//     PaletteReserved=4 (guessed)
//     AspectRatio=5:12
//     BitmapBig
//

#define WndScreenInfoFlag_Palette	0x00000001u
#define WndScreenInfoFlag_BitmapBig	0x00000002u /* supports >= 64KB bitmaps */

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
const struct WndStyle_t		WndStyle = { .style = WS_OVERLAPPEDWINDOW, .styleEx = 0 };
//const struct WndStyle_t		WndStyle = { .style = WS_POPUPWINDOW|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_CAPTION, .styleEx = 0 };
//const struct WndStyle_t		WndStyle = { .style = WS_DLGFRAME|WS_CAPTION|WS_SYSMENU|WS_BORDER, .styleEx = WS_EX_DLGMODALFRAME };

#if GAMEDEBUG
int near			debug_log_fd = -1;
DWORD				debug_p_ticks = 0;
uint64_t			debug_ticks = 0;
#endif

HINSTANCE near			myInstance = (HINSTANCE)NULL;
HWND near			hwndMain = (HWND)NULL;
HMENU near			SysMenu = (HMENU)NULL;

// Window config (WndCFG_...) bitfield
BYTE near			WndConfigFlags = WndCFG_ShowMenu;
//BYTE near			WndConfigFlags = WndCFG_ShowMenu | WndCFG_MultiInstance;
//BYTE near			WndConfigFlags = WndCFG_ShowMenu | WndCFG_TopMost;
//BYTE near			WndConfigFlags = WndCFG_ShowMenu | WndCFG_Fullscreen;
//BYTE near			WndConfigFlags = WndCFG_ShowMenu | WndCFG_Fullscreen | WndCFG_TopMost;
//BYTE near			WndConfigFlags = WndCFG_ShowMenu | WndCFG_Fullscreen | WndCFG_FullscreenWorkArea;
//BYTE near			WndConfigFlags = 0;
//BYTE near			WndConfigFlags = WndCFG_ShowMenu | WndCFG_DeactivateMinimize;
//BYTE near			WndConfigFlags = WndCFG_Fullscreen;

// NOTE: To prevent resizing completely, set the min, max, and def sizes to the exact same value
const POINT near		WndMinSizeClient = { 80, 60 };
POINT near			WndMinSize = { 0, 0 };
const POINT near		WndMaxSizeClient = { 600, 380 };
POINT near			WndMaxSize = { 0, 0 };
const POINT near		WndDefSizeClient = { 320, 240 };
POINT near			WndDefSize = { 0, 0 };

RECT near			WndFullscreenSize = { 0, 0, 0, 0 };
POINT near			WndCurrentSizeClient = { 0, 0 };
POINT near			WndCurrentSize = { 0, 0 };
POINT near			WndScreenSize = { 0, 0 };
RECT near			WndWorkArea = { 0, 0, 0, 0 };

#if TARGET_MSDOS == 32 && !defined(WIN386)
HANDLE				WndLocalAppMutex = NULL;
#endif

// Window state (WndState_...) bitfield
BYTE near			WndStateFlags = 0;
WORD near			DOSVersion = 0; /* 0 if unable to determine */
WORD near			WindowsVersion = 0;
BYTE near			WindowsVersionFlags = 0;

struct WndScreenInfo_t near	WndScreenInfo = { 0, 0, 0, 0, 0, 0 };
struct WndGraphicsCaps_t near	WndGraphicsCaps = { 0 };

// background brush
HBRUSH				WndBkBrush = (HBRUSH)NULL;
COLORREF			WndBkColor = RGB(0,0,0);

#if WINVER < 0x400
/* Windows 3.x compensation for off by 1 (or 2) bugs in AdjustWindowRect */
int near			WndAdjustWindowRectBug_yadd = 0;
#endif

// 256-color palette
LOGPALETTE*			WndLogPalette = NULL;
HPALETTE near			WndHanPalette = (HPALETTE)NULL;

// loaded bitmap resource
struct BMPres {
	HBITMAP					bmpObj;
	unsigned short				width,height;
	unsigned short				flags;
};

#define BMPresFlag_Allocated			0x0001u /* slot is allocated, whether or not there is a bitmap object */
#define BMPresFlag_TransparencyMask		0x0002u /* bitmap has transparency mask */

typedef WORD					BMPresHandle;
WORD near					BMPresMax = 4;
struct BMPres*					BMPres = NULL;
static const struct BMPres near			BMPresInit = { .bmpObj = (HBITMAP)NULL, .width = 0, .height = 0, .flags = 0 };
#define BMPresHandleNone			((WORD)(-1))

struct BMPres *GetBMPres(const BMPresHandle h) {
	if (BMPres && h < BMPresMax) return BMPres + h;
	return NULL;
}

static inline struct BMPres *GetBMPresNRC(const BMPresHandle h) {
	return BMPres + h;
}

// sprite within bitmap
struct SpriteRes {
	BMPresHandle				bmp;
	unsigned short				x,y,w,h; /* subregion of bmp */
	WORD					flags;
};

#define SpriteResFlag_Allocated			0x0001u /* slot is allocated */

typedef WORD					SpriteResHandle;
WORD near					SpriteResMax = 64;
struct SpriteRes*				SpriteRes = NULL;
static const struct SpriteRes near		SpriteResInit = { .bmp = BMPresHandleNone, .x = 0, .y = 0, .w = 0, .h = 0 };
#define SpriteResHandleNone			((WORD)(-1))

static inline struct SpriteRes *GetSpriteResNRC(const SpriteResHandle h) {
	return SpriteRes + h;
}

struct SpriteRes *GetSpriteRes(const SpriteResHandle h) {
	if (SpriteRes && h < SpriteResMax) return SpriteRes + h;
	return NULL;
}

// combo bitmap/sprite reference number. An imageref is a reference to a sprite or bitmap
typedef WORD					ImageRef;
#define ImageRefNone				((WORD)(-1))
#define ImageRefTypeShift			((WORD)(14))
#define ImageRefRefMask				((WORD)((1u << ImageRefTypeShift) - 1u))
#define ImageRefTypeMask			((WORD)(~((1u << ImageRefTypeShift) - 1u)))
/* ^ NTS: shift=14, 1 << 14 = 0x4000, (1 << 14) - 1 = 0x3FFF, ~0x3FFF = 0xC000 */
#define ImageRefTypeBitmap			((WORD)(0))
#define ImageRefTypeSprite			((WORD)(1))
#define ImageRefGetRef(x)			(((WORD)(x)) & ImageRefRefMask)
#define ImageRefGetType(x)			((((WORD)(x)) & ImageRefTypeMask) >> ((WORD)ImageRefTypeShift))
#define ImageRefMakeType(x)			((((WORD)(x)) << ImageRefTypeShift) & ImageRefTypeMask)
#define MAKEBMPIMAGEREF(x)			(((WORD)(x)) | ImageRefMakeType(ImageRefTypeBitmap))
#define MAKESPRITEIMAGEREF(x)			(((WORD)(x)) | ImageRefMakeType(ImageRefTypeSprite))

/////////////////////////////////////////////////////////////

// arrangement of bitmaps or sprites on the window.
// NTS: The transparency mask is IGNORED by the window element drawing code.
// NTS: This code copies down the subregion of a sprite. Changing the sprite source rect after setting it to
//      the window element will not change the subregion. Changing the bitmap assigned to the sprite will not
//      change the bitmap displayed.
// NTS: Windows 3.1/95 may have 2D acceleration but that is not guaranteed, performance may not be very fast.
//      If integer scaling is involved here, performance will definitely drop unless the driver provides some
//      hardware aceeleration for that too, which for Windows 3.1 is very unlikely.
struct WindowElement {
	ImageRef				imgRef;
	int					x,y; /* signed, to allow partially offscreen if that's what you want */
	unsigned int				w,h;
	unsigned int				sx,sy; /* source x,y coordinates of bitmap */
	BYTE					flags;
	BYTE					scale; /* integer scale - 1 */
};

#define WindowElementFlag_Allocated		0x0001u /* window element is allocated */
#define WindowElementFlag_Enabled		0x0002u /* window element is enabled */
#define WindowElementFlag_Update		0x0004u /* window element needs to be redrawn fully */
#define WindowElementFlag_BkUpdate		0x0008u /* window element will need to also trigger window background redraw */
#define WindowElementFlag_Overlapped		0x0010u /* window element is overlapped by another */
#define WindowElementFlag_NoAutoSize		0x0020u /* do not auto resize window element on content change */
#define WindowElementFlag_OwnsImage		0x0040u /* this window element owns the imageRef (must be bitmap), and will free it automatically */

typedef WORD					WindowElementHandle;
WORD near					WindowElementMax = 8;
struct WindowElement*				WindowElement = NULL;
static const struct WindowElement near		WindowElementInit = { .imgRef = ImageRefNone, .x = 0, .y = 0, .w = 0, .h = 0, .flags = 0, .scale = 0 };
#define WindowElementHandleNone			((WORD)(-1))

static inline struct WindowElement *GetWindowElementNRC(const WindowElementHandle h) {
	return WindowElement + h;
}

struct WindowElement *GetWindowElement(const WindowElementHandle h) {
	if (WindowElement && h < WindowElementMax) return WindowElement + h;
	return NULL;
}

/////////////////////////////////////////////////////////////

struct FontResource {
	HFONT					fontObj;
	short int				height,ascent,descent,avwidth,maxwidth;
};

typedef WORD					FontResourceHandle;
WORD near					FontResourceMax = 4;
struct FontResource*				FontResource = NULL;
static const struct FontResource near		FontResourceInit = { .fontObj = (HFONT)NULL, .height = 0, .ascent = 0, .descent = 0, .avwidth = 0, .maxwidth = 0 };
#define FontResourceHandleNone			((WORD)(-1))

static inline struct FontResource *GetFontResourceNRC(const FontResourceHandle h) {
	return FontResource + h;
}

struct FontResource *GetFontResource(const FontResourceHandle h) {
	if (FontResource && h < FontResourceMax) return FontResource + h;
	return NULL;
}

#define FontResourceFlagBold			0x0001u
#define FontResourceFlagItalic			0x0002u

/////////////////////////////////////////////////////////////

int clamp0(int x) {
	return x >= 0 ? x : 0;
}

uint32_t bytswp32(uint32_t t) {
	t = (t << 16) | (t >> 16);
	t = ((t & 0xFF00FF00) >> 8) | ((t & 0x00FF00FF) << 8);
	return t;
}

/////////////////////////////////////////////////////////////

#if GAMEDEBUG
# ifndef WIN32
static char dlogtmp[256];
# endif
// Win16 warning: Do not call this function from any code that Windows 3.1 calls from an "interrupt handler"
void debuglogprintf(unsigned int fl,const char *fmt,...) {
	if (WndConfigFlags & WndCFG_DebugLogging) {
# ifdef WIN32
		char dlogtmp[256];
# endif
		char *w = dlogtmp,*f = dlogtmp + sizeof(dlogtmp) - 3;
		va_list va;
		size_t l;

		va_start(va,fmt);

		if (fl & DLP_TICKS) {
			/* track the ticks not directly, but by counting deltas, so that the 49 day rollover does not affect our time counting */
			/* NTS: By default in Windows, tick count resolution is roughly 55ms (18.2 ticks/sec system timer) */
			const DWORD nc = GetTickCount();
			debug_ticks += (uint64_t)((DWORD)(nc - debug_p_ticks));
			debug_p_ticks = nc;
			w += snprintf(w,(size_t)(f - w),"@%lu.%03u: ",(unsigned long)(debug_ticks / 1000ull),(unsigned int)(debug_ticks % 1000ull));
		}

		if (w < f) w += vsnprintf(w,(size_t)(f - w),fmt,va);
		*w++ = '\r';
		*w++ = '\n';
		*w = 0;

		l = (size_t)(w - dlogtmp);

		write(debug_log_fd,dlogtmp,l);
		fsync(debug_log_fd); // in case the program crashes!

		va_end(va);
	}
}
#endif

/////////////////////////////////////////////////////////////

BOOL CheckMultiInstanceFindWindow(const BOOL mustError) {
	if (!(WndConfigFlags & WndCFG_MultiInstance)) {
		HWND hwnd = FindWindow(WndProcClass,NULL);
		if (hwnd) {
			DLOGT("found another instance via FindWindow");

			/* NTS: Windows 95 and later might ignore SetActiveWindow(), use SetWindowPos() as a backup
			 *      to at least make it visible. */
			SetWindowPos(hwnd,0,0,0,0,0,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW|SWP_NOMOVE|SWP_NOSIZE);
			SetActiveWindow(hwnd);

			if (IsIconic(hwnd)) {
				DLOGT("The other window is minimized, restoring it");
				SendMessage(hwnd,WM_SYSCOMMAND,SC_RESTORE,0);
			}

			return TRUE;
		}
		else if (mustError) {
			MessageBox(NULL,"This game is already running","Already running",MB_OK);
			return TRUE;
		}

	}

	return FALSE;
}

/////////////////////////////////////////////////////////////

void WinClientSizeInFullScreen(RECT *d,const struct WndStyle_t *style,const BOOL fMenu) {
	(void)fMenu;

	if (WndConfigFlags & WndCFG_FullscreenWorkArea) {
		*d = WndWorkArea;
	}
	else {
		d->left = 0;
		d->top = 0;
		d->right = GetSystemMetrics(SM_CXSCREEN);
		d->bottom = GetSystemMetrics(SM_CYSCREEN);
	}

	/* calculate the rect so that the window caption, sysmenu, and borders are just off the
	 * edges of the screen, leaving the client area to cover the screen, BUT, disregard the
	 * fMenu flag so that the menu bar, if any, is visible at the top of the screen. */
	AdjustWindowRectEx(d,style->style,FALSE,style->styleEx);
}

void WinClientSizeToWindowSize(POINT *d,const POINT *s,const struct WndStyle_t *style,const BOOL fMenu) {
	RECT um;
	memset(&um,0,sizeof(um));
	um.right = s->x;
	um.bottom = s->y;
	AdjustWindowRectEx(&um,style->style,fMenu,style->styleEx);
	d->x = um.right - um.left;
	d->y = um.bottom - um.top;
#if WINVER < 0x400
/* Windows 3.x compensation for off by 1 (or 2) bugs in AdjustWindowRect but only if fMenu == TRUE */
	if (fMenu) d->y += WndAdjustWindowRectBug_yadd;
#endif
}

/////////////////////////////////////////////////////////////

BOOL InitLogPalette(void) {
	if (!WndLogPalette) {
		if ((WndScreenInfo.Flags & WndScreenInfoFlag_Palette) && WndScreenInfo.PaletteSize > 0 && WndScreenInfo.PaletteSize <= 256) {
			/* NTS: LOGPALETTE is defined as the header followed by exactly one PALETTENTRY,
			 *      therefore subtract 1 when multiplying colors by PALETTENTRY. */
			const size_t sz = sizeof(LOGPALETTE) + ((WndScreenInfo.PaletteSize - 1) * sizeof(PALETTEENTRY));
			DLOGT("Display is %u-color paletted, initializing logical palette (%u bytes)",WndScreenInfo.PaletteSize,(unsigned int)sz);
			WndLogPalette = (LOGPALETTE*)malloc(sz);
			if (!WndLogPalette) {
				DLOGT("ERROR: Cannot allocate logpalette");
				return FALSE;
			}
			memset(WndLogPalette,0,sz); // default all-black palette
			WndLogPalette->palVersion = 0x300;
			WndLogPalette->palNumEntries = WndScreenInfo.PaletteSize;
		}
	}

	return TRUE;
}

UINT UnrealizePaletteObject(void) {
	UINT chg = 0;

	if (WndHanPalette) {
		HDC hdc;

		DLOGT("Selecting the default system palette into the window to flush our palette");

		hdc = GetDC(hwndMain);
		if (hdc) {
			if (SelectPalette(hdc,(HPALETTE)GetStockObject(DEFAULT_PALETTE),(WndStateFlags & WndState_Minimized) ? TRUE : FALSE) == (HPALETTE)NULL)
				DLOGT("ERROR: Cannot select system palette into main window");

			chg = RealizePalette(hdc);
			DLOGT("RealizePalette: Changed %u colors in the sytem palette",chg);

			ReleaseDC(hwndMain,hdc);
		}
		else {
			DLOGT("ERROR: Cannot GetDC the main window");
		}
	}

	return chg;
}

UINT RealizePaletteObject(void) {
	UINT chg = 0;

	if (WndHanPalette) {
		HDC hdc;

		DLOGT("Selecting the palette into the window");

		hdc = GetDC(hwndMain);
		if (hdc) {
			if (SelectPalette(hdc,WndHanPalette,(WndStateFlags & WndState_Minimized) ? TRUE : FALSE) == (HPALETTE)NULL)
				DLOGT("ERROR: Cannot select palette into main window");

			chg = RealizePalette(hdc);
			DLOGT("RealizePalette: Changed %u colors in the sytem palette",chg);

			ReleaseDC(hwndMain,hdc);
		}
		else {
			DLOGT("ERROR: Cannot GetDC the main window");
		}
	}

	return chg;
}

BOOL InitPaletteObject(void) {
	if (WndLogPalette && WndHanPalette == (HPALETTE)NULL && (WndScreenInfo.Flags & WndScreenInfoFlag_Palette)) {
		DLOGT("Logpalette valid, creating GDI palette object");
		WndHanPalette = CreatePalette(WndLogPalette);
		if (WndHanPalette == NULL) {
			DLOGT("ERROR: Unable to create GDI palette object");
			return FALSE;
		}

		RealizePaletteObject();
	}

	return TRUE;
}

BOOL InitColorPalette(void) {
	if (!InitLogPalette())
		return FALSE;

	/* NTS: Do not init the palette object until the call is made to load a logical palette */
	return TRUE;
}

void FreeLogPalette(void) {
	if (WndLogPalette) {
		DLOGT("Freeing logical palette");
		UnrealizePaletteObject();
		free(WndLogPalette);
		WndLogPalette = NULL;
	}
}

void FreePaletteObject(void) {
	if (WndHanPalette) {
		DLOGT("Freeing GDI palette object");
		DeleteObject(WndHanPalette);
		WndHanPalette = (HPALETTE)NULL;
	}
}

void FreeColorPalette(void) {
	FreeLogPalette();
	FreePaletteObject();
}

/////////////////////////////////////////////////////////////

BOOL InitBackgroundBrush(void) {
	if (!WndBkBrush) {
		WndBkBrush = CreateSolidBrush(WndBkColor);
		if (!WndBkBrush) return FALSE;
	}

	return TRUE;
}

void FreeBackgroundBrush(void) {
	if (WndBkBrush) {
		DeleteObject(WndBkBrush);
		WndBkBrush = (HBRUSH)NULL;
	}
}

void SetBackgroundColor(COLORREF x) {
	if (WndBkColor != x) {
		FreeBackgroundBrush();
		WndBkColor = x;
		InitBackgroundBrush();
		InvalidateRect(hwndMain,NULL,TRUE);
	}
}

/////////////////////////////////////////////////////////////

void LoadLogPaletteBMP(const int fd,PALETTEENTRY *pels,UINT colors) {
	BITMAPFILEHEADER bfh;
	BITMAPINFOHEADER bih;
	unsigned int i;
	DWORD bclr;

	lseek(fd,0,SEEK_SET);

	memset(&bfh,0,sizeof(bfh));
	read(fd,&bfh,sizeof(bfh));
	/* assume bfh.bfType == 'BM' because the calling function already checked that */
	read(fd,&bih,sizeof(bih));

	/* require at least biSize >= 40. No old OS/2 format, please. */
	if (bih.biSize < 40 || bih.biSize > 2048) {
		DLOGT("BMP file biSize is wrong size, must be BITMAPINFOHEADER");
		return;
	}

	/* biCompression == 0, uncompressed only.
	 * Don't even bother with Windows 95/NT biCompression == BI_BITFIELDS */
	if (bih.biCompression != 0) {
		DLOGT("BMP file compression not supported, must be traditional uncompressed format");
		return;
	}

	/* all that matters is whether the bit count is below 8 and the BMP has a color palette */
	if (!(bih.biBitCount >= 1 && bih.biBitCount <= 8 && bih.biPlanes == 1)) {
		DLOGT("BMP file is not paletted (%u bits per pixel)",bih.biBitCount);
		return;
	}

	bclr = 1u << bih.biBitCount;
	if (bih.biClrUsed != 0 && bclr > bih.biClrUsed) bclr = bih.biClrUsed;
	if (bclr > colors) bclr = colors;

	/* if biSize > 40 (as the GNU Image Manipulation Program likes to do by default especially if encoding
	 * colorspace info) then skip forward to the palette */
	if (bih.biSize > 40) {
		DLOGT("BITMAPINFOHEADER biSize larger than base format, skipping %u bytes to the color palette",(unsigned int)bih.biSize - 40);
		lseek(fd,bih.biSize - 40,SEEK_CUR);
	}

	DLOGT("Will load %u colors from BMP palette",bclr);
	if (bclr && sizeof(RGBQUAD) == sizeof(PALETTEENTRY)/*4 bytes per entry in both or else this code is wrong*/) {
		DLOGT("Loading %u colors from BMP palette",bclr);

		/* palette follows the BITMAPINFOHEADER */
		read(fd,pels,bclr * sizeof(RGBQUAD));

		/* convert in place */
		for (i=0;i < bclr;i++) {
			/* RGBQUAD
			 *   rgbBlue
			 *   rgbGreen
			 *   rgbBlue
			 *   rgbReserved
			 *
			 * PALETTEENTRY
			 *   peRed
			 *   peGreen
			 *   peBlue
			 *   peFlags
			 */
			const RGBQUAD tmp = ((RGBQUAD*)pels)[i];
			pels[i].peRed = tmp.rgbRed;
			pels[i].peGreen = tmp.rgbGreen;
			pels[i].peBlue = tmp.rgbBlue;
			pels[i].peFlags = 0;
		}
	}
}

/////////////////////////////////////////////////////////////

/* PNG IHDR [https://www.w3.org/TR/PNG/#11IHDR]. Fields are big endian. */
#pragma pack(push,1)
struct minipng_IHDR {
    uint32_t        width;                  /* +0x00 */
    uint32_t        height;                 /* +0x04 */
    uint8_t         bit_depth;              /* +0x08 */
    uint8_t         color_type;             /* +0x09 */
    uint8_t         compression_method;     /* +0x0A */
    uint8_t         filter_method;          /* +0x0B */
    uint8_t         interlace_method;       /* +0x0C */
};                                          /* =0x0D */
#pragma pack(pop)

/* PNG PLTE [https://www.w3.org/TR/PNG/#11PLTE] */
#pragma pack(push,1)
struct minipng_PLTE_color {
    uint8_t         red,green,blue;         /* +0x00,+0x01,+0x02 */
};                                          /* =0x03 */
#pragma pack(pop)

/////////////////////////////////////////////////////////////

void LoadLogPalettePNG(const int fd,PALETTEENTRY *pels,UINT colors) {
	unsigned char tmp[9];
	DWORD length,chktype;
	off_t ofs = 8;

#if GAMEDEBUG
	if (bytswp32(0x11223344) != 0x44332211) {
		DLOGT("bytswp32 failed to byte swap properly");
		return;
	}
#endif

	/* assume the PNG signature is already there and that the calling code verified it already */
	/* PNG chunk struct
	 *   DWORD length
	 *   DWORD chunkType
	 *   BYTE data[]
	 *   DWORD crc32 (we ignore it)
	 *
	 * 32-bit values are big endian */

	while (1) {
		if (lseek(fd,ofs,SEEK_SET) != ofs) break;

		if (read(fd,tmp,8) != 8) break;
		tmp[8] = 0;
		ofs += 8;

		length = bytswp32(*((DWORD*)(tmp+0)));
		chktype = bytswp32(*((DWORD*)(tmp+4)));

		DLOGT("PNG chunk at %lu: length=%lu chktype=0x%lx '%s'",
			(unsigned long)ofs,(unsigned long)length,(unsigned long)chktype,tmp+4);

		if (chktype == 0x504C5445/*PLTE*/ && length >= 3) {
			unsigned int pclr = length / 3;
			if (pclr > colors) pclr = colors;

			DLOGT("PNG PLTE loading %u colors",pclr);
			if (sizeof(struct minipng_PLTE_color) == 3 && sizeof(PALETTEENTRY) > 3 && pclr != 0) {
				unsigned int i = pclr;

				read(fd,pels,3*pclr);

				/* convert in place backwards */
				while ((i--) > 0) {
					const struct minipng_PLTE_color pc = ((struct minipng_PLTE_color*)pels)[i];
					pels[i].peRed = pc.red;
					pels[i].peGreen = pc.green;
					pels[i].peBlue = pc.blue;
					pels[i].peFlags = 0;
				}
			}
		}

		ofs += length; /* skip data[] */

		ofs += 4; /* skip CRC32 */

		/* stop at IEND */
		if (chktype == 0x49454E44/*IEND*/) {
			DLOGT("PNG IEND found, stopping");
			break;
		}
	}
}

/////////////////////////////////////////////////////////////

void LoadLogPalette(const char *p) {
	if (WndLogPalette) {
		int fd = open(p,O_RDONLY|O_BINARY);
		if (fd >= 0) {
			PALETTEENTRY *pels = (PALETTEENTRY*)(&WndLogPalette->palPalEntry[0]);
			char tmp[8] = {0};

			DLOGT("Loading logical palette from source image %s",p);
			memset(pels,0,WndScreenInfo.PaletteSize * sizeof(PALETTEENTRY));

			lseek(fd,0,SEEK_SET);
			read(fd,tmp,8);

			if (!memcmp(tmp,"BM",2)) {
				DLOGT("BMP image in %s",p);
				LoadLogPaletteBMP(fd,pels,WndScreenInfo.PaletteSize);
			}
			else if (!memcmp(tmp,"\x89PNG\x0D\x0A\x1A\x0A",8)) {
				DLOGT("PNG image in %s",p);
				LoadLogPalettePNG(fd,pels,WndScreenInfo.PaletteSize);
			}
			else {
				DLOGT("Unable to identify image type in %s",p);
			}

			if (!WndHanPalette)
				InitPaletteObject();

			if (WndHanPalette) {
				UINT chg;

				chg = SetPaletteEntries(WndHanPalette,0,WndScreenInfo.PaletteSize,pels);
				DLOGT("Applying palette to GDI object, %u colors changed",chg);

				RealizePaletteObject();
			}

			close(fd);
		}
		else {
			DLOGT("Unable to open palette source image %s",p);
		}
	}
	else {
		DLOGT("Ignoring palette load from %s, video mode does not use a palette",p);
	}
}

/////////////////////////////////////////////////////////////

BOOL InitFontResources(void) {
	unsigned int i;

	if (!FontResource && FontResourceMax != 0) {
		DLOGT("Allocating font res array, %u max",FontResourceMax);
		FontResource = malloc(FontResourceMax * sizeof(struct FontResource));
		if (!FontResource) {
			DLOGT("Failed to allocate array");
			return FALSE;
		}
		for (i=0;i < FontResourceMax;i++) FontResource[i] = FontResourceInit;
	}

	return TRUE;
}

void FreeFontResource(const FontResourceHandle f) {
	struct FontResource *fr = GetFontResource(f);

	if (fr && (fr->fontObj || fr->height == -1/*a way to mark a font allocated without a fontObj*/)) {
		DLOGT("Freeing font resource #%u",f);
		if (fr->fontObj) {
			DeleteObject(fr->fontObj);
			fr->fontObj = (HFONT)NULL;
		}
		fr->height = 0;
	}
}

void FreeFontResources(void) {
	unsigned int i;

	if (FontResource) {
		DLOGT("Freeing font res");
		for (i=0;i < FontResourceMax;i++) FreeFontResource(i);
		free(FontResource);
		FontResource = NULL;
	}
}

/////////////////////////////////////////////////////////////

// Windows height rules:
//   height > 0 describes cell height
//   height < 0 describes character height
//   height == 0 picks a default
//   width == 0 to pick a default width
BOOL LoadFontResource(const FontResourceHandle fh,int height,int width,unsigned int flags,const char *fontName) {
	struct FontResource *fr = GetFontResource(fh);

	if (fr) {
		FreeFontResource(fh);

		DLOGT("Loading font resource into #%u: height=%u width=%u font=\"%s\"",fh,height,width,fontName);
		if (flags & FontResourceFlagBold) DLOGT("Flag: BOLD");
		if (flags & FontResourceFlagItalic) DLOGT("Flag: ITALIC");

		fr->fontObj = CreateFont(height,width,/*escapement*/0,/*orientation*/0,
			/*weight*/(flags & FontResourceFlagBold) ? FW_BOLD : FW_NORMAL,
			/*italic*/(flags & FontResourceFlagItalic) ? TRUE : FALSE,
			/*underline*/FALSE,
			/*strikeout*/FALSE,
			ANSI_CHARSET/*TODO: Flag value to specify SHIFTJIS_CHARSET someday?*/,
			OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY,
			DEFAULT_PITCH,
			fontName);

		if (!fr->fontObj) {
			DLOGT("CreateFont failed");
			return FALSE;
		}

		{
			HDC hDC = GetDC(hwndMain);
			HFONT fhold = (HFONT)SelectObject(hDC,fr->fontObj);
			TEXTMETRIC tm = {0};
			GetTextMetrics(hDC,&tm);
			fr->height = tm.tmHeight;
			fr->ascent = tm.tmAscent;
			fr->descent = tm.tmDescent;
			fr->avwidth = tm.tmAveCharWidth;
			fr->maxwidth = tm.tmMaxCharWidth;
			DLOGT("Created font metrics: height=%u ascent=%u descent=%u avgwidth=%u maxwidth=%u",
				fr->height,fr->ascent,fr->descent,fr->avwidth,fr->maxwidth);
			SelectObject(hDC,fhold);
			ReleaseDC(hwndMain,hDC);
		}
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////

void FreeBMPres(const BMPresHandle h);

BOOL InitBMPResources(void) {
	unsigned int i;

	if (!BMPres && BMPresMax != 0) {
		DLOGT("Allocating BMP res array, %u max",BMPresMax);
		BMPres = malloc(BMPresMax * sizeof(struct BMPres));
		if (!BMPres) {
			DLOGT("Failed to allocate array");
			return FALSE;
		}
		for (i=0;i < BMPresMax;i++) BMPres[i] = BMPresInit;
	}

	return TRUE;
}

void FreeBMPResources(void) {
	unsigned int i;

	if (BMPres) {
		DLOGT("Freeing BMP res");
		for (i=0;i < BMPresMax;i++) FreeBMPres(i);
		free(BMPres);
		BMPres = NULL;
	}
}

void FreeBMPresGDIObject(const BMPresHandle h) {
	struct BMPres *b = GetBMPres(h);

	if (b && b->bmpObj != (HBITMAP)NULL) {
		DLOGT("Freeing BMP #%u res GDI object",h);
		DeleteObject((HGDIOBJ)(b->bmpObj));
		b->bmpObj = (HBITMAP)NULL;
	}
}

void FreeBMPres(const BMPresHandle h) {
	struct BMPres *b = GetBMPres(h);

	if (b) {
		FreeBMPresGDIObject(h);
		if (b->flags & BMPresFlag_Allocated) {
			DLOGT("Freeing BMP #%u res",h);
			b->width = b->height = b->flags = 0;
		}
	}
}

/////////////////////////////////////////////////////////////

BOOL InitSpriteResources(void) {
	unsigned int i;

	if (!SpriteRes && SpriteResMax != 0) {
		DLOGT("Allocating sprite res array, %u max",SpriteResMax);
		SpriteRes = malloc(SpriteResMax * sizeof(struct SpriteRes));
		if (!SpriteRes) {
			DLOGT("Failed to allocate array");
			return FALSE;
		}
		for (i=0;i < SpriteResMax;i++) SpriteRes[i] = SpriteResInit;
	}

	return TRUE;
}

BOOL InitWindowElements(void) {
	unsigned int i;

	if (!WindowElement && WindowElementMax != 0) {
		DLOGT("Allocating window element array, %u max",WindowElementMax);
		WindowElement = malloc(WindowElementMax * sizeof(struct WindowElement));
		if (!WindowElement) {
			DLOGT("Failed to allocate array");
			return FALSE;
		}
		for (i=0;i < WindowElementMax;i++) WindowElement[i] = WindowElementInit;
	}

	return TRUE;
}

BOOL IsBMPresAlloc(const BMPresHandle h) {
	const struct BMPres *b = GetBMPres(h);
	if (b && b->flags & BMPresFlag_Allocated) return TRUE;
	return FALSE;
}

BOOL IsSpriteResAlloc(const SpriteResHandle h) {
	const struct SpriteRes *r = GetSpriteRes(h);
	if (r && r->bmp != BMPresHandleNone) return TRUE;
	return FALSE;
}

/////////////////////////////////////////////////////////////

FontResourceHandle AllocFont(void) {
	if (FontResource) {
		unsigned int i;

		for (i=0;i < FontResourceMax;i++) {
			struct FontResource *we = GetFontResourceNRC(i);
			if (we->fontObj == (HFONT)NULL && we->height != -1) {
				we->height = -1; // a way to mark allocated without creating an HFONT
				DLOGT("Font #%u allocated",i);
				return (FontResourceHandle)i;
			}
		}
	}

	DLOGT("Unable to allocate font");
	return FontResourceHandleNone;
}

/////////////////////////////////////////////////////////////

SpriteResHandle AllocSpriteRes(const unsigned int count) {
	if (SpriteRes && count) {
		unsigned int i,ib=0,c=0;

		for (i=0;i < SpriteResMax;i++) {
			struct SpriteRes *we = GetSpriteResNRC(i);

			if (we->flags & SpriteResFlag_Allocated) {
				ib = i + 1; /* then maybe the next slot is open and could be the base of it */
				c = 0;
			}
			else {
				if ((++c) >= count) {
					for (c=0;c < count;c++) {
						struct SpriteRes *we = GetSpriteResNRC(ib+c);
#if GAMEDEBUG
						if ((ib+c) >= SpriteResMax) {
							DLOGT("BUG! Out of range sprite index line %d",__LINE__);
							break;
						}
#endif
						we->flags = SpriteResFlag_Allocated;
						we->bmp = BMPresHandleNone;
					}
					DLOGT("Allocated %u sprites starting at #%u",count,ib);
					return (SpriteResHandle)ib;
				}
			}
		}
	}

	DLOGT("Unable to allocate Spriter");
	return SpriteResHandleNone;
}

/////////////////////////////////////////////////////////////

BMPresHandle AllocBMPres(void) {
	if (BMPres) {
		unsigned int i;

		for (i=0;i < BMPresMax;i++) {
			struct BMPres *we = GetBMPresNRC(i);
			if (!(we->flags & BMPresFlag_Allocated)) {
				we->flags |= BMPresFlag_Allocated;
				DLOGT("BMPr #%u allocated",i);
				return (BMPresHandle)i;
			}
		}
	}

	DLOGT("Unable to allocate BMPr");
	return BMPresHandleNone;
}

/////////////////////////////////////////////////////////////

void InitSpriteGridFromBMP(SpriteResHandle sh,const BMPresHandle bh,int bx,int by,const unsigned int cols,const unsigned int rows,const unsigned int cellwidth,const unsigned int cellheight) {
	unsigned int r,c;
	int cx,cy;

	DLOGT("Sprite ref gen from grid bx=%u by=%u cols=%u rows=%u cellwidth=%u cellheight=%u",
		bx,by,cols,rows,cellwidth,cellheight);

	for (r=0;r < rows;r++) {
		for (c=0;c < cols;c++) {
			struct SpriteRes *sr = GetSpriteRes(sh++);

			if (sr) {
				sr->bmp = bh;
				cx = bx + (c * cellwidth);
				cy = by + (r * cellheight);
				sr->w = cellwidth;
				sr->h = cellheight;
				if (cx < 0) { sr->w += cx; cx = 0; }
				if (cy < 0) { sr->h += cy; cy = 0; }
				if ((int)(sr->w) <= 0) continue;
				if ((int)(sr->h) <= 0) continue;
				sr->x = (unsigned int)cx;
				sr->y = (unsigned int)cy;
				sr->flags = SpriteResFlag_Allocated;
				DLOGT("Init sprite bmp #%u sprite #%u x=%u y=%u w=%u h=%u",bh,sh-1,sr->x,sr->y,sr->w,sr->h);
			}
		}
	}
}

/////////////////////////////////////////////////////////////

BOOL InitBMPresGDIObject(const BMPresHandle h,unsigned int width,unsigned int height,unsigned int flags) {
	struct BMPres *b = GetBMPres(h);

	if (b) {
		/* free only if change in dimensions */
		if (b->bmpObj != (HBITMAP)NULL && (b->width != width || b->height != height)) {
			DLOGT("Init GDI BMP object #%u res, width and height changed, freeing bitmap",h);
			FreeBMPresGDIObject(h);
		}

		if (b->bmpObj == (HBITMAP)NULL && width > 0 && height > 0) {
			HDC hDC = GetDC(hwndMain); // This DC has realized a palette that we want to use, do not use CreateCompatibleDC()
			b->width = width;
			b->height = height;
			b->flags = BMPresFlag_Allocated;
			if (flags & GDIWantTransparencyMask) {
				b->flags |= BMPresFlag_TransparencyMask;
				b->bmpObj = CreateCompatibleBitmap(hDC,width,height * 2);
			}
			else {
				b->bmpObj = CreateCompatibleBitmap(hDC,width,height);
			}
			ReleaseDC(hwndMain,hDC);

			if (!b->bmpObj) {
				DLOGT("Unable to create GDI bitmap compatible with screen of width=%u height=%u",width,height);
				return FALSE;
			}
		}
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////

// current GetDC/Release target in functions below
static BMPresHandle BMPresGDICurrent = BMPresHandleNone;
static HDC BMPresGDIbmpDC = (HDC)NULL;
static HBITMAP BMPresGDIbmpOld = (HBITMAP)NULL;

HDC BMPresGDIObjectGetDC(const BMPresHandle h) {
	const struct BMPres *b = GetBMPres(h);

	if (b && BMPresGDICurrent == BMPresHandleNone && b->bmpObj) {
		HDC retDC = CreateCompatibleDC(NULL);

		if (retDC) {
			BMPresGDIbmpOld = (HBITMAP)SelectObject(retDC,b->bmpObj);
			if (BMPresGDIbmpOld == (HBITMAP)NULL) {
				DLOGT("Unable to select bitmap into compatdc");
				DeleteDC(retDC);
				return NULL;
			}

			if (WndHanPalette) {
				if (SelectPalette(retDC,WndHanPalette,FALSE) == (HPALETTE)NULL)
					DLOGT("ERROR: Cannot select palette into BMP compat DC");

				RealizePalette(retDC);
			}

			BMPresGDICurrent = h;
			BMPresGDIbmpDC = retDC;
			return retDC;
		}
		else {
			DLOGT("Unable to CreateCompatibleDC");
		}
	}

	return NULL;
}

void BMPresGDIObjectReleaseDC(const BMPresHandle h) {
	const struct BMPres *b = GetBMPres(h);

	if (b && BMPresGDICurrent != BMPresHandleNone && BMPresGDICurrent == h) {
		if (BMPresGDIbmpDC) {
			if (BMPresGDIbmpOld != (HBITMAP)NULL) {
				SelectObject(BMPresGDIbmpDC,BMPresGDIbmpOld);
				BMPresGDIbmpOld = (HBITMAP)NULL;
			}

			if (WndHanPalette) {
				if (SelectPalette(BMPresGDIbmpDC,(HPALETTE)GetStockObject(DEFAULT_PALETTE),FALSE) == (HPALETTE)NULL)
					DLOGT("ERROR: Cannot select stock palette into BMP compat DC");
			}

			DeleteDC(BMPresGDIbmpDC);
			BMPresGDIbmpDC = (HDC)NULL;
		}
		BMPresGDICurrent = BMPresHandleNone;
	}
	else {
		DLOGT(__FUNCTION__ " attempt to release #%u res when current is #%u",h,BMPresGDICurrent);
	}
}

/////////////////////////////////////////////////////////////

void InitBlankBMPres(const BMPresHandle h,unsigned int width,unsigned int height) {
	if (!InitBMPresGDIObject(h,width,height,0)) {
		DLOGT("Unable to init GDI bitmap for blank BMPr");
		return;
	}
}

/////////////////////////////////////////////////////////////

void LoadBMPresFromBMP(const int fd,struct BMPres *br,const BMPresHandle h) {
	unsigned char *bihraw = NULL; // combined BITMAPINFOHEADER and palette for GDI to use
	unsigned char *slice = NULL;
	unsigned int sliceheight;
	BITMAPFILEHEADER bfh;
	BITMAPINFOHEADER bih;
	HDC bmpDC = (HDC)NULL;
	unsigned int bihColors;
	unsigned int bihSize;
	unsigned int y,lh,r;
	unsigned int stride;

	lseek(fd,0,SEEK_SET);

	memset(&bfh,0,sizeof(bfh));
	read(fd,&bfh,sizeof(bfh));
	/* assume bfh.bfType == 'BM' because the calling function already checked that */
	read(fd,&bih,sizeof(bih));

	/* require at least biSize >= 40. No old OS/2 format, please. */
	if (bih.biSize < 40 || bih.biSize > 2048) {
		DLOGT("BMP file biSize is wrong size, must be BITMAPINFOHEADER");
		goto finish;
	}

	DLOGT("BMP file header bfSize=%lu bfOffBits=%lu",
		(unsigned long)bfh.bfSize,
		(unsigned long)bfh.bfOffBits);

	/* biCompression == 0, uncompressed only.
	 * Don't even bother with Windows 95/NT biCompression == BI_BITFIELDS */
	if (bih.biCompression != 0) {
		DLOGT("BMP file compression not supported, must be traditional uncompressed format");
		goto finish;
	}

	DLOGT("BMP biSize=%u biWidth=%ld biHeight=%ld biPlanes=%u biBitCount=%u biSizeImage=%lu biClrUsed=%lu",
		(unsigned int)bih.biSize,
		(int32_t)bih.biWidth,
		(int32_t)bih.biHeight,
		(unsigned int)bih.biPlanes,
		(unsigned int)bih.biBitCount,
		(unsigned long)bih.biSizeImage,
		(unsigned long)bih.biClrUsed);

	if (bih.biHeight <= 0 || bih.biWidth <= 0 || bih.biHeight > 2048 || bih.biWidth > 2048) {
		DLOGT("dimensions not supported");
		goto finish;
	}
	if (bih.biPlanes != 1) {
		DLOGT("Multi-planar BMPs not supported");
		goto finish;
	}
	if (!(bih.biBitCount == 1 || bih.biBitCount == 4 || bih.biBitCount == 8 || bih.biBitCount == 16 || bih.biBitCount == 24)) {
		DLOGT("Bit depth not supported");
		goto finish;
	}

	bihColors = 0;
	bihSize = bih.biSize;
	if (bih.biBitCount <= 8) {
		bihColors = 1u << bih.biBitCount;
		if (bih.biClrUsed > 0 && bih.biClrUsed <= 0x7FFFul && bihColors > bih.biClrUsed) bihColors = bih.biClrUsed;
		bihSize += bihColors * sizeof(RGBQUAD);
	}
	DLOGT("Final BITMAPINFOHEADER size=%u colors=%u",
		bihSize,
		bihColors);

	if (bihSize < sizeof(bih)) {
		DLOGT("BUG! sizeof err on line %d",__LINE__);
		goto finish;
	}

	stride = (unsigned int)(((((unsigned long)bih.biWidth * (unsigned long)bih.biBitCount) + 31ul) & (~31ul)) >> 3ul);
	DLOGT("BMP stride %u bytes/line",stride);
	if (!stride) {
		DLOGT("Stride not valid");
		goto finish;
	}

#if TARGET_MSDOS == 32
	sliceheight = (unsigned int)bih.biHeight;
#else
	sliceheight = 0xF000u / stride;
#endif
	DLOGT("BMP loading slice height %u/%u",sliceheight,(unsigned int)bih.biHeight);
	if (!sliceheight) {
		DLOGT("Slice height not valid");
		goto finish;
	}

	slice = malloc(sliceheight * stride);
	if (!slice) {
		DLOGT("ERROR: Cannot allocate memory for BMP data loading");
		goto finish;
	}

	bihraw = malloc(bihSize);
	if (!bihraw) {
		DLOGT("ERROR: Cannot allocate memory for BITMAPINFOHEADER");
		goto finish;
	}
	memcpy(bihraw,&bih,sizeof(bih));
	{
		const unsigned int extra = bihSize - sizeof(bih);
		if (extra > 0) {
			DLOGT("Reading additional %u bytes to load the remainder of the image and palette",extra);
			read(fd,bihraw+sizeof(bih),extra);
		}
	}

	if (lseek(fd,bfh.bfOffBits,SEEK_SET) != bfh.bfOffBits) {
		DLOGT("Failed to seek to BMP DIB bits");
		goto finish;
	}

	if (!InitBMPresGDIObject(h,bih.biWidth,bih.biHeight,0)) { /* updates br->width, br->height */
		DLOGT("Unable to init GDI bitmap for BMP");
		goto finish;
	}

	bmpDC = BMPresGDIObjectGetDC(h);
	if (!bmpDC) {
		DLOGT("CreateCompatibleDC failed");
		goto finish;
	}
	DLOGT("GDI bitmap OK for BMP");

	{
		HBRUSH oldBrush,newBrush;
		HPEN oldPen,newPen;

		newPen = (HPEN)GetStockObject(NULL_PEN);
		newBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);

		oldPen = SelectObject(bmpDC,newPen);
		oldBrush = SelectObject(bmpDC,newBrush);

		Rectangle(bmpDC,0,0,br->width+1u,br->height+1u);

		SelectObject(bmpDC,oldBrush);
		SelectObject(bmpDC,oldPen);
	}

	for (y=0;y < br->height;y += sliceheight) {
		lh = sliceheight;
		if ((y+lh) > br->height) lh = br->height - y;

		if (lh > sliceheight) {
			DLOGT("BUG! line %d",__LINE__);
			break;
		}

		DLOGT("BMP slice load y=%u h=%u of height=%u bytes=%lu",y,lh,br->height,(unsigned long)lh * (unsigned long)stride);

		r = read(fd,slice,lh * stride);
		if (r < (lh * stride)) DLOGT("BMP read short of expectations, want=%u got=%u",lh * stride,r);

		{
			const int done = SetDIBits(bmpDC,br->bmpObj,y,lh,slice,(BITMAPINFO*)bihraw,DIB_RGB_COLORS);
			if (done < lh) DLOGT("SetDIBits() wrote less scanlines want=%d got=%d",lh,done);
		}
	}

finish:
	if (bmpDC) {
		BMPresGDIObjectReleaseDC(h);
		bmpDC = (HDC)NULL;
	}
	if (slice) {
		free(slice);
		slice = NULL;
	}
	if (bihraw) {
		free(bihraw);
		bihraw = NULL;
	}
}

/////////////////////////////////////////////////////////////

struct png_idat_reader {
	unsigned char*		ibuf;
	unsigned int		ibuflen;
	unsigned int		idatend;
	unsigned int		idatpos;
	uint32_t		idat_remain;
	off_t			pos;
	z_stream		z;
	BOOL			idat_eof;
};

void png_idat_reader_free(struct png_idat_reader *pr) {
	if (pr->ibuf) {
		free(pr->ibuf);
		pr->ibuf = NULL;
	}
	if (pr->z.next_in != NULL) {
		inflateEnd(&(pr->z));
		pr->z.next_in = NULL;
	}
}

void png_idat_reader_refill(struct png_idat_reader *pr,int fd) {
	DWORD length,chktype;
	unsigned char tmp[9];

	if (!pr->ibuf) return;
	if (pr->idat_eof) return;

	if (pr->idatpos >= pr->idatend)
		pr->idatpos = pr->idatend = 0;

	while (pr->idatend < pr->ibuflen) {
		if (pr->idat_remain == 0) {
			/* assume the PNG signature is already there and that the calling code verified it already */
			/* PNG chunk struct
			 *   DWORD length
			 *   DWORD chunkType
			 *   BYTE data[]
			 *   DWORD crc32 (we ignore it)
			 *
			 * 32-bit values are big endian */
			if (lseek(fd,pr->pos,SEEK_SET) != pr->pos) break;

			if (read(fd,tmp,8) != 8) break;
			tmp[8] = 0;

			length = bytswp32(*((DWORD*)(tmp+0)));
			chktype = bytswp32(*((DWORD*)(tmp+4)));

			DLOGT("[IDAT refill] PNG chunk at %lu: length=%lu chktype=0x%lx '%s'",
				(unsigned long)(pr->pos + (off_t)8),(unsigned long)length,(unsigned long)chktype,tmp+4);

			if (chktype == 0x49444154/*IDAT*/) {
				pr->pos += (off_t)8/*header we just read*/ + (off_t)length/*data*/ + (off_t)4/*crc32*/;
				pr->idat_remain = length;
			}
			else {
				pr->idat_eof = TRUE;
				break;
			}
		}
		else {
			unsigned int got,need = pr->ibuflen - pr->idatend; /* assume nonzero, this loop would exit otherwise */
			if ((uint32_t)need > pr->idat_remain) need = (unsigned int)pr->idat_remain;

			DLOG("PNG IDAT refill readpos=%u appendpos=%u buflen=%u need=%u IDATremain=%lu",
				pr->idatpos,pr->idatend,pr->ibuflen,need,(unsigned long)(pr->idat_remain));

			got = read(fd,pr->ibuf+pr->idatend,need);
			if ((int)got < 0) got = 0;
			pr->idat_remain -= (uint32_t)got;
			pr->idatend += got;

			if (got < need) {
				DLOGT("PNG IDAT unexpected short read want=%u got=%u",need,got);
				pr->idat_eof = TRUE;
			}
		}
	}
}

unsigned int png_idat_read(struct png_idat_reader *pr,unsigned char *buf,unsigned int len,int fd) {
	unsigned int r = 0,avail;
	int err;

	if (!pr->ibuf || len == 0) return 0;

	while (len > 0) {
		if (pr->idatpos > pr->idatend || pr->idatend > pr->ibuflen) {
			DLOGT("png_idat_read invalid datpos/datend buffer state");
			break;
		}

		avail = pr->idatend - pr->idatpos;
		if (avail) {
			pr->z.total_in = pr->z.total_out = 0;
			pr->z.next_in = pr->ibuf + pr->idatpos;
			pr->z.avail_in = pr->idatend - pr->idatpos;
			pr->z.next_out = buf;
			pr->z.avail_out = len;

			err = inflate(&(pr->z),Z_NO_FLUSH);
			if (err == Z_STREAM_END) {
				DLOGT("ZLIB stream end");
				pr->idat_eof = TRUE;
			}
			else if (err != Z_OK) {
				DLOGT("ZLIB stream error %d",err);
				pr->idatend = pr->idatpos = 0;
				pr->idat_eof = TRUE;
				break;
			}

			pr->idatpos += pr->z.total_in;
			if (pr->idatpos > pr->idatend) {
				DLOGT("ZLIB inflate read too much");
				break;
			}
			if (pr->z.total_out > len) {
				DLOGT("ZLIB inflate wrote too much");
				break;
			}
			buf += pr->z.total_out;
			len -= pr->z.total_out;
			r += pr->z.total_out;
		}
		else {
			png_idat_reader_refill(pr,fd);
			if (pr->idatend == 0 && pr->idatpos == 0) break;
		}
	}

	return r;
}

BOOL png_idat_reader_init(struct png_idat_reader *pr,off_t ofs) {
	if (!pr->ibuf) {
#if TARGET_MSDOS == 32
		pr->ibuflen = 256*1024;
#else
		pr->ibuflen = 15*1024;
#endif

		pr->ibuf = malloc(pr->ibuflen);
		if (!pr->ibuf) {
			DLOGT("PNG IDAT reader failed to malloc buffer");
			return FALSE;
		}

		pr->idatend = pr->idatpos = 0;
		pr->idat_eof = FALSE;
		pr->idat_remain = 0;
		pr->pos = ofs;

		memset(&(pr->z),0,sizeof(z_stream));
		if (inflateInit2(&(pr->z),15/*max window size 32KB*/) != Z_OK) {
			DLOGT("PNG IDAT reader failed to init zlib");
			png_idat_reader_free(pr);
			return FALSE;
		}
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////

void LoadBMPresFromPNG(const int fd,struct BMPres *br,const BMPresHandle h) {
	struct minipng_PLTE_color *plte = NULL;
	struct png_idat_reader pir = {0};
	struct minipng_IHDR ihdr = {0};
	unsigned char *tRNS = NULL; // PNG tRNS (transparency map)
	unsigned short tRNSlen = 0;
	unsigned char *bihraw = NULL; // combined BITMAPINFOHEADER and palette for GDI to use
	unsigned char *bihraw2 = NULL; // combined BITMAPINFOHEADER and palette for GDI to use for mask
	unsigned int plte_colors = 0;
	unsigned char *slice2 = NULL;
	unsigned char *slice = NULL;
	unsigned int pngstride = 0;
	unsigned int stride,i,y,lh;
	unsigned int sliceheight;
	unsigned int gdiFlags = 0;
	unsigned int bihSize = 0;
	HDC bmpDC = (HDC)NULL;
	unsigned char tmp[9];
	DWORD length,chktype;
	off_t ofs = 8;

#if GAMEDEBUG
	if (bytswp32(0x11223344) != 0x44332211) {
		DLOGT("bytswp32 failed to byte swap properly");
		return;
	}
#endif

	/* assume the PNG signature is already there and that the calling code verified it already */
	/* PNG chunk struct
	 *   DWORD length
	 *   DWORD chunkType
	 *   BYTE data[]
	 *   DWORD crc32 (we ignore it)
	 *
	 * 32-bit values are big endian */

	DLOGT("Loading PNG #%u res",h);

	while (1) {
		if (lseek(fd,ofs,SEEK_SET) != ofs) break;

		if (read(fd,tmp,8) != 8) break;
		tmp[8] = 0;
		ofs += 8;

		length = bytswp32(*((DWORD*)(tmp+0)));
		chktype = bytswp32(*((DWORD*)(tmp+4)));

		DLOGT("PNG chunk at %lu: length=%lu chktype=0x%lx '%s'",
			(unsigned long)ofs,(unsigned long)length,(unsigned long)chktype,tmp+4);

		if (chktype == 0x49484452/*IHDR*/ && length >= sizeof(ihdr)) {
			read(fd,&ihdr,sizeof(ihdr));

			ihdr.height = bytswp32(ihdr.height);
			ihdr.width = bytswp32(ihdr.width);

			DLOGT("PNG IHDR: width=%lu height=%lu bit_depth=%u color_type=%u compression_method=%u filter_method=%u interlace_method=%u",
				(unsigned long)ihdr.width,
				(unsigned long)ihdr.height,
				ihdr.bit_depth,
				ihdr.color_type,
				ihdr.compression_method,
				ihdr.filter_method,
				ihdr.interlace_method);
		}
		else if (chktype == 0x504C5445/*PLTE*/ && length >= 3) {
			unsigned int pclr = length / 3;
			if (pclr > 256) pclr = 256;
			plte_colors = pclr;

			plte = malloc(sizeof(struct minipng_PLTE_color) * plte_colors);
			if (plte) {
				DLOGT("Loading PNG PLTE palette colors=%u",pclr);
				read(fd,plte,sizeof(struct minipng_PLTE_color) * plte_colors);
			}
			else {
				DLOGT("ERROR: Unable to malloc PNG PLTE palette colors=%u",pclr);
			}
		}
		else if (chktype == 0x74524E53/*tRNS*/) {
			if (length > 0 && length <= 256) {
				/* NTS: tRNS can be (and often is) shorter than the color palette, which of course implies
				 *      that any palette entries beyond the end of tRNS are opaque */
				tRNSlen = length;
				DLOGT("PNG has tRNS transparency map, %u colors long",tRNSlen);

				tRNS = malloc(tRNSlen);
				if (tRNS) read(fd,tRNS,tRNSlen);
			}
		}
		else if (chktype == 0x49444154/*IDAT*/) {
			DLOGT("Found PNG IDAT, halting read to process");
			ofs -= 8;
			break;
		}

		ofs += length; /* skip data[] */

		ofs += 4; /* skip CRC32 */

		/* stop at IEND */
		if (chktype == 0x49454E44/*IEND*/) {
			DLOGT("PNG IEND unexpected");
			goto finish;
		}
	}

	if (ihdr.width == 0 || ihdr.height == 0 || ihdr.width > 2048 || ihdr.height > 2048 ||
		!(ihdr.bit_depth == 1 || ihdr.bit_depth == 2 || ihdr.bit_depth == 4 || ihdr.bit_depth == 8) ||
		ihdr.compression_method != 0 || ihdr.filter_method != 0 || ihdr.interlace_method != 0) {
		DLOGT("PNG format not supported");
		goto finish;
	}

	bihSize = sizeof(BITMAPINFOHEADER);
	if (ihdr.color_type == 3/*indexed*/) {
		bihSize += plte_colors * sizeof(RGBQUAD);
	}
	else {
		DLOGT("PNG format (color type) not supported");
		goto finish;
	}

	bihraw = malloc(bihSize);
	if (!bihraw) {
		DLOGT("Cannot alloc PNG BITMAPINFOHEADER");
		goto finish;
	}
	memset(bihraw,0,bihSize);

	/* if tRNS is present, and indexed color, and any colors are transparent, ask the GDI init for a transparency mask */
	if (ihdr.color_type == 3/*indexed*/ && tRNS && tRNSlen > 0 && plte_colors != 0) {
		for (i=0;i < tRNSlen && i < plte_colors;i++) {
			if (tRNS[i] < 0x80) {
				gdiFlags |= GDIWantTransparencyMask;
				DLOGT("PNG first tRNS transparent color is %u, will allocate transparency mask",i);
				break;
			}
		}
	}

	if (!InitBMPresGDIObject(h,ihdr.width,ihdr.height,gdiFlags)) { /* updates br->width, br->height */
		DLOGT("Unable to init GDI bitmap for BMP");
		goto finish;
	}

	bmpDC = BMPresGDIObjectGetDC(h);
	if (!bmpDC) {
		DLOGT("CreateCompatibleDC failed");
		goto finish;
	}
	DLOGT("GDI bitmap OK for BMP");

	{
		HBRUSH oldBrush,newBrush;
		HPEN oldPen,newPen;

		newPen = (HPEN)GetStockObject(NULL_PEN);
		newBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);

		oldPen = SelectObject(bmpDC,newPen);
		oldBrush = SelectObject(bmpDC,newBrush);

		Rectangle(bmpDC,0,0,br->width+1u,br->height+1u);

		SelectObject(bmpDC,oldBrush);
		SelectObject(bmpDC,oldPen);
	}

	{
		BITMAPINFOHEADER *bih = (BITMAPINFOHEADER*)bihraw;
		bih->biSize = sizeof(BITMAPINFOHEADER);
		bih->biWidth = ihdr.width;
		bih->biCompression = 0;
		bih->biPlanes = 1;

		if (ihdr.color_type == 3/*indexed*/ && ihdr.bit_depth == 2) {
			/* WinGDI does not support 2bpp, convert to 4bpp in place on load */
			bih->biBitCount = 4;
		}
		else {
			bih->biBitCount = ihdr.bit_depth;
		}

		stride = (unsigned int)(((((unsigned long)bih->biWidth * (unsigned long)bih->biBitCount) + 31ul) & (~31ul)) >> 3ul);
		pngstride = (unsigned int)((((unsigned long)ihdr.bit_depth * (unsigned long)ihdr.width) + 7ul) / 8ul);
		if (pngstride > stride) {
			DLOGT("pngstride > stride");
			goto finish;
		}

		if (gdiFlags & GDIWantTransparencyMask) {
			bih->biHeight = ihdr.height * 2;
			bih->biSizeImage = stride * ihdr.height * 2;
		}
		else {
			bih->biHeight = ihdr.height;
			bih->biSizeImage = stride * ihdr.height;
		}

		if (bih->biBitCount <= 8) {
			RGBQUAD *pal = (RGBQUAD*)(bihraw + bih->biSize);

			bih->biClrUsed = plte_colors;
			bih->biClrImportant = plte_colors;
			if (plte && plte_colors != 0u) {
				for (i=0;i < plte_colors;i++) {
					pal[i].rgbRed = plte[i].red;
					pal[i].rgbGreen = plte[i].green;
					pal[i].rgbBlue = plte[i].blue;
					pal[i].rgbReserved = 0;
				}
			}

			if ((gdiFlags & GDIWantTransparencyMask) && ihdr.color_type == 3/*indexed*/ && tRNS && tRNSlen > 0) {
				bihraw2 = malloc(bihSize);
				if (bihraw2) {
					// copy only the BITMAPINFOHEADER and then generate a new palette consisting of either
					// black or white, in order to generate the sprite mask properly.
					pal = (RGBQUAD*)(bihraw2 + bih->biSize);
					memcpy(bihraw2,bihraw,bih->biSize);
					for (i=0;i < plte_colors;i++) {
						pal[i].rgbRed = pal[i].rgbGreen = pal[i].rgbBlue = i ? 0xFF : 0x00;
					}
				}
			}
		}

#if TARGET_MSDOS == 32
		sliceheight = (unsigned int)ihdr.height;
#else
		sliceheight = 0xF000u / stride;
#endif
		DLOGT("BMP loading slice height %u/%u",sliceheight,(unsigned int)ihdr.height);
		DLOGT("BMP stride %u, PNG stride %u",stride,pngstride);
		if (!sliceheight) {
			DLOGT("Slice height not valid");
			goto finish;
		}

		slice = malloc(sliceheight * stride);
		if (!slice) {
			DLOGT("ERROR: Cannot allocate memory for BMP data loading");
			goto finish;
		}
	}

	if (pngstride < 2) {
		DLOGT("Invalid PNG stride");
		goto finish;
	}

	if (!png_idat_reader_init(&pir,ofs)) {
		DLOGT("ERROR: Cannot init PNG IDAT reader");
		goto finish;
	}

	png_idat_reader_refill(&pir,fd);
	for (y=0;y < br->height;y += sliceheight) {
		lh = sliceheight;
		if ((y+lh) > br->height) lh = br->height - y;

		if (lh > sliceheight) {
			DLOGT("BUG! line %d",__LINE__);
			break;
		}

		DLOGT("PNG slice load y=%u h=%u of height=%u bytes=%lu",y,lh,br->height,(unsigned long)lh * (unsigned long)stride);

		if (gdiFlags & GDIWantTransparencyMask) {
			slice2 = malloc(sliceheight * stride);
			if (!slice2) {
				DLOGT("ERROR: Cannot allocate memory for BMP data loading (2)");
				goto finish;
			}
		}

		{
			unsigned char BlackColor;
			unsigned char filter;
			unsigned int ny,my;
			unsigned int sy,sr;

			if (gdiFlags & GDIWantTransparencyMask) {
				/* The transparent color must be mapped to black in the BITMAPINFOHEADER palette in order for
				 * SRCPAINT and SRCAND to work properly to produce proper sprite transparency */
				BlackColor = 0;
				for (i=0;i < tRNSlen;i++) {
					if (tRNS[i] < 0x80) {
						BITMAPINFOHEADER *bih = (BITMAPINFOHEADER*)bihraw;
						RGBQUAD *pal = (RGBQUAD*)(bihraw + bih->biSize);

						pal[i].rgbRed = pal[i].rgbGreen = pal[i].rgbBlue = 0;

						BlackColor = i;
						DLOGT("PNG transparency black color is %u",BlackColor);
						break;
					}
				}
			}

			ny = my = br->height-y-lh;

			// NTS: Because of the double height, and bitmaps are upside down, it is the normal non-mask data that must be
			//      offset by br->height, not the mask!
			if (gdiFlags & GDIWantTransparencyMask)
				ny += br->height;

			for (sy=0;sy < lh;sy++) {
				BITMAPINFOHEADER *bih = (BITMAPINFOHEADER*)bihraw;

				sr = png_idat_read(&pir,&filter,1,fd); // filter byte
				sr = png_idat_read(&pir,slice+((lh-1u-sy)*stride),pngstride,fd);
				if (sr != pngstride) DLOGT("PNG IDAT decompress short read want=%u got=%u sy=%u y=%u lh=%u",pngstride,sr,sy,sy+y,lh);

				/* convert 2bpp to 4bpp, in place, backwards */
				/* NTS: This 2bpp processing is the only way to handle monochrome image with transparency */
				if (ihdr.color_type == 3/*indexed*/ && ihdr.bit_depth == 2 && bih->biBitCount == 4 && pngstride != 0 && (pngstride*2) <= stride) {
					unsigned char *row = slice+((lh-1u-sy)*stride);
					unsigned int i = pngstride;

					while ((i--) != 0) {
						const unsigned char b2 = row[i];
						row[i*2 + 0] = (((b2 >> 6u) & 3u) << 4u) + ((b2 >> 4u) & 3u);
						row[i*2 + 1] = (((b2 >> 2u) & 3u) << 4u) + ( b2        & 3u);
					}
				}
			}

			if (gdiFlags & GDIWantTransparencyMask) {
				BITMAPINFOHEADER *bih = (BITMAPINFOHEADER*)bihraw;

				// copy the slice to another buffer to become the mask.
				memcpy(slice2,slice,stride * lh);

				// filter transparent pixels to black so the OR operation works.
				if (bih->biBitCount == 8) {
					{
						unsigned int i = 0,t = stride * lh;

						while (i < t) {
							const unsigned char c = slice[i];

							if (c < tRNSlen && tRNS[c] < 0x80)
								slice[i] = BlackColor;

							i++;
						}
					}

					// filter all pixels to either black OR white based on transparency alone in order
					// for the sprite mask (AND operation) to work
					{
						unsigned int i = 0,t = stride * lh;

						while (i < t) {
							const unsigned char c = slice2[i];

							if (c < tRNSlen)
								slice2[i++] = (tRNS[c] >= 0x80) ? 0x00 : 0x01;
							else
								slice2[i++] = 0x00;
						}
					}
				}
				else if (bih->biBitCount == 4) {
					{
						unsigned int i = 0,t = stride * lh;

						while (i < t) {
							unsigned char c1 =  slice[i]        & 0xFu;
							unsigned char c2 = (slice[i] >> 4u) & 0xFu;

							if (c1 < tRNSlen && tRNS[c1] < 0x80)
								c1 = BlackColor;
							if (c2 < tRNSlen && tRNS[c2] < 0x80)
								c2 = BlackColor;

							slice[i++] = c1 | (c2 << 4u);
						}
					}

					// filter all pixels to either black OR white based on transparency alone in order
					// for the sprite mask (AND operation) to work
					{
						unsigned int i = 0,t = stride * lh;

						while (i < t) {
							unsigned char c1 =  slice2[i]        & 0xFu;
							unsigned char c2 = (slice2[i] >> 4u) & 0xFu;

							if (c1 < tRNSlen)
								c1 = (tRNS[c1] >= 0x80) ? 0x00 : 0x01;
							else
								c1 = 0x00;

							if (c2 < tRNSlen)
								c2 = (tRNS[c2] >= 0x80) ? 0x00 : 0x01;
							else
								c2 = 0x00;

							slice2[i++] = c1 | (c2 << 4u);
						}
					}
				}
			}

			{
				const int done = SetDIBits(bmpDC,br->bmpObj,ny,lh,slice,(BITMAPINFO*)bihraw,DIB_RGB_COLORS);
				if (done < lh) DLOGT("SetDIBits() wrote less scanlines want=%d got=%d",lh,done);
			}

			if (gdiFlags & GDIWantTransparencyMask) {
				const int done = SetDIBits(bmpDC,br->bmpObj,my,lh,slice2,(BITMAPINFO*)bihraw2,DIB_RGB_COLORS);
				if (done < lh) DLOGT("SetDIBits() wrote less scanlines (mask) want=%d got=%d",lh,done);
			}
		}
	}

finish:
	png_idat_reader_free(&pir);
	if (bmpDC) {
		BMPresGDIObjectReleaseDC(h);
		bmpDC = (HDC)NULL;
	}
	if (tRNS) {
		free(tRNS);
		tRNS = NULL;
	}
	if (slice) {
		free(slice);
		slice = NULL;
	}
	if (slice2) {
		free(slice2);
		slice2 = NULL;
	}
	if (plte) {
		free(plte);
		plte = NULL;
	}
	if (bihraw) {
		free(bihraw);
		bihraw = NULL;
	}
	if (bihraw2) {
		free(bihraw2);
		bihraw2 = NULL;
	}
}

/////////////////////////////////////////////////////////////

BOOL LoadBMPres(const BMPresHandle h,const char *p) {
	struct BMPres *b = GetBMPres(h);

	if (b) {
		int fd;

		fd = open(p,O_RDONLY|O_BINARY);
		if (fd >= 0) {
			char tmp[8] = {0};

			DLOGT("Loading BMP #%u res from %s",(unsigned int)h,p);

			lseek(fd,0,SEEK_SET);
			read(fd,tmp,8);

			if (!memcmp(tmp,"BM",2)) {
				DLOGT("BMP image in %s",p);
				LoadBMPresFromBMP(fd,b,h);
			}
			else if (!memcmp(tmp,"\x89PNG\x0D\x0A\x1A\x0A",8)) {
				DLOGT("PNG image in %s",p);
				LoadBMPresFromPNG(fd,b,h);
			}
			else {
				DLOGT("Unable to identify image type in %s",p);
			}

			close(fd);
		}
		else {
			DLOGT("Unable to load BMP #%u res from %s",(unsigned int)h,p);
			return FALSE;
		}
	}
	else {
		DLOGT("Unable to load BMP #%u res from %s, handle out of range",(unsigned int)h,p);
		return FALSE;
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////

void FreeSpriteRes(const SpriteResHandle h) {
	struct SpriteRes *r = GetSpriteRes(h);

	if (r && (r->flags & SpriteResFlag_Allocated)) {
		DLOGT("Freeing sprite #%u res",h);
		r->flags &= ~(SpriteResFlag_Allocated);
		r->x = r->y = r->w = r->h = 0;
		r->bmp = BMPresHandleNone;
	}
}

void FreeSpriteResources(void) {
	unsigned int i;

	if (SpriteRes) {
		DLOGT("Freeing sprite res");
		for (i=0;i < SpriteResMax;i++) FreeSpriteRes(i);
		free(SpriteRes);
		SpriteRes = NULL;
	}
}

void ShowWindowElement(const WindowElementHandle h,const BOOL how);

void WindowElementFreeOwnedImage(const WindowElementHandle h) {
	struct WindowElement *we = GetWindowElement(h);
	if (we && (we->flags & WindowElementFlag_OwnsImage)) {
		if (we->imgRef != ImageRefNone && ImageRefGetType(we->imgRef) == ImageRefTypeBitmap) {
			DLOGT("Window element #%u owns a bitmap and is freeing it now",h);
			FreeBMPres((BMPresHandle)ImageRefGetRef(we->imgRef));
			we->flags &= ~(WindowElementFlag_OwnsImage);
			we->imgRef = ImageRefNone;
		}
	}
}

WindowElementHandle AllocWindowElement(void) {
	if (WindowElement) {
		unsigned int i;

		for (i=0;i < WindowElementMax;i++) {
			struct WindowElement *we = GetWindowElementNRC(i);
			if (!(we->flags & WindowElementFlag_Allocated)) {
				we->flags |= WindowElementFlag_Allocated;
				DLOGT("Window element #%u allocated",i);
				return (WindowElementHandle)i;
			}
		}
	}

	DLOGT("Unable to allocate window element");
	return WindowElementHandleNone;
}

void FreeWindowElement(const WindowElementHandle h) {
	struct WindowElement *we = GetWindowElement(h);
	if (we && (we->flags & WindowElementFlag_Allocated)) {
		DLOGT("Window element #%u freeing",h);
		ShowWindowElement(h,FALSE);
		WindowElementFreeOwnedImage(h);
		we->flags &= ~(WindowElementFlag_Allocated);
		we->imgRef = ImageRefNone;
	}
}

void FreeWindowElements(void) {
	unsigned int i;

	if (WindowElement) {
		DLOGT("Freeing window elements");
		for (i=0;i < WindowElementMax;i++) FreeWindowElement(i);
		free(WindowElement);
		WindowElement = NULL;
	}
}

/////////////////////////////////////////////////////////////

void DrawWindowElement(HDC hDC,struct WindowElement *we) {
	if ((we->flags & WindowElementFlag_Enabled) && we->imgRef != ImageRefNone) {
		HBITMAP bmp = (HBITMAP)NULL;

		if (ImageRefGetType(we->imgRef) == ImageRefTypeBitmap) {
			const struct BMPres *br = GetBMPres((BMPresHandle)ImageRefGetRef(we->imgRef));
			if (br && br->bmpObj) bmp = br->bmpObj;
		}
		else if (ImageRefGetType(we->imgRef) == ImageRefTypeSprite) {
			const struct SpriteRes *sr = GetSpriteRes((SpriteResHandle)ImageRefGetRef(we->imgRef));
			if (sr) {
				const struct BMPres *br = GetBMPres(sr->bmp);
				if (br && br->bmpObj) bmp = br->bmpObj;
			}
		}

		if (bmp) {
			HDC bDC = CreateCompatibleDC(NULL);
			if (bDC) {
				HBITMAP ob = (HBITMAP)SelectObject(bDC,(HGDIOBJ)bmp);
#if 0//DEBUG: Show the redraw by making it momentarily flicker white
				SelectObject(hDC,GetStockObject(WHITE_BRUSH));
				Rectangle(hDC,we->x,we->y,we->x+we->w+1,we->y+we->h+1);
#endif
				if (we->scale)
					StretchBlt(hDC,we->x,we->y,we->w,we->h,bDC,we->sx,we->sy,we->w / (we->scale + 1u),we->h / (we->scale + 1u),SRCCOPY);
				else
					BitBlt(hDC,we->x,we->y,we->w,we->h,bDC,we->sx,we->sy,SRCCOPY);

				SelectObject(bDC,(HGDIOBJ)ob);
				DeleteDC(bDC);
			}
		}
	}

	if (we->flags & WindowElementFlag_BkUpdate)
		WndStateFlags |= WndState_NeedBkRedraw;

	we->flags &= ~(WindowElementFlag_Update | WindowElementFlag_BkUpdate);
}

void DoDrawWindowElementUpdate(HDC hDC,const WindowElementHandle h) {
	struct WindowElement *we = GetWindowElement(h);

	if (we) {
		if (we->flags & WindowElementFlag_Update)
			DrawWindowElement(hDC,we);
	}
}

/////////////////////////////////////////////////////////////

void DrawBackgroundSub(HDC hDC,RECT* updateRect) {
	HPEN oldPen,newPen;
	HBRUSH oldBrush;

	newPen = (HPEN)GetStockObject(NULL_PEN);

	oldPen = SelectObject(hDC,newPen);

	if (WndBkBrush)
		oldBrush = SelectObject(hDC,WndBkBrush);
	else
		oldBrush = SelectObject(hDC,(HBRUSH)GetStockObject(BLACK_BRUSH));

	Rectangle(hDC,updateRect->left,updateRect->top,updateRect->right+1,updateRect->bottom+1);

	SelectObject(hDC,oldBrush);
	SelectObject(hDC,oldPen);
}

// NTS: To avoid painting over the window elements, this function expects the caller
//      to set the region to update to the clip region, and then excluse from the region
//      the rectangular area of each window element. What WM_PAINT does, for example.
void DrawBackground(HDC hDC,RECT* updateRect) {
	DrawBackgroundSub(hDC,updateRect);
	WndStateFlags &= ~WndState_NeedBkRedraw;
}

/////////////////////////////////////////////////////////////

void UpdateWindowElementsHDCWithClipRegion(HDC hDC,HRGN rgn,RECT *rgnRect) {
	unsigned int i;
	HRGN orgn;

	orgn = CreateRectRgn(0,0,0,0);
	if (rgn && orgn) {
		SelectClipRgn(hDC,rgn);

		if (WindowElement) {
			i = WindowElementMax;
			while ((i--) != 0) {
				struct WindowElement *we = GetWindowElementNRC(i);
				if (we->flags & WindowElementFlag_Enabled) {
					/* detect overlap using orgn, then add the current element to orgn */
					{
						RECT um;
						um.left = we->x;
						um.top = we->y;
						um.right = we->x+we->w;
						um.bottom = we->y+we->h;
						if (RectInRegion(orgn,&um)) {
							we->flags |= WindowElementFlag_Overlapped;
							if (WndStateFlags & WndState_NeedBkRedraw)
								we->flags |= WindowElementFlag_Update | WindowElementFlag_Allocated;
						}
						else {
							if (we->flags & WindowElementFlag_Overlapped) {
								we->flags &= ~WindowElementFlag_Overlapped;
								if (WndStateFlags & WndState_NeedBkRedraw)
									we->flags |= WindowElementFlag_Update | WindowElementFlag_Allocated;
							}
						}
					}

					{
						HRGN cr = CreateRectRgn(we->x,we->y,we->x+we->w,we->y+we->h);
						if (cr) CombineRgn(orgn,orgn,cr,RGN_OR);
						DeleteObject(cr);
					}
				}
				else {
					we->flags &= ~WindowElementFlag_Overlapped;
				}

				DoDrawWindowElementUpdate(hDC,i);

				if (we->flags & WindowElementFlag_Enabled)
					ExcludeClipRect(hDC,we->x,we->y,we->x+we->w,we->y+we->h);
			}
		}

		if (WndStateFlags & WndState_NeedBkRedraw)
			DrawBackground(hDC,rgnRect); // clears NeedBkRedraw

		SelectClipRgn(hDC,NULL);
	}
	if (orgn) DeleteObject(orgn);
}

void UpdateWindowElementsHDC(HDC hDC) {
	HRGN rgn;
	RECT ur;

	ur.top = ur.left = 0;
	ur.right = WndCurrentSizeClient.x;
	ur.bottom = WndCurrentSizeClient.y;
	rgn = CreateRectRgn(ur.left,ur.top,ur.right,ur.bottom);
	if (rgn) {
		UpdateWindowElementsHDCWithClipRegion(hDC,rgn,&ur);
		DeleteObject(rgn);
	}
}

void UpdateWindowElements(void) {
	HDC hDC = GetDC(hwndMain);
	UpdateWindowElementsHDC(hDC);
	ReleaseDC(hwndMain,hDC);
}

void UpdateWindowElementsPaintStruct(HDC hDC,HRGN rgn,RECT *upRgn) {
	UpdateWindowElementsHDCWithClipRegion(hDC,rgn,upRgn);
	DeleteObject(rgn);
}

/////////////////////////////////////////////////////////////

BOOL IsWindowElementVisible(const WindowElementHandle h) {
	struct WindowElement *we = GetWindowElement(h);
	if (we) return (we->flags & WindowElementFlag_Enabled) ? TRUE : FALSE;
	return FALSE;
}

void ShowWindowElement(const WindowElementHandle h,const BOOL how) {
	const WORD enf = how ? WindowElementFlag_Enabled : 0;
	struct WindowElement *we = GetWindowElement(h);

	if (we && (we->flags & WindowElementFlag_Enabled) != enf) {
		if (how) {
			we->flags |= WindowElementFlag_Update | WindowElementFlag_Enabled | WindowElementFlag_Allocated;
		}
		else {
			we->flags |= WindowElementFlag_Update | WindowElementFlag_BkUpdate | WindowElementFlag_Allocated;
			we->flags &= ~WindowElementFlag_Enabled;
		}
	}
}

WindowElementHandle WindowElementFromPoint(int x,int y) {
	if (WindowElement) {
		unsigned int i = WindowElementMax;
		while ((i--) != 0) {
			struct WindowElement *we = GetWindowElementNRC(i);
			if (we->flags & WindowElementFlag_Enabled) {
				if (x >= we->x && x < (we->x+we->w) && y >= we->y && y < (we->y+we->h))
					return (WindowElementHandle)i;
			}
		}
	}

	return WindowElementHandleNone;
}

void SetWindowElementPosition(const WindowElementHandle h,int x,int y) {
	struct WindowElement *we = GetWindowElement(h);

	if (we) {
		if (we->w && we->h && (we->x != x || we->y != y))
			we->flags |= WindowElementFlag_Update | WindowElementFlag_BkUpdate | WindowElementFlag_Allocated;

		we->x = x;
		we->y = y;
	}
}

void UpdateWindowElementDimensions(const WindowElementHandle h) {
	struct WindowElement *we = GetWindowElement(h);

	if (we) {
		unsigned int nw = 0,nh = 0,nsx = 0,nsy = 0;

		if (we->imgRef != ImageRefNone) {
			if (ImageRefGetType(we->imgRef) == ImageRefTypeBitmap) {
				const struct BMPres *br = GetBMPres((BMPresHandle)ImageRefGetRef(we->imgRef));

				if (br && br->bmpObj && (br->flags & BMPresFlag_Allocated)) {
					nw = br->width;
					nh = br->height;
				}
			}
			else if (ImageRefGetType(we->imgRef) == ImageRefTypeSprite) {
				const struct SpriteRes *sr = GetSpriteRes((SpriteResHandle)ImageRefGetRef(we->imgRef));

				if (sr && sr->bmp != BMPresHandleNone && (sr->flags & SpriteResFlag_Allocated)) {
					nw = sr->w;
					nh = sr->h;
					nsx = sr->x;
					nsy = sr->y;
				}
			}
		}

		/* we allow integer scaling up i.e. for pixel art or when rendering is slower than display, etc */
		nw *= ((unsigned int)we->scale + 1u);
		nh *= ((unsigned int)we->scale + 1u);

		/* if the reference or source x/y coords changed, update the element */
		if (we->sx != nsx || we->sy != nsy) {
			we->flags |= WindowElementFlag_Update | WindowElementFlag_Allocated;
			we->sx = nsx;
			we->sy = nsy;
		}

		/* if the region changed size, need to redraw background AND update the element */
		if (we->w != nw || we->h != nh) {
			if (!(we->flags & WindowElementFlag_NoAutoSize)) {
				we->flags |= WindowElementFlag_Update | WindowElementFlag_BkUpdate | WindowElementFlag_Allocated;
				we->w = nw;
				we->h = nh;
			}
		}

	}
}

void SetWindowElementScale(const WindowElementHandle h,const unsigned int scale) {
	struct WindowElement *we = GetWindowElement(h);
	if (we && scale > 0u && scale <= 8u && we->scale != (scale - 1u)) {
		we->scale = scale - 1u;
		we->flags |= WindowElementFlag_Update | WindowElementFlag_Allocated;
		UpdateWindowElementDimensions(h);
	}
}

void SetWindowElementContent(const WindowElementHandle h,const ImageRef ir) {
	struct WindowElement *we = GetWindowElement(h);
	if (we && we->imgRef != ir) {
		we->imgRef = ir;
		we->flags |= WindowElementFlag_Update | WindowElementFlag_Allocated;
		UpdateWindowElementDimensions(h);
	}
}

/////////////////////////////////////////////////////////////

// Generic demonstration function---may disappear later
void DrawTextBMPres(const BMPresHandle h,const FontResourceHandle fh,const char *txt) {
	const struct FontResource *fr = GetFontResource(fh);
	const unsigned int txtlen = strlen(txt);
	struct BMPres *br = GetBMPres(h);

	if (br && fr) {
		HDC bmpDC = BMPresGDIObjectGetDC(h);

		if (bmpDC) {
			RECT um;
			HFONT fhold;

			if (fr->fontObj) fhold = (HFONT)SelectObject(bmpDC,fr->fontObj);

			um.left = um.top = 0;
			um.right = br->width;
			um.bottom = br->height;
			DrawBackgroundSub(bmpDC,&um);

			SetBkMode(bmpDC,TRANSPARENT);
			SetTextAlign(bmpDC,TA_CENTER|TA_TOP);

			/* NTS: Apparently bright green on monochrome 1bpp displays is not enough to display as white */
			if (WndScreenInfo.TotalBitsPerPixel >= 4)
				SetTextColor(bmpDC,RGB(0,255,0));
			else
				SetTextColor(bmpDC,RGB(255,255,255));

			TextOut(bmpDC,br->width/2,(br->height - fr->height)/2,txt,txtlen);

			if (fr->fontObj) SelectObject(bmpDC,fhold);

			BMPresGDIObjectReleaseDC(h);

			br->flags |= WindowElementFlag_Update | WindowElementFlag_Allocated;
		}
	}
}

/////////////////////////////////////////////////////////////

UINT near SpriteAnimFrame = 0;
SpriteResHandle near SpriteAnimBaseFrame = 0;
BYTE near MouseCapture = 0;
WindowElementHandle near MouseDragWinElem = WindowElementHandleNone;
POINT near MouseDragWinElemOrigin = {0,0};
UINT near IdleTimerId = 0;

// NTS: The reason time is tracked in ms using both timeGetTime() and GetTickCount() is
//      because of some interesting quirks about timeGetTime() in Windows 3.1 that do
//      not occur in Windows 95/NT and later.
//
//      timeGetTime() appears to get "stuck" within a 55ms period if this program does
//      not process the message processing queue enough. Which means if you try to do
//      an animation loop in Windows 3.1 that takes longer than 50ms without processing
//      the message queue, and the loop only terminates when, say 100ms has passed,
//      the loop will never break. While this doesn't happen often, it can happen from
//      experience if you use the timer to do smooth BitBlt or GDI drawing effects, as
//      experienced while developing the DEC VT100 console window emulation in DOSLIB hw/dos.
//
//      To avoid the 47 day rollover, deltas in time are counted and added to a 64-bit
//      variable.
//
//      This hack is not necessary in Windows 95/NT and we can safely use timeGetTime()
//      without the additional checks.
//
// NTS: This double checking is disabled for Win386. For whatever reason, the Win386
//      extender does not like GetTickCount() / GetCurrentTime() and calling those functions
//      will work ONCE and then later calls will hang the program.
uint64_t near CurrentTimeMS = 0,acCurrentTimeMM = 0;
#if !defined(WIN386) // Win386 extender does not like GetCurrentTime()
uint64_t near acCurrentTimeWM = 0;
#endif
DWORD near CurrentTimeMM = 0,pCurrentTimeMM = 0; // timeGetTime()
#if !defined(WIN386) // Win386 extender does not like GetCurrentTime()
DWORD near CurrentTimeWM = 0,pCurrentTimeWM = 0; // GetTickCount()
#endif

void InitCurrentTime(void) {
	CurrentTimeMM = pCurrentTimeMM = timeGetTime();
#if !defined(WIN386) // Win386 extender does not like GetCurrentTime()
	CurrentTimeWM = pCurrentTimeWM = GetCurrentTime();
#endif
	DLOGT("InitCurrentTime mm=%lu wm=%lu",
		(unsigned long)CurrentTimeMM,
#if !defined(WIN386) // Win386 extender does not like GetCurrentTime()
		(unsigned long)CurrentTimeWM
#else
		(unsigned long)CurrentTimeMM
#endif
	);
}

void UpdateCurrentTime(void) {
	int32_t dTimeMM;
#if !defined(WIN386) // Win386 extender does not like GetCurrentTime()
	int32_t dTimeWM;
	int64_t d;
#endif

	CurrentTimeMM = timeGetTime();
#if !defined(WIN386) // Win386 extender does not like GetCurrentTime()
	CurrentTimeWM = GetCurrentTime();
#endif

	dTimeMM = (int32_t)(CurrentTimeMM - pCurrentTimeMM);
#if !defined(WIN386) // Win386 extender does not like GetCurrentTime()
	dTimeWM = (int32_t)(CurrentTimeWM - pCurrentTimeWM);
#endif

	// only allow counting forward!
	if (dTimeMM > 0l) acCurrentTimeMM += (uint64_t)dTimeMM;
#if !defined(WIN386) // Win386 extender does not like GetCurrentTime()
	if (dTimeWM > 0l) acCurrentTimeWM += (uint64_t)dTimeWM;
#endif

	pCurrentTimeMM = CurrentTimeMM;
#if !defined(WIN386) // Win386 extender does not like GetCurrentTime()
	pCurrentTimeWM = CurrentTimeWM;
#endif

#if !defined(WIN386) // Win386 extender does not like GetCurrentTime()
	d = (int64_t)(acCurrentTimeWM - acCurrentTimeMM);
#endif

#if 0//DEBUG
	DLOGT("UpdateCurrentTime mm=%lu wm=%lu dmm=%ld dwm=%ld amm=%llu dmm=%llu mm-wm=%lld",
		(unsigned long)CurrentTimeMM,
		(unsigned long)CurrentTimeWM,
		(signed long)dTimeMM,
		(signed long)dTimeWM,
		(unsigned long long)acCurrentTimeMM,
		(unsigned long long)acCurrentTimeWM,
		(signed long long)d);
#endif

#if !defined(WIN386) // Win386 extender does not like GetCurrentTime()
	if (d <= -150ll || d >= 150ll) {
		// too far off!
		acCurrentTimeMM = acCurrentTimeWM;
		DLOGT("UpdateCurrentTime mm timer is too far off from wm timer, adjusting");
	}
	else if (d <= -60ll || d >= 60ll) {
		if (dTimeWM != 0) { // because GetTickCount() has a coarse 55ms precision to it, so only adjust when it changes
			const int32_t adj = (int32_t)(d / 20ll);
			acCurrentTimeMM += (uint64_t)adj;
			DLOGT("UpdateCurrentTime mm timer is a bit too far off from wm timer, nudging adj=%ld",(signed long)adj);
		}
	}
#endif
}

/////////////////////////////////////////////////////////////

BOOL InitIdleTimer(void) {
	if (!IdleTimerId) {
		IdleTimerId = SetTimer(hwndMain,0,1000 / 10,NULL);
		if (!IdleTimerId) return FALSE;
	}

	return TRUE;
}

void FreeIdleTimer(void) {
	if (IdleTimerId) {
		KillTimer(hwndMain,IdleTimerId);
		IdleTimerId = 0;
	}
}

void GoAppIdleNothing(void) {
	WndStateFlags &= ~WndState_NotIdle;
	FreeIdleTimer();
}

void GoAppIdleTimer(void) {
	WndStateFlags &= ~WndState_NotIdle;
	InitIdleTimer();
}

void GoAppActive(void) {
	WndStateFlags |= WndState_NotIdle;
	FreeIdleTimer();
}

/////////////////////////////////////////////////////////////

#if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && defined(WIN386))
LRESULT PASCAL FAR WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
#else
LRESULT WINAPI WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
#endif
	if (message == WM_CREATE) {
		return 0; /* Success */
	}
	else if (message == WM_DESTROY) {
		FreeColorPalette();
		PostQuitMessage(0);
		return 0; /* OK */
	}
	else if (message == WM_SIZE) {
		WndStateFlags &= ~WndState_Maximized;
		if (wparam == SIZE_MINIMIZED) {
			WndStateFlags |= WndState_Minimized;

			DLOGT("WM_SIZE: Minimized min=%u max=%u",
				(WndStateFlags & WndState_Minimized)?1:0,
				(WndStateFlags & WndState_Maximized)?1:0);

			if (WndConfigFlags & WndCFG_TopMost) {
				SetWindowPos(hwndMain,HWND_NOTOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE); // take away topmost
			}
		}
		else {
			WndStateFlags &= ~WndState_Minimized;
			if (wparam == SIZE_MAXIMIZED) WndStateFlags |= WndState_Maximized;
			WndCurrentSizeClient.x = LOWORD(lparam);
			WndCurrentSizeClient.y = HIWORD(lparam);

			/* WM_SIZE is sent after the window has been resized, so asking for the window RECT is OK */
			{
				RECT um;
				GetWindowRect(hwnd,&um);
				WndCurrentSize.x = um.right - um.left;
				WndCurrentSize.y = um.bottom - um.top;
			}

			DLOGT("WM_SIZE: Client={w=%d, h=%d}, Window={w=%d, h=%d} min=%u max=%u",
				WndCurrentSizeClient.x,WndCurrentSizeClient.y,WndCurrentSize.x,WndCurrentSize.y,
				(WndStateFlags & WndState_Minimized)?1:0,
				(WndStateFlags & WndState_Maximized)?1:0);

			if (WndConfigFlags & WndCFG_TopMost) {
				SetWindowPos(hwndMain,HWND_TOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE);
			}
		}

		return 0;
	}
	else if (message == WM_ACTIVATE) {
		if (wparam == WA_CLICKACTIVE || wparam == WA_ACTIVE) {
			WndStateFlags |= WndState_Active;
			DLOGT("Game window activated");
		}
		else {
			WndStateFlags &= ~WndState_Active;
			DLOGT("Game window deactivated");

			if (WndConfigFlags & WndCFG_DeactivateMinimize) {
				ShowWindow(hwnd,SW_SHOWMINNOACTIVE);
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
#if WINVER >= 0x30A /* Did not appear until Windows 3.1 */
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
		if (IsZoomed(hwnd))
			WndStateFlags |= WndState_Maximized;
		else
			WndStateFlags &= ~WndState_Maximized;

		if ((WndConfigFlags & WndCFG_Fullscreen) && !(WndStateFlags & WndState_Minimized)) {
			wpc->x = WndFullscreenSize.left;
			wpc->y = WndFullscreenSize.top;
		}

		return DefWindowProc(hwnd,message,wparam,lparam);
	}
#endif
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
		if (IsZoomed(hwnd))
			WndStateFlags |= WndState_Maximized;
		else
			WndStateFlags &= ~WndState_Maximized;

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
			mmi->ptMaxPosition.x = clamp0(((WndWorkArea.right - WndWorkArea.left) - mmi->ptMaxSize.x) / 2) + WndWorkArea.left;
			mmi->ptMaxPosition.y = clamp0(((WndWorkArea.bottom - WndWorkArea.top) - mmi->ptMaxSize.y) / 2) + WndWorkArea.top;
		}
	}
	else if (message == WM_SETCURSOR) {
		if (LOWORD(lparam) == HTCLIENT) {
			SetCursor(LoadCursor(NULL,IDC_ARROW));
			return 1;
		}
		else {
			return DefWindowProc(hwnd,message,wparam,lparam);
		}
	}
	else if (message == WM_ERASEBKGND) {
		return 0; // let WM_PAINT do the background erase
	}
	else if (message == WM_SYSCOMMAND) {
		if (LOWORD(wparam) >= 0xF000) {
			/* "In WM_SYSCOMMAND messages the four low-order bits of the wCmdType (wparam) parameter
			 * are used internally by Windows. When an application tests the value of wCmdType it must
			 * combine the value 0xFFF0 with wCmdType to obtain the correct result"
			 *
			 * OK whatever Microsoft */
			switch (LOWORD(wparam) & 0xFFF0) {
				case SC_MOVE:
					if ((WndConfigFlags & WndCFG_Fullscreen) && !(WndStateFlags & WndState_Minimized)) return 0;
					if (WndStateFlags & WndState_Maximized) return 0;
					break;
				case SC_MINIMIZE:
					if (WndConfigFlags & WndCFG_TopMost) {
						SetWindowPos(hwndMain,HWND_NOTOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE); // take away topmost
					}
					break;
				case SC_MAXIMIZE:
					if (WndConfigFlags & WndCFG_Fullscreen) {
						// NTS: It is very important to SW_SHOWNORMAL then SW_SHOWMAXIMIZED or else Windows 3.0
						//      will only show the window in normal maximized dimensions
						if (WndStateFlags & WndState_Minimized) ShowWindow(hwnd,SW_SHOWNORMAL);
						ShowWindow(hwnd,SW_SHOWMAXIMIZED);
						return 0;
					}
					break;
				case SC_RESTORE:
					if (WndConfigFlags & WndCFG_Fullscreen) {
						// NTS: It is very important to SW_SHOWNORMAL then SW_SHOWMAXIMIZED or else Windows 3.0
						//      will only show the window in normal maximized dimensions
						if (WndStateFlags & WndState_Minimized) ShowWindow(hwnd,SW_SHOWNORMAL);
						ShowWindow(hwnd,SW_SHOWMAXIMIZED);
						return 0;
					}
					break;
				default:
					break;
			}
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
	else if (message == WM_LBUTTONDOWN) {
		if (MouseCapture == 0) SetCapture(hwnd);
		MouseCapture |= 1;

		MouseDragWinElem = WindowElementFromPoint(LOWORD(lparam),HIWORD(lparam));
		{
			struct WindowElement *we = GetWindowElement(MouseDragWinElem);
			if (we) {
				MouseDragWinElemOrigin.x = LOWORD(lparam) - we->x;
				MouseDragWinElemOrigin.y = HIWORD(lparam) - we->y;
			}
		}
	}
	else if (message == WM_LBUTTONUP) {
		if (MouseCapture & 1) {
			SetWindowElementPosition(MouseDragWinElem,LOWORD(lparam) - MouseDragWinElemOrigin.x,HIWORD(lparam) - MouseDragWinElemOrigin.y);
			UpdateWindowElements();
		}

		MouseCapture &= ~1;
		if (MouseCapture == 0) ReleaseCapture();
	}
	else if (message == WM_RBUTTONDOWN) {
		if (MouseCapture == 0) SetCapture(hwnd);
		MouseCapture |= 2;
	}
	else if (message == WM_RBUTTONUP) {
		MouseCapture &= ~2;
		if (MouseCapture == 0) ReleaseCapture();
	}
	else if (message == WM_MOUSEMOVE) {
		if (MouseCapture & 1) SetWindowElementPosition(MouseDragWinElem,LOWORD(lparam) - MouseDragWinElemOrigin.x,HIWORD(lparam) - MouseDragWinElemOrigin.y);
		if (MouseCapture) UpdateWindowElements();
	}
	else if (message == WM_KEYDOWN) {
		if (wparam == '1') {
			ShowWindowElement(0,IsWindowElementVisible(0)?FALSE:TRUE);
			UpdateWindowElements();
		}
		else if (wparam == '2') {
			ShowWindowElement(1,IsWindowElementVisible(1)?FALSE:TRUE);
			UpdateWindowElements();
		}
		else if (wparam == 'A') {
			SetBackgroundColor(RGB(255,128,0));
			DrawTextBMPres(/*BMPr*/1,/*FontRes*/0,"Hello world");
		}
		else if (wparam == 'B') {
			SetBackgroundColor(RGB(63,63,63));
			DrawTextBMPres(/*BMPr*/1,/*FontRes*/0,"Hello world");
		}

		return 0;
	}
	else if (message == WM_PAINT) {
		RECT um;

		if (GetUpdateRect(hwnd,&um,TRUE)) {
			HPALETTE oldPalette;
			PAINTSTRUCT ps;
			unsigned int i;
			HRGN rgn;

			rgn = CreateRectRgn(0,0,0,0);
			if (rgn) GetUpdateRgn(hwnd,rgn,FALSE);

			BeginPaint(hwnd,&ps);

			if (WndHanPalette) {
				oldPalette = SelectPalette(ps.hdc,WndHanPalette,(WndStateFlags & WndState_Minimized) ? TRUE : FALSE);
				if (oldPalette) RealizePalette(ps.hdc);
			}

			/* all window elements need to be redrawn */
			if (WindowElement) {
				for (i=0;i < WindowElementMax;i++) {
					struct WindowElement *we = GetWindowElementNRC(i);
					if (we->flags & WindowElementFlag_Enabled) {
						um.left = we->x;
						um.top = we->y;
						um.right = we->x+we->w;
						um.bottom = we->y+we->h;
						if (RectInRegion(rgn,&um))
							we->flags |= WindowElementFlag_Update | WindowElementFlag_Allocated;
					}
				}
			}

			if (ps.fErase)
				WndStateFlags |= WndState_NeedBkRedraw;

			UpdateWindowElementsPaintStruct(ps.hdc,rgn,&ps.rcPaint);

			if (WndHanPalette)
				SelectPalette(ps.hdc,oldPalette,TRUE);

			EndPaint(hwnd,&ps);
			if (rgn) DeleteObject(rgn);
		}

		return 0; /* Return 0 to signal we processed the message */
	}
	else if (message == WM_PALETTECHANGED || message == WM_QUERYNEWPALETTE) {
		if (message == WM_PALETTECHANGED && (HWND)wparam == hwnd)
			return 0;

		if (WndHanPalette) {
			UINT changed;

			DLOGT("Responding to message %s",message == WM_PALETTECHANGED ? "WM_PALETTECHANGED" : "WM_QUERYNEWPALETTTE");
			changed = RealizePaletteObject();
			InvalidateRect(hwnd,NULL,FALSE);

			if (message == WM_QUERYNEWPALETTE)
				return changed;
		}
	}
	else if (message == WM_TIMER) {
		if (++SpriteAnimFrame >= (12*4))
			SpriteAnimFrame = 0;

		SetWindowElementContent(0,MAKESPRITEIMAGEREF(SpriteAnimFrame+SpriteAnimBaseFrame));
		UpdateWindowElements();
	}
	else {
		return DefWindowProc(hwnd,message,wparam,lparam);
	}

	return 0;
}

/////////////////////////////////////////////////////////////

/* NOTES:
 *   For Win16, programmers generally use WINAPI WinMain(...) but WINAPI is defined in such a way
 *   that it always makes the function prolog return FAR. Unfortunately, when Watcom C's runtime
 *   calls this function in a memory model that's compact or small, the call is made as if NEAR,
 *   not FAR. To avoid a GPF or crash on return, we must simply declare it PASCAL instead. */
int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	WNDCLASS wnd;
	MSG msg;

#if GAMEDEBUG
	WndConfigFlags |= WndCFG_DebugLogging;

	if (WndConfigFlags & WndCFG_DebugLogging) {
		debug_ticks = 0;
		debug_p_ticks = GetTickCount();
		debug_log_fd = open("debug.log",O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644);
		if (debug_log_fd >= 0) {
			DLOG("=======================================================================");
			DLOG("BEGIN GAME DEBUG LOG");
#if TARGET_MSDOS == 32 && !defined(WIN386) // AKA WIN32
			{
				SYSTEMTIME st;
				memset(&st,0,sizeof(st));
				GetSystemTime(&st);
				DLOG("%04u-%02u-%02u %02u:%02u:%02u",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
			}
#else
			{
				struct _dostime_t t;
				struct _dosdate_t d;

				memset(&t,0,sizeof(t));
				memset(&d,0,sizeof(d));
				_dos_gettime(&t);
				_dos_getdate(&d);
				DLOG("%04u-%02u-%02u %02u:%02u:%02u",d.year,d.month,d.day,t.hour,t.minute,t.second);
			}
#endif
			DLOG("=======================================================================");
			DLOG("ZLIB library, resident: %s",zlibVersion());
			DLOG("ZLIB library, compiled against: %s",ZLIB_VERSION);
		}
		else {
			WndConfigFlags &= ~WndCFG_DebugLogging;
		}
	}
#endif

	(void)lpCmdLine; /* unused */

	myInstance = hInstance;

#if TARGET_MSDOS == 32 && !defined(WIN386)
	/* NTS: Mutexes in Windows 3.1 Win32s are local to the application, therefore you
	 *      cannot synchronize across multiple Win32s applications with mutexes. I'm
	 *      not even sure if the Win32s system is even paying attention to the name.
	 *      However Windows 3.1 is cooperative multitasking and another Win32s program
	 *      cannot run at the same time we are. This is needed to prevent concurrent
	 *      issues on preemptive multitasking Windows 95/NT systems. */
	{
		char tmp[256];
		if (snprintf(tmp,sizeof(tmp),"AppMutex_%s",WndProcClass) >= (sizeof(tmp)-1)) return 1;
		WndLocalAppMutex = CreateMutexA(NULL,FALSE,tmp);
		if (WndLocalAppMutex == NULL) {
			DLOGT("Win32 CreateMutex failed");
			return 1;
		}
	}

	{
		DWORD r = WaitForSingleObject(WndLocalAppMutex,INFINITE);
		if (!(r == WAIT_OBJECT_0 || r == WAIT_ABANDONED)) {
			DLOGT("Win32 mutex wait failed");
			return 1;
		}
	}

	if (CheckMultiInstanceFindWindow(FALSE))
		return 1;
#endif

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
#if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && defined(WIN386))
		if (CheckMultiInstanceFindWindow(TRUE/*mustError*/))
			return 1;
#endif

#if defined(WIN386)
		/* FIXME: Win386 builds will CRASH if multiple instances try to run this way.
		 *        Somehow, creating a window with a class registered by another Win386 application
		 *        causes Windows to hang. */
		if (MessageBox(NULL,"Win386 builds may crash if you run multiple instances. Continue?","",MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON2) == IDNO)
			return 1;
#endif
	}

	{
		DWORD t,w;
		HDC screenDC = GetDC(NULL/*for the screen*/);

		WndScreenInfo.BitsPerPixel = GetDeviceCaps(screenDC,BITSPIXEL);
		if (WndScreenInfo.BitsPerPixel == 0) WndScreenInfo.BitsPerPixel = 1;
		WndScreenInfo.BitPlanes = GetDeviceCaps(screenDC,PLANES);
		if (WndScreenInfo.BitPlanes == 0) WndScreenInfo.BitPlanes = 1;

		WndScreenInfo.ColorBitsPerPixel = 0; /* we don't know */

		/* used for picking graphics */
		WndScreenInfo.TotalBitsPerPixel =
			WndScreenInfo.RenderBitsPerPixel = WndScreenInfo.BitsPerPixel * WndScreenInfo.BitPlanes;

		/* unless RC_PALETTE is set, assume every color is reserved */
		WndScreenInfo.PaletteSize = 1u << WndScreenInfo.TotalBitsPerPixel;
		WndScreenInfo.PaletteReserved = 1u << WndScreenInfo.TotalBitsPerPixel;

		WndScreenInfo.AspectRatio.x = 1;
		w = GetDeviceCaps(screenDC,ASPECTX); if (w) WndScreenInfo.AspectRatio.x = w;
		WndScreenInfo.AspectRatio.y = 1;
		w = GetDeviceCaps(screenDC,ASPECTY); if (w) WndScreenInfo.AspectRatio.y = w;

		/* TODO: VGA driver returns AspectRatio = 10:10 call a function to reduce the fraction i.e. to 1:1 */

		t = GetDeviceCaps(screenDC,RASTERCAPS);
		if (t & RC_PALETTE) {
			WndScreenInfo.Flags |= WndScreenInfoFlag_Palette;
			w = GetDeviceCaps(screenDC,SIZEPALETTE); if (w) WndScreenInfo.PaletteSize = w;
			w = GetDeviceCaps(screenDC,COLORRES); if (w) WndScreenInfo.ColorBitsPerPixel = w;
			WndScreenInfo.PaletteReserved = GetDeviceCaps(screenDC,NUMRESERVED);
		}
		if (t & RC_BITMAP64) WndScreenInfo.Flags |= WndScreenInfoFlag_BitmapBig;

		DLOGT("GDI screen info:");
		DLOGT("  Bits Per Pixel: %u",WndScreenInfo.BitsPerPixel);
		DLOGT("  Bitplanes: %u",WndScreenInfo.BitPlanes);
		DLOGT("  Color Bits Per Pixel: %u",WndScreenInfo.ColorBitsPerPixel);
		DLOGT("  Total Bits Per Pixel: %u",WndScreenInfo.TotalBitsPerPixel);
		DLOGT("  Render Bits Per Pixel: %u",WndScreenInfo.RenderBitsPerPixel);
		DLOGT("  Palette size: %u",WndScreenInfo.PaletteSize);
		DLOGT("  Palette reserved colors: %u",WndScreenInfo.PaletteReserved);
		DLOGT("  Pixel aspect ratio: %u:%u",WndScreenInfo.AspectRatio.x,WndScreenInfo.AspectRatio.y);
		DLOGT("  Has color palette: %u",WndScreenInfo.Flags & WndScreenInfoFlag_Palette ? 1 : 0);
		DLOGT("  Big Bitmaps (64K or larger): %u",WndScreenInfo.Flags & WndScreenInfoFlag_BitmapBig ? 1 : 0);
		DLOGT("  RC_BITBLT (required): %u",(t & RC_BITBLT) ? 1 : 0);
		DLOGT("  RC_DI_BITMAP (required): %u",(t & RC_DI_BITMAP) ? 1 : 0);

		w = RC_BITBLT | RC_DI_BITMAP;
		if ((t&w) != w) {
			ReleaseDC(NULL,screenDC);
			MessageBox(NULL,"Your video driver is missing some important functions","",MB_OK);
			return 1;
		}

#if 0//DEBUG
		{
			char tmp[350],*w=tmp;
			w += sprintf(w,"bpp=%u bpln=%u tbpp=%u cbpp=%u rbpp=%u palsz=%u palrsv=%u a/r=%u:%u",
				WndScreenInfo.BitsPerPixel,
				WndScreenInfo.BitPlanes,
				WndScreenInfo.TotalBitsPerPixel,
				WndScreenInfo.ColorBitsPerPixel,
				WndScreenInfo.RenderBitsPerPixel,
				WndScreenInfo.PaletteSize,
				WndScreenInfo.PaletteReserved,
				WndScreenInfo.AspectRatio.x,
				WndScreenInfo.AspectRatio.y);
			if (WndScreenInfo.Flags & WndScreenInfoFlag_Palette)
				w += sprintf(w," PAL");
			if (WndScreenInfo.Flags & WndScreenInfoFlag_BitmapBig)
				w += sprintf(w," BMPBIG");
			MessageBox(NULL,tmp,"DEBUG ScreenInfo",MB_OK);
		}
#endif

		ReleaseDC(NULL,screenDC);
	}

	{
		/* For 32-bit programs, Windows 95 reports 4.00 (0x400)
		 * For 16-bit programs, Windows 95 reports 3.95 (0x35F) because something about Windows 3.1 application compatibility */
		DWORD v = GetVersion();
		/* Win16 GetVersion()
		 *
		 * [ MSDOS VERSION ] [ WINDOWS VERSION ]
		 *  [ major minor ]    [ minor major ]
		 *
		 * Win32 GetVersion()
		 *
		 * [bit 31: Windows Win32s/95/98/ME] [ WINDOWS VERSION ]
		 *
		 * We want:
		 * [ major minor ] */
		WindowsVersion = ((v >> 8u) & 0xFF) + ((v & 0xFFu) << 8u);

#if TARGET_MSDOS == 32 && !defined(WIN386)
		/* Win32 versions set bit 31 to indicate Windows 95/98/ME and Windows 3.1 Win32s */
		DOSVersion = 0;
		if (!(v & 0x80000000u))
			WindowsVersionFlags |= WindowsVersionFlags_NT;
#else
		DOSVersion = (WORD)(v >> 16);
		if (DOSVersion == 0x500) {
			/* get the real MS-DOS version */
			WORD x=0;

			__asm {
				mov	ax,0x3306
				xor	bx,bx
				int	21h
				jc	err1
				xchg	bl,bh
				mov	x,bx
err1:
			}

			if (x != 0) {
				DOSVersion = x;
				if (x == 0x0532) /* Windows NT NTVDM.EXE reports 5.00, real version 5.50 */
					WindowsVersionFlags |= WindowsVersionFlags_NT;
			}
		}
#endif

		DLOGT("Windows version info:");
		DLOGT("  Windows version: %u.%u",WindowsVersion >> 8,WindowsVersion & 0xFFu);
		DLOGT("  DOS version: %u.%u",DOSVersion >> 8,DOSVersion & 0xFFu);
		DLOGT("  Is Windows NT: %u",(WindowsVersionFlags & WindowsVersionFlags_NT) ? 1 : 0);

#if 0//DEBUG
		{
			char tmp[256],*w=tmp;
			w += sprintf(w,"%x %x",WindowsVersion,DOSVersion);
			if (WindowsVersionFlags & WindowsVersionFlags_NT)
				w += sprintf(w," NT");
			MessageBox(NULL,tmp,"",MB_OK);
		}
#endif
	}

	/* DIBs are always bottom up in Windows 3.1 and earlier.
	 * Windows 95/NT4 allows top-down DIBs if the BITMAPINFOHEADER biHeight value is negative */
	if (WindowsVersion >= 0x35F) WndGraphicsCaps.flags |= WndGraphicsCaps_Flags_DIBTopDown;

	DLOGT("Game graphics capabilities:");
	DLOGT("  Windows 95/NT top-down DIBs: %u",WndGraphicsCaps.flags & WndGraphicsCaps_Flags_DIBTopDown);

	{
		HMENU menu = WndMenu!=0u?LoadMenu(hInstance,MAKEINTRESOURCE(WndMenu)):((HMENU)NULL);
		BOOL fMenu = (menu!=NULL && (WndConfigFlags & WndCFG_ShowMenu))?TRUE:FALSE;
		struct WndStyle_t style = WndStyle;

		/* must be computed BEFORE creating the window */
		WinClientSizeToWindowSize(&WndMinSize,&WndMinSizeClient,&style,fMenu);
		WinClientSizeToWindowSize(&WndMaxSize,&WndMaxSizeClient,&style,fMenu);
		WinClientSizeToWindowSize(&WndDefSize,&WndDefSizeClient,&style,fMenu);

		/* assume current size == def size */
		WndCurrentSizeClient = WndDefSizeClient;
		WndCurrentSize = WndDefSize;

		/* screen size */
		WndScreenSize.x = GetSystemMetrics(SM_CXSCREEN);
		WndScreenSize.y = GetSystemMetrics(SM_CYSCREEN);

		/* work area, which by default, is screen area */
		WndWorkArea.left = 0;
		WndWorkArea.top = 0;
		WndWorkArea.right = WndScreenSize.x;
		WndWorkArea.bottom = WndScreenSize.y;

#if WINVER >= 0x30A
		// Windows 95: Get the work area, meaning, the area of the screen minus the area taken by the task bar.
		//
		// Windows 3.1 has SystemParametersInfo() but will return FALSE because of course the concept of the work
		// area did not exist then. However Win16 code in Windows 95 can call this function and get the work area
		// in the same way that any Win32 program can.
		//
		// NTS: Open Watcom win386 will abort exit this program here in Windows 3.0 because SystemParametersInfo()
		// did not exist until Windows 3.1.
		{
			RECT um;
			if (SystemParametersInfo(SPI_GETWORKAREA,0,&um,0)) WndWorkArea = um;
		}
#endif

		WinClientSizeInFullScreen(&WndFullscreenSize,&style,fMenu); /* requires WndWorkArea and WndScreenSize */

		DLOGT("Initial window sizing calculations:");
		DLOGT("  MinSize: Client={w=%d, h=%d}, Window={w=%d, h=%d}",
			WndMinSizeClient.x,WndMinSizeClient.y,WndMinSize.x,WndMinSize.y);
		DLOGT("  MaxSize: Client={w=%d, h=%d}, Window={w=%d, h=%d}",
			WndMaxSizeClient.x,WndMaxSizeClient.y,WndMaxSize.x,WndMaxSize.y);
		DLOGT("  DefSize: Client={w=%d, h=%d}, Window={w=%d, h=%d}",
			WndDefSizeClient.x,WndDefSizeClient.y,WndDefSize.x,WndDefSize.y);
		DLOGT("  ScreenSize: w=%d, h=%d",
			WndScreenSize.x,WndScreenSize.y);
		DLOGT("  WorkArea: (left,top,right,bottom){%d,%d,%d,%d}",
			WndWorkArea.left,WndWorkArea.top,WndWorkArea.right,WndWorkArea.bottom);
		DLOGT("  Fullscreen: (left,top,right,bottom){%d,%d,%d,%d}",
			WndFullscreenSize.left,WndFullscreenSize.top,WndFullscreenSize.right,WndFullscreenSize.bottom);

		hwndMain = CreateWindowEx(style.styleEx,WndProcClass,WndTitle,style.style,
			CW_USEDEFAULT,CW_USEDEFAULT,
			WndDefSize.x,WndDefSize.y,
			NULL,NULL,
			hInstance,NULL);
		if (!hwndMain) {
			MessageBox(NULL,"Unable to create window","Oops!",MB_OK);
			return 1;
		}

		if (fMenu) SetMenu(hwndMain,menu);
	}

#if WINVER < 0x400
	/* Windows 3.0/3.1 bug: AdjustWindowRect window dimensions are correct for the width of the window,
	 * but it's off by 1 (or 2 if WS_POPUP) for the height of the window. This bug only happens if there
	 * is a menu bar in the window (fMenu == TRUE). Windows 95/NT appears to have fixed this bug.
	 * The fullscreen rect calculation is not subject to this bug because, even if the menu bar is there,
	 * it calls AdjustWindowRect with fMenu == FALSE to ensure that the menu bar is visible at the top
	 * of the screen. */
	if (!(WndConfigFlags & WndCFG_Fullscreen)) {
		RECT um;

		GetClientRect(hwndMain,&um);
		DLOGT("Window client area check after CreateWindow (Windows 3.x AdjustWindowRect bug)");
		DLOGT("  ClientRect=(left,top,right,bottom){%d,%d,%d,%d}",
			um.left,um.top,um.right,um.bottom);

		WndAdjustWindowRectBug_yadd = WndDefSizeClient.y - (um.bottom - um.top);
		DLOGT("  Possible height adjust (add to bottom): %d",WndAdjustWindowRectBug_yadd);

		if (WndAdjustWindowRectBug_yadd != 0 && WndAdjustWindowRectBug_yadd >= -2 && WndAdjustWindowRectBug_yadd <= 2) {
			WndMinSize.y += WndAdjustWindowRectBug_yadd;
			WndMaxSize.y += WndAdjustWindowRectBug_yadd;
			WndDefSize.y += WndAdjustWindowRectBug_yadd;
			SetWindowPos(hwndMain,0,0,0,WndDefSize.x,WndDefSize.y,SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);

			GetClientRect(hwndMain,&um);
			DLOGT("Window client area check after CreateWindow and adjustment (Windows 3.x AdjustWindowRect bug)");
			DLOGT("  ClientRect=(left,top,right,bottom){%d,%d,%d,%d}",
				um.left,um.top,um.right,um.bottom);
		}
	}
#endif

	DLOGT("Game window configuration state at startup:");
	DLOGT("  Show menu: %u",(WndConfigFlags & WndCFG_ShowMenu) ? 1 : 0);
	DLOGT("  Fullscreen: %u",(WndConfigFlags & WndCFG_Fullscreen) ? 1 : 0);
	DLOGT("  Topmost: %u",(WndConfigFlags & WndCFG_TopMost) ? 1 : 0);
	DLOGT("  Minimize if deactivated: %u",(WndConfigFlags & WndCFG_DeactivateMinimize) ? 1 : 0);
	DLOGT("  Allow multiple instances: %u",(WndConfigFlags & WndCFG_MultiInstance) ? 1 : 0);
	DLOGT("  Fullscreen fit work area: %u",(WndConfigFlags & WndCFG_FullscreenWorkArea) ? 1 : 0);
	DLOGT("  Debug logging: %u",(WndConfigFlags & WndCFG_DebugLogging) ? 1 : 0);

	if (nCmdShow == SW_MINIMIZE || nCmdShow == SW_SHOWMINIMIZED || nCmdShow == SW_SHOWMINNOACTIVE) WndStateFlags |= WndState_Minimized;

	/* NTS: Windows 3.1 will not send WM_WINDOWPOSCHANGING for window creation because,
	 *      well, the window was just created and therefore didn't change position!
	 *      For fullscreen to work whether or not the window is maximized, position
	 *      change is necessary! To make this work with any other Windows, also maximize
	 *      the window. */
	if ((WndConfigFlags & WndCFG_Fullscreen) && !(WndStateFlags & WndState_Minimized)) {
		SetWindowPos(hwndMain,HWND_TOP,0,0,0,0,SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE);
	}

	SysMenu = GetSystemMenu(hwndMain,FALSE);
	if (WndConfigFlags & WndCFG_Fullscreen) {
		// do not remove SC_MOVE, it makes it impossible in Windows 3.x to move the minimized icon.
		// do not remove SC_RESTORE, it makes it impossible in Windows 3.x to restore the window by double-clicking the minimized icon.
		DeleteMenu(SysMenu,SC_MAXIMIZE,MF_BYCOMMAND);
		DeleteMenu(SysMenu,SC_SIZE,MF_BYCOMMAND);
	}
	if (WndConfigFlags & WndCFG_TopMost) {
		SetWindowPos(hwndMain,HWND_TOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE);
	}

	InitCurrentTime();
	InitColorPalette();
	if (!InitBMPResources()) {
		DLOGT("Unable to init BMP res");
		return 1;
	}
	if (!InitSpriteResources()) {
		DLOGT("Unable to init sprite res");
		return 1;
	}
	if (!InitWindowElements()) {
		DLOGT("Unable to init window elements");
		return 1;
	}
	if (!InitBackgroundBrush()) {
		DLOGT("Unable to init background brush");
		return 1;
	}
	if (!InitFontResources()) {
		DLOGT("Unable to init fonts");
		return 1;
	}
	if (!InitIdleTimer()) {
		DLOGT("Unable to init anim timer");
		return 1;
	}

	LoadLogPalette("palette.png");

	{
		BMPresHandle bh = AllocBMPres();
		SpriteResHandle sh = AllocSpriteRes(12/*cols*/*4/*rows*/);

		if (WndScreenInfo.TotalBitsPerPixel >= 8)
			LoadBMPres(bh,"sht1_8.png");
		else if (WndScreenInfo.TotalBitsPerPixel >= 4)
			LoadBMPres(bh,"sht1_4.png");
		else if (WndScreenInfo.TotalBitsPerPixel >= 3)
			LoadBMPres(bh,"sht1_3.png");
		else
			LoadBMPres(bh,"sht1_1.png");

		SpriteAnimBaseFrame = sh;
		InitSpriteGridFromBMP(sh/*base sprite*/,bh/*BMP*/,
			-4/*x*/,0/*y*/,
			12/*cols*/,4/*rows*/,
			44/*cell width*/,62/*cell height*/);
	}

	{
		FontResourceHandle fh = AllocFont();
		BMPresHandle bh = AllocBMPres();

		LoadFontResource(fh,/*height (by char)*/-14,/*width (default)*/0,/*flags*/0,"Arial");
		InitBlankBMPres(bh,320,32);
		DrawTextBMPres(/*BMPr*/bh,/*FontRes*/0,"Hello world! This is a text region");
	}

	SpriteAnimFrame = 0;

	{
		WindowElementHandle wh = AllocWindowElement();
		SetWindowElementContent(wh,MAKESPRITEIMAGEREF(SpriteAnimFrame+SpriteAnimBaseFrame));
		SetWindowElementPosition(wh,20,20);
		ShowWindowElement(wh,TRUE);
	}

	{
		WindowElementHandle wh = AllocWindowElement();
		SetWindowElementContent(wh,MAKEBMPIMAGEREF(0));
		SetWindowElementPosition(wh,120 + 20 + 20,20);
		ShowWindowElement(wh,TRUE);
	}

	{
		WindowElementHandle wh = AllocWindowElement();
		SetWindowElementContent(wh,MAKEBMPIMAGEREF(1));
		SetWindowElementPosition(wh,20,210);
		ShowWindowElement(wh,TRUE);
	}

	UpdateWindowElements();

	ShowWindow(hwndMain,nCmdShow);
#if WINVER < 0x30A /* Windows 3.0, if we don't process WM_ERASEBKGND on initial show, does not call WM_PAINT with fErase = TRUE */
	InvalidateRect(hwndMain,NULL,TRUE);
#else /* Windows 3.1 will correctly call WM_PAINT with fErase = TRUE if we do not process WM_ERASEBKGND */
	UpdateWindow(hwndMain);
#endif

	if (GetActiveWindow() == hwndMain) WndStateFlags |= WndState_Active;

	/* Windows 95: Show the window normally, then maximize it, to encourage the task bar to hide itself when we go fullscreen.
	 *             Without this, the task bar will SOMETIMES hide itself, and other times just stay there at the bottom of the screen. */
	if ((WndConfigFlags & WndCFG_Fullscreen) && !(WndStateFlags & WndState_Minimized) && (WndStateFlags & WndState_Active))
		ShowWindow(hwndMain,SW_MAXIMIZE);

#if TARGET_MSDOS == 32 && !defined(WIN386)
	ReleaseMutex(WndLocalAppMutex);
#endif

	while (1) {
		UpdateCurrentTime();
		if (WndStateFlags & WndState_NotIdle) {
			if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
				if (msg.message == WM_QUIT) break;
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else {
			if (!GetMessage(&msg,NULL,0,0)) break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	FreeIdleTimer();
	FreeWindowElements();
	FreeFontResources();
	FreeBMPResources();
	FreeSpriteResources();
	FreeColorPalette();
	FreeBackgroundBrush();

	return msg.wParam;
}

