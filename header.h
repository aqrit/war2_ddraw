#include <windows.h>

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
void HookMainWindow( HWND hwnd );

// fonts.cpp
void HookFonts(void);