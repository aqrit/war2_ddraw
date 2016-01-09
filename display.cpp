#include <windows.h>
#include <d3d9.h>
#include "header.h"
#include "PaletteShader.h"

HWND hwnd_main = NULL;

BOOL IsWindowed = FALSE;

BOOL import_gdi_bits = FALSE;
WORD prtscn_toggle;

LPDIRECT3D9 d3d = NULL;
LPDIRECT3DDEVICE9 d3ddev = NULL;
LPDIRECT3DVERTEXBUFFER9 d3dvb = NULL;
IDirect3DTexture9* d3dtex = NULL;
IDirect3DTexture9* d3dpal = NULL;
IDirect3DPixelShader9* shader = NULL;

__declspec(align(128)) BYTE client_bits[640*480];
BYTE* dib_bits;

HDC hdc_offscreen;
HBITMAP hbmp_old;
COLORREF clear_color;

// double buffered (discard) with no vsync seems best,
// as there doesn't appear to be a way to drop frames while triple buffering w/vsync,
// ...which might be needed???, 
// ...since we don't want to alter how many fps the game can draw
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
	d3dpp.AutoDepthStencilFormat = D3DFMT_UNKNOWN,
	d3dpp.Flags = 0,
	d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT,
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE
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


#define CUSTOMFVF (D3DFVF_XYZRHW | D3DFVF_TEX1)
struct CUSTOMVERTEX {float x, y, z, rhw, u, v;};
CUSTOMVERTEX vertices[] =
{
	{   0.0f - 0.5f, 480.0f - 0.5f, 0.0f, 1.0f, 0.0f,             480.0f / 1024.0f },
	{   0.0f - 0.5f,   0.0f - 0.5f, 0.0f, 1.0f, 0.0f,             0.0f             },
	{ 640.0f - 0.5f, 480.0f - 0.5f, 0.0f, 1.0f, 640.0f / 1024.0f, 480.0f / 1024.0f },
	{ 640.0f - 0.5f,   0.0f - 0.5f, 0.0f, 1.0f, 640.0f / 1024.0f, 0.0f             }
};

////////////////////////////////////////////////////////////////////////////////
HRESULT lock( LONG* pitch, void** surf_bits )
{
	SDLGDIALOG_CACHE_ENTRY* ce;
	HDC hdc;
	DWORD i;
	WORD key_state;
	HRESULT hr;
	D3DLOCKED_RECT lock_rc;
	RECT rc = { 0,0,640,480 };

	if( SDlgDialog_count == 0 )
	{
		hr = d3dtex->LockRect( 0, &lock_rc, &rc, 0 );
		*pitch = lock_rc.Pitch;
		*surf_bits = lock_rc.pBits;
		return hr;
	}

	*pitch = 640;
	*surf_bits = dib_bits;

	if( import_gdi_bits == FALSE ){
		key_state = GetKeyState( VK_SNAPSHOT );
		// if_not (current_toggle_state ^ is_key_down ^ pev_toggle_state) ret;
		if( !( ( key_state & 1 ) ^ ( key_state >> 15 ) ^ prtscn_toggle ) ) return 0;
		// else update prev state then fall thru to import gdi bits for screenshot
		prtscn_toggle ^= 1;
	}

	// assumes SDlgDialog_cache is sorted by z-order...(it is sorted by age not z-order)
	// color converts from 32bpp to 8bpp... ( slow, is gdi trust worthy for this? )
	for( i = 0; i < SDlgDialog_count; i++ ){ 
		ce = &SDlgDialog_cache[i];
		hdc = GetDC( ce->hwnd );
		BitBlt( hdc_offscreen, ce->x, ce->y, ce->cx, ce->cy, hdc, 0, 0, SRCCOPY );
		ReleaseDC( ce->hwnd, hdc );
	}
	GdiFlush();
	return 0;
}


void unlock( void* surface ){
	HDC hdc;
	DWORD i;
	SDLGDIALOG_CACHE_ENTRY* ce;
	D3DLOCKED_RECT lock_rc;
	RECT rc = { 0,0,640,480 };

	if( surface != dib_bits ){
		d3dtex->UnlockRect(0);
		d3ddev->BeginScene();
		d3ddev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
	    d3ddev->EndScene();
		if( FAILED( d3ddev->Present(NULL, NULL, NULL, NULL) ) ) d3d_reset();
		return;
	}

	if( SUCCEEDED( d3dtex->LockRect( 0, &lock_rc, &rc, 0 ) ) ){
		if( import_gdi_bits != FALSE ){
			for( i = 0; i < SDlgDialog_count; i++ ){
				ce = &SDlgDialog_cache[i];
				hdc = GetDCEx( ce->hwnd, NULL, DCX_PARENTCLIP | DCX_CACHE );
				BitBlt( hdc, 0, 0, ce->cx, ce->cy, hdc_offscreen, ce->x, ce->y, SRCCOPY );
				ReleaseDC( ce->hwnd, hdc );
			}
			GdiFlush();
			for( int i = 0; i < 480; i++ ){ // for each scanline
				color_convert( &(((BYTE*)dib_bits)[i*640]), bmi.palette, (DWORD*)(((BYTE*)lock_rc.pBits) + (lock_rc.Pitch * i)), 640/4 );
			}
		} else {
			for( i = 0; i < SDlgDialog_count; i++ ){
				ce = &SDlgDialog_cache[i];
				hdc = GetDCEx( ce->hwnd, NULL, DCX_PARENTCLIP | DCX_CACHE );
				GdiTransparentBlt( hdc, 0, 0, ce->cx, ce->cy, 
					hdc_offscreen, ce->x, ce->y, ce->cx, ce->cy, clear_color );
				ReleaseDC( ce->hwnd, hdc );
			}
			GdiFlush();
			multiblt( lock_rc.Pitch, (DWORD*) lock_rc.pBits );
		}
		return unlock( lock_rc.pBits );
	}
}

