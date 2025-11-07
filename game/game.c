#ifndef TARGET_WINDOWS
# error This is Windows code, not DOS
#endif

#include <windows.h>
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

// WndGraphicsCaps.flags
#define WndGraphicsCaps_Flags_DIBTopDown	0x00000001u /* Windows 95/NT4 top-down DIBs are supported */

// WindowsVersionFlags
#define WindowsVersionFlags_NT			0x00000001u /* Windows NT */

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

#if WINVER < 0x400
/* Windows 3.x compensation for off by 1 (or 2) bugs in AdjustWindowRect */
int near			WndAdjustWindowRectBug_yadd = 0;
#endif

// 256-color palette
LOGPALETTE*			WndLogPalette = NULL;
HPALETTE near			WndHanPalette = (HPALETTE)NULL;

// loaded bitmap resource
struct BMPres {
	HBITMAP			bmpObj;
	unsigned short		width,height;
	unsigned short		flags;
};

#define BMPresFlag_Allocated	0x0001u

typedef WORD			BMPrHandle;
WORD near			BMPrMax = 4;
struct BMPres*			BMPr = NULL;

// sprite within bitmap
struct SpriteRes {
	BMPrHandle		bmp;
	unsigned short		x,y,w,h; /* subregion of bmp */
};

typedef WORD			SpriterHandle;
WORD near			SpriterMax = 4;
struct SpriteRes*		Spriter = NULL;

/////////////////////////////////////////////////////////////

int clamp0(int x) {
	return x >= 0 ? x : 0;
}

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
			if (SelectPalette(hdc,(HPALETTE)GetStockObject(DEFAULT_PALETTE),FALSE) == (HPALETTE)NULL)
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
			if (SelectPalette(hdc,WndHanPalette,FALSE) == (HPALETTE)NULL)
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
	if (!InitPaletteObject())
		return FALSE;

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

uint32_t bytswp32(uint32_t t) {
	t = (t << 16) | (t >> 16);
	t = ((t & 0xFF00FF00) >> 8) | ((t & 0x00FF00FF) << 8);
	return t;
}

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

