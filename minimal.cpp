#pragma comment(linker, "/dll")
#pragma comment(linker, "/ENTRY:\"DllEntryPoint\"")
#pragma comment(linker, "/export:DirectDrawCreate=_DirectDrawCreate@12")
#pragma comment(linker, "/NODEFAULTLIB")
#pragma comment( lib, "kernel32" )
#pragma comment( lib, "user32" )
#pragma comment( lib, "gdi32" )

#include <windows.h>
#include <ddraw.h>

HWND hwnd_main;
HRGN hrgn_acc;

IDirectDraw* ddraw;
IDirectDrawSurface* dds_primary;
IDirectDrawPalette* ddpal;
extern const DWORD dd_vtbl[];
extern const DWORD dds_vtbl[];
extern const DWORD ddp_vtbl[];
const DWORD* const IDDraw = dd_vtbl;
const DWORD* const IDDSurf = dds_vtbl;
const DWORD* const IDDPal = ddp_vtbl;
typedef HRESULT (__stdcall* DIRECTDRAWCREATE)( GUID*, IDirectDraw**, IUnknown* );


// EnumThreadWindows callback for dd->Lock 
// top-level thread windows are enum'd by z-order ( top to bottom... hopefully )
BOOL __stdcall gdi_to_ddraw( HWND hwnd, LPARAM lParam ){	
	RECT rc;
	GetWindowRect( hwnd, &rc );
	HDC hdc = GetDCEx( hwnd, NULL, DCX_CACHE );
	BitBlt( (HDC)lParam, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hdc, 0, 0, SRCCOPY );
	ReleaseDC( hwnd, hdc );
	ExcludeClipRect( (HDC)lParam, rc.left, rc.top, rc.right, rc.bottom );
	return TRUE;
}

// EnumThreadWindows callback for dd->Unlock
BOOL __stdcall ddraw_to_gdi( HWND hwnd, LPARAM lParam ){
	RECT rc;
	HRGN hrgn_temp;
	GetWindowRect( hwnd, &rc );
	HRGN hrgn = CreateRectRgnIndirect( &rc );
	CombineRgn( hrgn, hrgn_acc, hrgn, RGN_OR );
	hrgn_temp = hrgn;
	hrgn = hrgn_acc;
	hrgn_acc = hrgn_temp;
	HDC hdc = GetDCEx( hwnd, hrgn, DCX_EXCLUDERGN | DCX_CACHE );
	BitBlt( hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, (HDC)lParam, rc.left, rc.top, SRCCOPY );
	ReleaseDC( hwnd, hdc );
	return TRUE;
}

BOOL __stdcall DllEntryPoint( HINSTANCE hDll, DWORD dwReason, LPVOID lpvReserved ){
	if( dwReason == DLL_PROCESS_ATTACH ){
		DisableThreadLibraryCalls( hDll );
		hrgn_acc = CreateRectRgn( 0,0,0,0 );
	}
	if( dwReason == DLL_PROCESS_DETACH ){
		DeleteObject( hrgn_acc );
	}
	return TRUE;
}

#pragma intrinsic( strcat )
HRESULT __stdcall DirectDrawCreate( GUID* lpGUID, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter ){
	HMODULE ddraw_dll;
	char szPath[ MAX_PATH ];
	if( GetSystemDirectory( szPath, MAX_PATH - 10 )){
		strcat( szPath, "\\ddraw.dll" );
		ddraw_dll = LoadLibrary( szPath );
		if( ddraw_dll != NULL){
			DIRECTDRAWCREATE pfnDirectDrawCreate = (DIRECTDRAWCREATE) GetProcAddress( ddraw_dll, "DirectDrawCreate" );
			if( pfnDirectDrawCreate != NULL ){
				if( SUCCEEDED( pfnDirectDrawCreate( (GUID*)lpGUID, &ddraw, pUnkOuter ) ) ){
					*lplpDD = (LPDIRECTDRAW) &IDDraw;
					return DD_OK;
				}
			}
			FreeLibrary( ddraw_dll );
		}
	}
	return DDERR_GENERIC;
}

HRESULT __stdcall dds_Lock( void* This, LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent ){
	HWND hwnd = GetActiveWindow();
	if( ( hwnd_main != hwnd ) && ( hwnd != NULL ) ){
		HDC hdc_dds;
		if( SUCCEEDED( dds_primary->GetDC( &hdc_dds ) ) ){
			EnumThreadWindows( GetCurrentThreadId(), gdi_to_ddraw, (LPARAM)hdc_dds );
			dds_primary->ReleaseDC( hdc_dds );
		}
	}
	return dds_primary->Lock( lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent );
}

HRESULT __stdcall dds_Unlock( void* This, LPVOID lpSurfMemPtr ){
	HRESULT hr = dds_primary->Unlock( lpSurfMemPtr );
	HWND hwnd = GetActiveWindow();
	if( ( hwnd_main != hwnd ) && ( hwnd != NULL ) ){
		HDC hdc_dds;
		if( SUCCEEDED( dds_primary->GetDC( &hdc_dds ) ) ){
			SetRectRgn( hrgn_acc, 0,0,0,0 );
			EnumThreadWindows( GetCurrentThreadId(), ddraw_to_gdi, (LPARAM)hdc_dds );
			dds_primary->ReleaseDC( hdc_dds );
		}
	}
	return hr;
}

HRESULT __stdcall dd_SetCooperativeLevel( void* This, HWND hWnd, DWORD dwFlags ){
	hwnd_main = hWnd;
	HRESULT hr = ddraw->SetCooperativeLevel( hWnd, dwFlags );
	return hr;
}

