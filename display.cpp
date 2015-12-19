#include <windows.h>
#include <d3d9.h>
#include "header.h"

BOOL sse2_supported = FALSE;

HWND hwnd_main = NULL;

BOOL IsWindowed = FALSE;

LPDIRECT3D9 d3d = NULL;
LPDIRECT3DDEVICE9 device = NULL;
LPDIRECT3DSURFACE9 secondary = NULL;

void* dib_bits;
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

struct BITMAPINFO256 
{
	BITMAPINFOHEADER h;
	RGBQUAD palette[256];
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

void ToScreen()
{
	HWND hwnd;
	RECT rc;
	HDC hdc;
	int i;
	DWORD* p;

	hwnd = FindWindowEx( HWND_DESKTOP, NULL, "SDlgDialog", NULL ); // detect mixed gdi/ddraw screen
	if( hwnd == NULL ) return d3d_blit(); // ddraw only

	// blast it out to all top-level SDlgDialog windows... the real wtf
	do
	{ 	
		GetWindowRect( hwnd, &rc );
		hdc = GetDCEx( hwnd, NULL, DCX_PARENTCLIP | DCX_CACHE );
		GdiTransparentBlt( hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
				hdc_offscreen, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
				clear_color 
			);
		ReleaseDC( hwnd, hdc );
		hwnd = FindWindowEx( HWND_DESKTOP, hwnd, "SDlgDialog", NULL );
	} while( hwnd != NULL );

	// erase
	if( sse2_supported )
	{
		__asm{
			mov eax, dib_bits
			pcmpeqw xmm0,xmm0 
			psllw xmm0,1 
			packsswb xmm0,xmm0 
			mov ecx, 640*480/128
		lbl_loop:
			movntdq 0[eax], xmm0
			movntdq 16[eax], xmm0
			movntdq 32[eax], xmm0
			movntdq 48[eax], xmm0
			movntdq 64[eax], xmm0
			movntdq 80[eax], xmm0
			movntdq 96[eax], xmm0
			movntdq 112[eax], xmm0
			add eax,128
			dec ecx
			jnz lbl_loop
		}
	}
	else{ // sse2 not supported
		p = (DWORD*) dib_bits;
		for( i = 0; i < 640 * 480 / 4; i++ ) p [ i ] = 0xFEFEFEFE;
	}
}


HRESULT lock( LONG* pitch, void** surf_bits ){
	*surf_bits = dib_bits;
	*pitch = 640;
	return 0;
}

void unlock( void* surface ){
	ToScreen();
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

	ToScreen(); // animate palette

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

	sse2_supported = IsProcessorFeaturePresent( PF_XMMI64_INSTRUCTIONS_AVAILABLE );


	// disable antiAliased fonts
	HookFonts();

	// create dib_section ( offscreen gdi drawing surface )
	hbmp = CreateDIBSection( NULL, (BITMAPINFO*) &bmi, DIB_RGB_COLORS, &dib_bits, NULL, 0 );
	hdc_offscreen = CreateCompatibleDC( NULL );
	hbmp_old = (HBITMAP) SelectObject( hdc_offscreen, hbmp );
	if( ( hbmp == NULL ) || ( hbmp_old == NULL ) ) goto fail;

	// sub-class main window to help detect focus loss
	HookMainWindow( hwnd );

	// needed on bnet screens 
	// because the sibling windows are not scaled to new resolutions. 
	if( SetResolution_640x480() == FALSE ) goto fail;

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
			if( d3dpp.BackBufferHeight != 480 ) __asm int 3 //
			if( d3dpp.BackBufferWidth != 640 ) __asm int 3 //

		}
	}
	else Sleep( 100 );
}

void d3d_blit(void)
{
	D3DLOCKED_RECT rc;
	HRESULT hr;

	if( secondary != NULL )
	{
		hr = secondary->LockRect( &rc, NULL, 0 );
		if( SUCCEEDED( hr ) )
		{
			BYTE* p = (BYTE*)rc.pBits;
			for( int y = 0; y < 480; y++ )
			{
				color_convert( &(((BYTE*)dib_bits)[y*640]), bmi.palette, (DWORD*)((BYTE*)p + rc.Pitch * y ), 640/4 );
			}
			secondary->UnlockRect();
		    
			hr = device->Present(NULL, NULL, NULL, NULL);
			if( SUCCEEDED( hr ) ) return;
		}
	}
	// failure of some kind
	d3d_reset();
}