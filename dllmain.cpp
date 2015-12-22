#pragma comment(linker, "/dll")
#pragma comment(linker, "/ENTRY:\"DllEntryPoint\"") // define entry point cause no C Lib
#pragma comment(linker, "/export:DirectDrawCreate=_DirectDrawCreate@12")
#pragma comment(linker, "/NODEFAULTLIB") // specifically no C runtime lib (for no real reason)
#pragma comment( lib, "kernel32" )
#pragma comment( lib, "user32" )
#pragma comment( lib, "gdi32" )
#pragma comment( lib, "d3d9" )

#include <windows.h>
#include "header.h"

BOOL __stdcall DllEntryPoint( HINSTANCE hDll, DWORD dwReason, LPVOID lpvReserved )
{
	if( dwReason == DLL_PROCESS_ATTACH )
	{
		DisableThreadLibraryCalls( hDll );
	}
	return TRUE;
}
