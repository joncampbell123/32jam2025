
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