void LoadLogPalette(const char *p) {
	if (WndLogPalette) {
		int fd;

		fd = open(p,O_RDONLY|O_BINARY);
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
}

#define BMPrNone ((WORD)(-1))

static const struct BMPres near BMPrInit = { .bmpObj = (HBITMAP)NULL, .width = 0, .height = 0, .flags = 0 };
static const struct SpriteRes near SpriterInit = { .bmp = BMPrNone, .x = 0, .y = 0, .w = 0, .h = 0 };

BOOL InitBMPRes(void) {
	unsigned int i;

	if (!BMPr && BMPrMax != 0) {
		DLOGT("Allocating BMP res array, %u max",BMPrMax);
		BMPr = malloc(BMPrMax * sizeof(struct BMPres));
		if (!BMPr) {
			DLOGT("Failed to allocate array");
			return FALSE;
		}
		for (i=0;i < BMPrMax;i++) BMPr[i] = BMPrInit;
	}

	return TRUE;
}

BOOL InitSpriteRes(void) {
	unsigned int i;

	if (!Spriter && SpriterMax != 0) {
		DLOGT("Allocating sprite res array, %u max",SpriterMax);
		Spriter = malloc(SpriterMax * sizeof(struct SpriteRes));
		if (!Spriter) {
			DLOGT("Failed to allocate array");
			return FALSE;
		}
		for (i=0;i < SpriterMax;i++) Spriter[i] = SpriterInit;
	}

	return TRUE;
}

BOOL IsBMPresAlloc(const BMPrHandle h) {
	if (BMPr && h < BMPrMax) {
		struct BMPres *b = BMPr + h;
		if (b->flags & BMPresFlag_Allocated) return TRUE;
	}

	return FALSE;
}

BOOL IsSpriteResAlloc(const SpriterHandle h) {
	if (BMPr && h < SpriterMax) {
		struct SpriteRes *r = Spriter + h;
		if (r->bmp != BMPrNone) return TRUE;
	}

	return FALSE;
}

void FreeBMPrGDIObject(const BMPrHandle h) {
	if (BMPr && h < BMPrMax) {
		struct BMPres *b = BMPr + h;
		if (b->bmpObj != (HBITMAP)NULL) {
			DLOGT("Freeing BMP #%u res GDI object",h);
			DeleteObject((HGDIOBJ)(b->bmpObj));
			b->bmpObj = (HBITMAP)NULL;
		}
	}
}

BOOL InitBMPrGDIObject(const BMPrHandle h,unsigned int width,unsigned int height) {
	if (BMPr && h < BMPrMax) {
		struct BMPres *b = BMPr + h;

		/* free only if change in dimensions */
		if (b->bmpObj != (HBITMAP)NULL && (b->width != width || b->height != height)) {
			DLOGT("Init GDI BMP object #%u res, width and height changed, freeing bitmap",h);
			FreeBMPrGDIObject(h);
		}

		if (b->bmpObj == (HBITMAP)NULL && width > 0 && height > 0) {
			HDC hDC = GetDC(hwndMain);
			b->width = width;
			b->height = height;
			b->flags = BMPresFlag_Allocated;
			b->bmpObj = CreateCompatibleBitmap(hDC,width,height);
			ReleaseDC(hwndMain,hDC);

			if (!b->bmpObj) {
				DLOGT("Unable to create GDI bitmap compatible with screen of width=%u height=%u",width,height);
				return FALSE;
			}
		}
	}

	return TRUE;
}

static BMPrHandle BMPrGDICurrent = BMPrNone;
static HDC BMPrGDIbmpDC = (HDC)NULL;
static HBITMAP BMPrGDIbmpOld = (HBITMAP)NULL;

HDC BMPrGDIObjectGetDC(const BMPrHandle h) {
	if (BMPr && h < BMPrMax && BMPrGDICurrent == BMPrNone) {
		struct BMPres *b = BMPr + h;
		if (b->bmpObj) {
			HDC hDC = GetDC(hwndMain);
			HDC retDC = CreateCompatibleDC(hDC);
			ReleaseDC(hwndMain,hDC);

			if (retDC) {
				BMPrGDIbmpOld = (HBITMAP)SelectObject(retDC,b->bmpObj);
				if (BMPrGDIbmpOld == (HBITMAP)NULL) {
					DLOGT("Unable to select bitmap into compatdc");
					DeleteDC(retDC);
					return NULL;
				}

				if (WndHanPalette) {
					if (SelectPalette(retDC,WndHanPalette,FALSE) == (HPALETTE)NULL)
						DLOGT("ERROR: Cannot select palette into BMP compat DC");

					RealizePalette(retDC);
				}

				BMPrGDICurrent = h;
				BMPrGDIbmpDC = retDC;
				return retDC;
			}
			else {
				DLOGT("Unable to CreateCompatibleDC");
			}
		}
	}

	return NULL;
}

void BMPrGDIObjectReleaseDC(const BMPrHandle h) {
	if (BMPr && h < BMPrMax) {
		if (BMPrGDICurrent != BMPrNone) {
			if (BMPrGDICurrent == h) {
				if (BMPrGDIbmpDC) {
					if (BMPrGDIbmpOld != (HBITMAP)NULL) {
						SelectObject(BMPrGDIbmpDC,BMPrGDIbmpOld);
						BMPrGDIbmpOld = (HBITMAP)NULL;
					}

					if (WndHanPalette) {
						if (SelectPalette(BMPrGDIbmpDC,(HPALETTE)GetStockObject(DEFAULT_PALETTE),FALSE) == (HPALETTE)NULL)
							DLOGT("ERROR: Cannot select stock palette into BMP compat DC");
					}

					DeleteDC(BMPrGDIbmpDC);
					BMPrGDIbmpDC = (HDC)NULL;
				}
				BMPrGDICurrent = BMPrNone;
			}
			else  {
				DLOGT(__FUNCTION__ " attempt to release #%u res when #%u is the one in use, not releasing",h,BMPrGDICurrent);
			}
		}
		else {
			DLOGT(__FUNCTION__ " attempt to release #%u res when none are in use, not releasing",h);
		}
	}
}

void LoadBMPrFromBMP(const int fd,struct BMPres *br,const BMPrHandle h) {
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

	if (!InitBMPrGDIObject(h,bih.biWidth,bih.biHeight)) { /* updates br->width, br->height */
		DLOGT("Unable to init GDI bitmap for BMP");
		goto finish;
	}

	bmpDC = BMPrGDIObjectGetDC(h);
	if (!bmpDC) {
		DLOGT("CreateCompatibleDC failed");
		goto finish;
	}
	DLOGT("GDI bitmap OK for BMP");

	{
		HBRUSH oldBrush,newBrush;
		HPEN oldPen,newPen;

		newPen = (HPEN)GetStockObject(NULL_PEN);
		newBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);

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
			const int done = SetDIBits(bmpDC,br->bmpObj,y,sliceheight,slice,(BITMAPINFO*)bihraw,DIB_RGB_COLORS);
			if (done < sliceheight) DLOGT("SetDIBitsToDevice() wrote less scanlines want=%d got=%d",sliceheight,done);
		}
	}