void cleanup( void )
{
	; // todo
}

#pragma intrinsic( _byteswap_ulong, memcpy )
void set_palette( PALETTEENTRY* colors ){
	DWORD i;
	D3DLOCKED_RECT lock_rc;
	RECT rc = { 0,0,256,1 };

	// not to speed critical here...
	// so don't diff between "in-game" and "bnet menu" modes

	for( i = 0; i < 256; i++ )
	{ // convert 0xFFBBGGRR to 0x00RRGGBB ( X8R8G8B8 )
		// speed is unknown		
		*((DWORD*)&bmi.palette[i]) = _byteswap_ulong( *(DWORD*)&colors[i] ) >> 8;
	}

	if( SUCCEEDED( d3dpal->LockRect( 0, &lock_rc, &rc, 0 ) ) ){	
		memcpy( lock_rc.pBits, bmi.palette, 4 * 256 );
		d3dpal->UnlockRect(0);
	}
	
	clear_color = *((DWORD*)&colors[254]) & 0x00FFFFFF;
	SetDIBColorTable( hdc_offscreen, 0, 256, bmi.palette );

	// animate palette
	unlock(0);
}

#pragma intrinsic( memcpy )
HRESULT init( HWND hwnd )
{ 
	HBITMAP hbmp;
	HRESULT hr;
	void* vb;

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
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if(!d3d) goto fail;

	hr = d3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, 
		D3DCREATE_NOWINDOWCHANGES | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE,
		&d3dpp, &d3ddev );
	if( FAILED ( hr ) ) goto fail;	

	hr = d3ddev->SetFVF(CUSTOMFVF);
	if( FAILED ( hr ) ) goto fail;

	hr = d3ddev->CreateVertexBuffer( sizeof(vertices), 0, CUSTOMFVF, D3DPOOL_MANAGED, &d3dvb, NULL );
	if( FAILED ( hr ) ) goto fail;	

    hr = d3dvb->Lock(0, 0, (void**)&vb, 0);
	if( FAILED ( hr ) ) goto fail;

    memcpy(vb, vertices, sizeof(vertices));
    
	hr = d3dvb->Unlock();
	if( FAILED ( hr ) ) goto fail;

	hr = d3ddev->SetStreamSource(0, d3dvb, 0, sizeof(CUSTOMVERTEX));
	if( FAILED ( hr ) ) goto fail;

	// todo: NPOT if supported
	hr = d3ddev->CreateTexture( 1024, 1024, 1, 0, D3DFMT_L8, D3DPOOL_MANAGED, &d3dtex, 0 );
	if( FAILED ( hr ) ) goto fail;

	hr = d3ddev->SetTexture( 0, d3dtex );
	if( FAILED ( hr ) ) goto fail;

	// todo: NPOT if supported
	hr = d3ddev->CreateTexture( 256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &d3dpal, 0 );
	if( FAILED ( hr ) ) goto fail;

	hr = d3ddev->SetTexture( 1, d3dpal );
	if( FAILED ( hr ) ) goto fail;

	hr = d3ddev->CreatePixelShader((DWORD*)g_ps20_main, &shader);
	if( FAILED ( hr ) ) goto fail;

	hr = d3ddev->SetPixelShader(shader);
	if( FAILED ( hr ) ) goto fail;

	// ...?
    d3ddev->SetRenderState(D3DRS_LIGHTING, FALSE);
    d3ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	d3ddev->SetRenderState(D3DRS_COLORVERTEX, FALSE);
	d3ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    d3ddev->SetRenderState(D3DRS_ZENABLE, FALSE);
	// ...?
	d3ddev->SetSamplerState(0,D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	d3ddev->SetSamplerState(0,D3DSAMP_MINFILTER, D3DTEXF_POINT);
	d3ddev->SetSamplerState(0,D3DSAMP_MIPFILTER, D3DTEXF_NONE);

	// success
	return S_OK;

fail:
	MessageBox( hwnd, "Display Failure!", "aqrit's compat mod", MB_OK );
	return E_FAIL;
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

#pragma intrinsic( memcpy )
void d3d_reset(void)
{
	void* vb;

	if( SUCCEEDED( d3ddev->Reset( &d3dpp ) ) )
	{
		d3ddev->SetFVF(CUSTOMFVF);
		d3ddev->SetStreamSource(0, d3dvb, 0, sizeof(CUSTOMVERTEX));
		d3ddev->SetTexture( 0, d3dtex );
		d3ddev->SetTexture( 1, d3dpal );
		d3ddev->SetPixelShader(shader);
	
		// ...?
		d3ddev->SetRenderState(D3DRS_LIGHTING, FALSE);
		d3ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		d3ddev->SetRenderState(D3DRS_COLORVERTEX, FALSE);
		d3ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		d3ddev->SetRenderState(D3DRS_ZENABLE, FALSE);
		// ...?
		d3ddev->SetSamplerState(0,D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		d3ddev->SetSamplerState(0,D3DSAMP_MINFILTER, D3DTEXF_POINT);
		d3ddev->SetSamplerState(0,D3DSAMP_MIPFILTER, D3DTEXF_NONE);

		if( SUCCEEDED( d3dvb->Lock(0, 0, (void**)&vb, 0) ) ) {
			memcpy(vb, vertices, sizeof(vertices));
			d3dvb->Unlock();
		}

	}
	else Sleep( 100 );
}

