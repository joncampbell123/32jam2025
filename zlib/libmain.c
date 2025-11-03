
#define ZLIB_INTERNAL

#include <windows.h>
#include "zlib.h"

#if TARGET_MSDOS == 16

#ifdef ZLIB_DLL
int CALLBACK LibMain(HINSTANCE hinst, WORD wDataSeg, WORD cbHeapSize, LPSTR lpszCmdLine) {
	(void)hinst;
	(void)wDataSeg;
	(void)cbHeapSize;
	(void)lpszCmdLine;
	return 1;
}
#endif

#ifdef USELOCKCOUNT
static WORD UseLockCount = 0;

int ZLIB_INTERNAL UseLock() {
	if (UseLockCount++ == 0) {
		WORD c=0;
		__asm {
			mov	ax,cs
			mov	c,ax
		}
		LockSegment(c); // ZLIB uses function pointers, lock code segment too
		LockSegment(-1);
	}

	return UseLockCount;
}

int ZLIB_INTERNAL UseUnlock() {
	if (--UseLockCount == 0) {
		WORD c=0;
		__asm {
			mov	ax,cs
			mov	c,ax
		}
		UnlockSegment(c);
		UnlockSegment(-1);
	}

	return UseLockCount;
}

ZEXTERN WORD ZEXPORT CheckUseLock() {
	return UseLockCount;
}
#endif

#endif // TARGET_MSDOS == 16

#if TARGET_MSDOS == 32

#ifdef ZLIB_DLL
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD fdwReason, LPVOID lpwReserved) {
	(void)hinst;
	(void)fdwReason;
	(void)lpwReserved;
	return TRUE;
}
#endif

#endif

