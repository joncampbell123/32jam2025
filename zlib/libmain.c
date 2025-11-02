
#include <windows.h>

#if TARGET_MSDOS == 16
int CALLBACK LibMain(HINSTANCE hinst, WORD wDataSeg, WORD cbHeapSize, LPSTR lpszCmdLine) {
	(void)hinst;
	(void)wDataSeg;
	(void)cbHeapSize;
	(void)lpszCmdLine;
	return 1;
}
#endif

#if TARGET_MSDOS == 32
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD fdwReason, LPVOID lpwReserved) {
	(void)hinst;
	(void)fdwReason;
	(void)lpwReserved;
	return TRUE;
}
#endif

