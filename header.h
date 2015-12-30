#include <windows.h>

struct SDLGDIALOG_CACHE_ENTRY {
	HWND  hwnd;
	DWORD cy; // height
	DWORD cx; // width
	DWORD y;
	DWORD x;
};

struct BITMAPINFO256 
{
	BITMAPINFOHEADER h;
	RGBQUAD palette[256];
};

// display.cpp
extern HWND hwnd_main;
extern BOOL IsWindowed;
extern WORD prtscn_toggle;
extern BYTE* dib_bits;
extern __declspec(align(128)) BYTE client_bits[640*480];
extern BITMAPINFO256 bmi;
//
HRESULT init( HWND hwnd ); 
void cleanup( void );
HRESULT lock( LONG* pitch, void** surf_bits );
void unlock( void* surface ); 
void set_palette( PALETTEENTRY* color_table_256 );
BOOL SetResolution_640x480(void);
void d3d_reset(void);

// image_helpers.cpp
void __cdecl color_convert( BYTE* src, RGBQUAD* pal, DWORD* dst, DWORD cnt );
void __stdcall multiblt( DWORD pitch, DWORD* d3d_bits );

// wndproc.cpp
void HookWndProcs( HWND hwnd );
extern DWORD SDlgDialog_count;
extern SDLGDIALOG_CACHE_ENTRY SDlgDialog_cache[16];
