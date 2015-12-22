#include <windows.h>

struct SDLGDIALOG_CACHE_ENTRY {
	HWND  hwnd;
	DWORD cy; // height
	DWORD cx; // width
	DWORD y;
	DWORD x;
};

// display.cpp
extern HWND hwnd_main;
extern BOOL IsWindowed;
//
void init( HWND hwnd ); 
void cleanup( void );
HRESULT lock( LONG* pitch, void** surf_bits );
void unlock( void* surface ); 
void set_palette( PALETTEENTRY* color_table_256 );
BOOL SetResolution_640x480(void);
void d3d_reset(void);

// color_convert.cpp
void color_convert( BYTE* src, RGBQUAD* pal, DWORD* dst, DWORD cnt );

// wndproc.cpp
void HookWndProcs( HWND hwnd );
extern DWORD SDlgDialog_count;
extern SDLGDIALOG_CACHE_ENTRY SDlgDialog_cache[16];