HRESULT __stdcall dd_CreatePalette( void* This, DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE* lplpDDPalette, IUnknown* pUnkOuter ){
	*lplpDDPalette = (LPDIRECTDRAWPALETTE) &IDDPal;
	return ddraw->CreatePalette( dwFlags, lpColorTable, &ddpal, pUnkOuter );
}

HRESULT __stdcall dd_CreateSurface( void* This, LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE* lplpDDSurface, IUnknown* pUnkOuter ){
	*lplpDDSurface = (LPDIRECTDRAWSURFACE) &IDDSurf;
	return ddraw->CreateSurface( lpDDSurfaceDesc, &dds_primary, pUnkOuter );
}

HRESULT __stdcall ddp_SetEntries( void* This, DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries ){
	return ddpal->SetEntries( dwFlags, dwStartingEntry, dwCount, lpEntries );
}

HRESULT __stdcall ddp_GetEntries( void* This, DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries ){
	return ddpal->GetEntries( dwFlags, dwBase, dwNumEntries, lpEntries );
}

HRESULT __stdcall dd_SetDisplayMode( void* This, DWORD dwWidth, DWORD dwHeight, DWORD dwBPP ){
	return ddraw->SetDisplayMode( dwWidth, dwHeight, dwBPP );
}

HRESULT __stdcall dds_SetPalette( void* This, LPDIRECTDRAWPALETTE lpDDPalette ){
	return dds_primary->SetPalette( ddpal );
}

HRESULT __stdcall dd_GetVerticalBlankStatus( void* This, BOOL *lpbIsInVB ){
	return ddraw->GetVerticalBlankStatus( lpbIsInVB );
}

HRESULT __stdcall dd_WaitForVerticalBlank( void* This, DWORD dwFlags, HANDLE hEvent){
	return ddraw->WaitForVerticalBlank( dwFlags, hEvent );
}

ULONG __stdcall iunknown_Release( void* This )
{
	if( This == IDDraw ) return ddraw->Release();
	if( This == IDDSurf ) return dds_primary->Release();
	if( This == IDDPal ) return ddpal->Release();
	return 1;
}

const DWORD dd_vtbl[] = {
	0, //QueryInterface,               // 0x00
	0, //AddRef,                       // 0x04
	(DWORD) iunknown_Release,          // 0x08
	0, //Compact,                      // 0x0C
	0, //CreateClipper,                // 0x10
	(DWORD) dd_CreatePalette,          // 0x14
	(DWORD) dd_CreateSurface,          // 0x18
	0, //DuplicateSurface,             // 0x1C
	0, //EnumDisplayModes,             // 0x20
	0, //EnumSurfaces,                 // 0x24
	0, //FlipToGDISurface,             // 0x28
	0, //GetCaps,                      // 0x2C
	0, //GetDisplayMode,               // 0x30
	0, //GetFourCCCodes,               // 0x34
	0, //GetGDISurface,                // 0x38
	0, //GetMonitorFrequency,          // 0x3C
	0, //GetScanLine,                  // 0x40
	(DWORD) dd_GetVerticalBlankStatus, // 0x44
	0, //Initialize,                   // 0x48
	0, //RestoreDisplayMode,           // 0x4C
	(DWORD) dd_SetCooperativeLevel,    // 0x50
	(DWORD) dd_SetDisplayMode,         // 0x54
	(DWORD) dd_WaitForVerticalBlank,   // 0x58
};

const DWORD dds_vtbl[] = {
	0, //QueryInterface,             // 0x00
	0, //AddRef,                     // 0x04
	(DWORD) iunknown_Release,        // 0x08
	0, //AddAttachedSurface,         // 0x0C
	0, //AddOverlayDirtyRect,        // 0x10
	0, //Blt,                        // 0x14
	0, //BltBatch,                   // 0x18
	0, //BltFast,                    // 0x1C
	0, //DeleteAttachedSurface,      // 0x20
	0, //EnumAttachedSurfaces,       // 0x24
	0, //EnumOverlayZOrders,         // 0x28
	0, //Flip,                       // 0x2C
	0, //GetAttachedSurface,         // 0x30
	0, //GetBltStatus,               // 0x34
	0, //GetCaps,                    // 0x38
	0, //GetClipper,                 // 0x3C
	0, //GetColorKey,                // 0x40
	0, //GetDC,                      // 0x44
	0, //GetFlipStatus,              // 0x48
	0, //GetOverlayPosition,         // 0x4C
	0, //GetPalette,                 // 0x50
	0, //GetPixelFormat,             // 0x54
	0, //GetSurfaceDesc,             // 0x58
	0, //Initialize,                 // 0x5C
	0, //IsLost,                     // 0x60
	(DWORD) dds_Lock,                // 0x64
	0, //ReleaseDC,                  // 0x68
	0, //Restore,                    // 0x6C
	0, //SetClipper,                 // 0x70
	0, //SetColorKey,                // 0x74
	0, //SetOverlayPosition,         // 0x78
	(DWORD) dds_SetPalette,          // 0x7C
	(DWORD) dds_Unlock,              // 0x80
	0, //UpdateOverlay,              // 0x84
	0, //UpdateOverlayDisplay,       // 0x88
	0  //UpdateOverlayZOrder,        // 0x8C
};

const DWORD ddp_vtbl[] = { 
	0, //QueryInterface,      // 0x00
	0, //AddRef,              // 0x04
	(DWORD) iunknown_Release, // 0x08
	0, //GetCaps,             // 0x0C
	(DWORD) ddp_GetEntries,   // 0x10
	0, //Initialize,          // 0x14
	(DWORD) ddp_SetEntries    // 0x18
};
