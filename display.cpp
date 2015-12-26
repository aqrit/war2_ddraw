#include <windows.h>
#include <d3d9.h>
#include "header.h"

BOOL import_gdi_bits = FALSE;

HWND hwnd_main = NULL;

BOOL IsWindowed = FALSE;

LPDIRECT3D9 d3d = NULL;
LPDIRECT3DDEVICE9 device = NULL;
LPDIRECT3DSURFACE9 secondary = NULL;

 __declspec(align(128)) BYTE client_bits[640*480];
BYTE* dib_bits;

HDC hdc_offscreen;
HBITMAP hbmp_old;
COLORREF clear_color;

D3DPRESENT_PARAMETERS d3dpp = {
	d3dpp.BackBufferWidth = 640,   
	d3dpp.BackBufferHeight = 480,  
	d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8,
	d3dpp.BackBufferCount = 1,
	d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE,
	d3dpp.MultiSampleQuality = 0,
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD,
	d3dpp.hDeviceWindow = 0,
	d3dpp.Windowed = TRUE,   
	d3dpp.EnableAutoDepthStencil = FALSE,
	d3dpp.AutoDepthStencilFormat = (D3DFORMAT)0,
	d3dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER,
	d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT,
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE // D3DPRESENT_INTERVAL_ONE for vsync?
};

BITMAPINFO256 bmi = {
	bmi.h.biSize = sizeof( BITMAPINFOHEADER ), 
	bmi.h.biWidth = 640,
	bmi.h.biHeight = 0 - 480,  // origin top-left 
	bmi.h.biPlanes = 1,
	bmi.h.biBitCount = 8,
	bmi.h.biCompression = BI_RGB,
	bmi.h.biSizeImage = 0,
	bmi.h.biXPelsPerMeter = 0,
	bmi.h.biYPelsPerMeter = 0,
	bmi.h.biClrUsed = 0,
	bmi.h.biClrImportant = 0 
};

void d3d_blit(void);

////////////////////////////////////////////////////////////////////////////////


HRESULT lock( LONG* pitch, void** surf_bits )
{
	SDLGDIALOG_CACHE_ENTRY* ce;
	HDC hdc;
	DWORD i;

	*pitch = 640;
	*surf_bits = dib_bits;

	if( ( SDlgDialog_count != 0 ) && ( import_gdi_bits != FALSE ) ){
		for( i = 0; i < SDlgDialog_count; i++ ){ 
			ce = &SDlgDialog_cache[i];
			hdc = GetDC( ce->hwnd );
			BitBlt( hdc_offscreen, ce->x, ce->y, ce->cx, ce->cy, hdc, 0, 0, SRCCOPY );
			ReleaseDC( ce->hwnd, hdc );
		}
	}

//	if( SDlgDialog_count != 0 ){
	//	if( 0x8001 & GetAsyncKeyState( VK_SNAPSHOT )){ // if a screenshot is wanted
// ..... } } 
		
	return 0;
}


void unlock( void* surface ){
	HDC hdc;
	DWORD i;
	SDLGDIALOG_CACHE_ENTRY* ce;
	D3DLOCKED_RECT rc;
	HRESULT hr;

	if( secondary != NULL ){
		hr = secondary->LockRect( &rc, NULL, 0 );
		if( SUCCEEDED( hr ) ){
			if( SDlgDialog_count != 0 ){
				if( import_gdi_bits != FALSE ){
					for( i = 0; i < SDlgDialog_count; i++ ){
						ce = &SDlgDialog_cache[i];
						hdc = GetDCEx( ce->hwnd, NULL, DCX_PARENTCLIP | DCX_CACHE );
						BitBlt( hdc, 0, 0, ce->cx, ce->cy, hdc_offscreen, ce->x, ce->y, SRCCOPY );
						ReleaseDC( ce->hwnd, hdc );
					}
					// color convert
					for( int i = 0; i < 480; i++ ){ // for each scanline
						color_convert( &(((BYTE*)dib_bits)[i*640]), bmi.palette, (DWORD*)(((BYTE*)rc.pBits) + (rc.Pitch * i)), 640/4 );
					}

				} else {
					for( i = 0; i < SDlgDialog_count; i++ ){
						ce = &SDlgDialog_cache[i];
						hdc = GetDCEx( ce->hwnd, NULL, DCX_PARENTCLIP | DCX_CACHE );
						GdiTransparentBlt( hdc, 0, 0, ce->cx, ce->cy, 
							hdc_offscreen, ce->x, ce->y, ce->cx, ce->cy, clear_color );
						ReleaseDC( ce->hwnd, hdc );
					}
					multiblt( rc.Pitch, (DWORD*) rc.pBits );
				} 
			} else {
				for( int i = 0; i < 480; i++ ){ // for each scanline
					color_convert( &dib_bits[i*640], bmi.palette, (DWORD*)(((BYTE*)rc.pBits) + (rc.Pitch * i)), 640/4 );
				}
			}
			secondary->UnlockRect();
			hr = device->Present(NULL, NULL, NULL, NULL);
		}
		if( FAILED( hr ) ) return d3d_reset();
	}
}