finish:
	if (bmpDC) {
		BMPrGDIObjectReleaseDC(h);
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

BOOL LoadBMPr(const BMPrHandle h,const char *p) {
	if (BMPr && h < BMPrMax) {
		struct BMPres *b = BMPr + h;
		int fd;

		fd = open(p,O_RDONLY|O_BINARY);
		if (fd >= 0) {
			char tmp[8] = {0};

			DLOGT("Loading BMP #%u res from %s",(unsigned int)h,p);

			lseek(fd,0,SEEK_SET);
			read(fd,tmp,8);

			if (!memcmp(tmp,"BM",2)) {
				DLOGT("BMP image in %s",p);
				LoadBMPrFromBMP(fd,b,h);
			}
			else if (!memcmp(tmp,"\x89PNG\x0D\x0A\x1A\x0A",8)) {
				DLOGT("PNG image in %s",p);
				DLOGT("[TODO] PNG image load");
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

void FreeBMPr(const BMPrHandle h) {
	FreeBMPrGDIObject(h);
	if (BMPr && h < BMPrMax) {
		struct BMPres *b = BMPr + h;
		if (b->flags & BMPresFlag_Allocated) {
			DLOGT("Freeing BMP #%u res",h);
			b->width = b->height = b->flags = 0;
		}
	}
}

void FreeBMPRes(void) {
	unsigned int i;

	if (BMPr) {
		DLOGT("Freeing BMP res");
		for (i=0;i < BMPrMax;i++) FreeBMPr(i);
		free(BMPr);
		BMPr = NULL;
	}
}

void FreeSpriter(const SpriterHandle h) {
	if (Spriter && h < SpriterMax) {
		struct SpriteRes *r = Spriter + h;
		if (r->bmp != BMPrNone) {
			DLOGT("Freeing sprite #%u res",h);
			r->x = r->y = r->w = r->h = 0;
			r->bmp = BMPrNone;
		}
	}
}

void FreeSpriteRes(void) {
	unsigned int i;

	if (Spriter) {
		DLOGT("Freeing sprite res");
		for (i=0;i < SpriterMax;i++) FreeSpriter(i);
		free(Spriter);
		Spriter = NULL;
	}
}

#if GAMEDEBUG
/* DEBUG: Draw a BMPr directly on the window */
void DrawBMPr(const BMPrHandle h,int x,int y) {
	if (Spriter && h < SpriterMax) {
		struct BMPres *r = BMPr + h;

		if (r->bmpObj) {
			HDC hDC = GetDC(hwndMain);
			HDC retDC = CreateCompatibleDC(hDC);

			if (retDC) {
				HBITMAP bmpOld = (HBITMAP)SelectObject(retDC,r->bmpObj);
				if (bmpOld == (HBITMAP)NULL) {
					DLOGT("Unable to select bitmap into compatdc");
					SelectObject(retDC,bmpOld);
					DeleteDC(retDC);
					return;
				}

				if (WndHanPalette) {
					if (SelectPalette(retDC,WndHanPalette,FALSE) == (HPALETTE)NULL)
						DLOGT("ERROR: Cannot select palette into BMP compat DC");

					RealizePalette(retDC);
				}

				DLOGT("BitBlt BMP #%u res",h);
				BitBlt(hDC,x,y,r->width,r->height,retDC,0,0,SRCCOPY);

				if (WndHanPalette) {
					if (SelectPalette(retDC,(HPALETTE)GetStockObject(DEFAULT_PALETTE),FALSE) == (HPALETTE)NULL)
						DLOGT("ERROR: Cannot select stock palette into BMP compat DC");
				}

				SelectObject(retDC,bmpOld);
				DeleteDC(retDC);
			}

			ReleaseDC(hwndMain,hDC);
		}
	}
}
#else
# define DrawBMPr(...)
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
	else if (message == WM_PAINT) {
		RECT um;

		if (GetUpdateRect(hwnd,&um,TRUE)) {
			PAINTSTRUCT ps;

			BeginPaint(hwnd,&ps);
			EndPaint(hwnd,&ps);

			DrawBMPr(0,0,0);
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

	InitColorPalette();
	if (!InitBMPRes()) {
		DLOGT("Unable to init BMP res");
		return 1;
	}
	if (!InitSpriteRes()) {
		DLOGT("Unable to init sprite res");
		return 1;
	}

	LoadLogPalette("palette.png");
	LoadBMPr(0,"sht1_8.bmp");

	ShowWindow(hwndMain,nCmdShow);
	UpdateWindow(hwndMain);

	if (GetActiveWindow() == hwndMain) WndStateFlags |= WndState_Active;

	/* Windows 95: Show the window normally, then maximize it, to encourage the task bar to hide itself when we go fullscreen.
	 *             Without this, the task bar will SOMETIMES hide itself, and other times just stay there at the bottom of the screen. */
	if ((WndConfigFlags & WndCFG_Fullscreen) && !(WndStateFlags & WndState_Minimized) && (WndStateFlags & WndState_Active))
		ShowWindow(hwndMain,SW_MAXIMIZE);

#if TARGET_MSDOS == 32 && !defined(WIN386)
	ReleaseMutex(WndLocalAppMutex);
#endif

	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	FreeBMPRes();
	FreeSpriteRes();
	FreeColorPalette();

	return msg.wParam;
}

