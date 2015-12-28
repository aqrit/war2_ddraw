//
// Implements just enough ddraw to talk to the game
//

#include <windows.h>
#include <ddraw.h> 
#include "header.h"

#pragma comment(linker, "/export:DirectDrawCreate=_DirectDrawCreate@12")
#pragma warning( disable : 4100 ) // silence UNREFERENCED_PARAMETER warnings

extern const DWORD dd_vtbl[];
extern const DWORD dds_vtbl[];
extern const DWORD ddp_vtbl[];
const DWORD* const IDDraw = dd_vtbl;
const DWORD* const IDDSurf = dds_vtbl;
const DWORD* const IDDPal = ddp_vtbl;

HRESULT __stdcall dd_SetCooperativeLevel( void* This, HWND hWnd, DWORD dwFlags )
{ // "describe how DirectDraw interacts with the display", exclusive or normal
	return init( hWnd );
}

HRESULT __stdcall dds_Lock( void* This, LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent )
{ // obtains a pointer to the surface memory
	lock( &lpDDSurfaceDesc->lPitch, &lpDDSurfaceDesc->lpSurface );
	return 0;
}

HRESULT __stdcall dds_Unlock( void* This, LPVOID lpSurfMemPtr )
{ // notifies that direct surface manipulations are complete
	unlock( lpSurfMemPtr );
	return 0;
}

HRESULT __stdcall ddp_SetEntries( void* This, DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries )
{ // set palette colors
	set_palette( lpEntries );
	return 0;
}

HRESULT __stdcall dd_CreatePalette( void* This, DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE* lplpDDPalette, IUnknown* pUnkOuter )
{
	*lplpDDPalette = (LPDIRECTDRAWPALETTE) &IDDPal;
	set_palette( lpColorTable );
	return 0;
}

HRESULT __stdcall DirectDrawCreate( GUID* lpGUID, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter )
{
	*lplpDD = (LPDIRECTDRAW) &IDDraw;
	return 0;
}

HRESULT __stdcall dd_SetDisplayMode( void* This, DWORD dwWidth, DWORD dwHeight, DWORD dwBPP )
{
	return 0; 
}

HRESULT __stdcall dd_CreateSurface( void* This, LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE* lplpDDSurface, IUnknown* pUnkOuter )
{	
	*lplpDDSurface = (LPDIRECTDRAWSURFACE) &IDDSurf;
	return 0;
}

HRESULT __stdcall dds_SetPalette( void* This, LPDIRECTDRAWPALETTE lpDDPalette )
{ // associate a palette with a surface, don't care because there are only one of each.
	return 0;
}

HRESULT __stdcall ddp_GetEntries( void* This, DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries )
{ // seems okay to ignore... the screen is no longer in 8-bit mode, so gdi doesn't care.
	return 0; 
}

HRESULT __stdcall dd_GetVerticalBlankStatus( void* This, BOOL *lpbIsInVB )
{ // used for vsync while playing movies, not needed.
	return DDERR_UNSUPPORTED;
}

HRESULT __stdcall dd_WaitForVerticalBlank( void* This, DWORD dwFlags, HANDLE hEvent)
{ // used for vsync while playing movies, not needed.
	return DDERR_UNSUPPORTED;
}

ULONG __stdcall iunknown_Release( void* This )
{
	if( This == IDDraw ) cleanup();
	return 0; 
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