void cleanup( void )
{
	; // todo
}

#pragma intrinsic( _byteswap_ulong )
void set_palette( PALETTEENTRY* colors ){
	DWORD i;

	for( i = 0; i < 256; i++ )
	{ // convert 0xFFBBGGRR to 0x00RRGGBB ( X8R8G8B8 )
		// speed is unknown		
		*((DWORD*)&bmi.palette[i]) = _byteswap_ulong( *(DWORD*)&colors[i] ) >> 8;
	}

	// probably best to keep this in sync even if not in mixed_mode
	clear_color = *((DWORD*)&colors[254]) & 0x00FFFFFF;
	SetDIBColorTable( hdc_offscreen, 0, 256, bmi.palette );

	// animate palette
	unlock( client_bits );

	// HACK // 
	// not drawing main menu after movie? 
	// this still isn't fix 100% ...
	InvalidateRect( hwnd_main, NULL, TRUE );
}


void init( HWND hwnd )
{ 
	HBITMAP hbmp;
	HRESULT hr;

	hwnd_main = hwnd;
	d3dpp.hDeviceWindow = hwnd;

	if( ! IsProcessorFeaturePresent( PF_XMMI64_INSTRUCTIONS_AVAILABLE ) ){
		import_gdi_bits = TRUE; // sse2 not supported use only gdi instead
	}

	// create dib_section ( offscreen gdi drawing surface )
	hbmp = CreateDIBSection( NULL, (BITMAPINFO*) &bmi, DIB_RGB_COLORS, (void**) &dib_bits, NULL, 0 );
	hdc_offscreen = CreateCompatibleDC( NULL );
	hbmp_old = (HBITMAP) SelectObject( hdc_offscreen, hbmp );
	if( ( hbmp == NULL ) || ( hbmp_old == NULL ) ) goto fail;

	// 
	HookWndProcs( hwnd );

	// needed on bnet screens 
	// because the sibling windows are not scaled to new resolutions. 
	SetResolution_640x480();

	// the window size is originally the size of the desktop...
	SetWindowPos( hwnd, HWND_TOP, 0, 0, 640, 480, SWP_NOOWNERZORDER | SWP_NOZORDER );

	// init d3d
	// must not be exclusive, to keep gdi drawing visible 
	d3d = Direct3DCreate9( D3D_SDK_VERSION );
	if( d3d == NULL ) goto fail;
	hr = d3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &device );
	if( FAILED ( hr ) ) goto fail;	
	hr = device->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &secondary );
	if( FAILED ( hr ) ) goto fail;

	// success
	return;

fail:
	MessageBox( hwnd, "Display Failure!", "aqrit's compat mod", MB_OK );
	DebugBreak(); // crash and burn
}


BOOL SetResolution_640x480(void)
{
	DEVMODE devMode;

	RtlSecureZeroMemory(&devMode, sizeof(devMode));
	devMode.dmSize = sizeof(devMode);
	devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT; 
	devMode.dmPelsWidth  = 640;
	devMode.dmPelsHeight = 480;
	if( DISP_CHANGE_SUCCESSFUL == ChangeDisplaySettings(&devMode, CDS_FULLSCREEN) )
	{
		return TRUE;
	}
	return FALSE;
}

void d3d_reset(void)
{
	if( secondary != NULL )
	{
		secondary->Release();
		secondary = NULL;
	}
	if( SUCCEEDED( device->Reset( &d3dpp ) ) )
	{
		if( SUCCEEDED(  device->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &secondary ) ) )
		{
			; //
		}
	}
	else Sleep( 100 );
}

