// sub-class the main window so we can listen for WM_ACTIVATEAPP messages

#include <windows.h>
#include "header.h"

struct FONT_CACHE_ENTRY {
	LOGFONTA lf;
	HFONT hfont;
};

DWORD SDlgDialog_count = 0;
SDLGDIALOG_CACHE_ENTRY SDlgDialog_cache[16];

DWORD font_count = 0;
FONT_CACHE_ENTRY font_cache[24]; // to limit leaks


WNDPROC Main_Original = NULL;
WNDPROC SDlgDialog_Original = NULL;
WNDPROC SDlgStatic_Original = NULL;
WNDPROC Button_Original = NULL;
WNDPROC Edit_Original = NULL;
WNDPROC Listbox_Original = NULL;
WNDPROC Combobox_Original = NULL;
// WNDPROC Scrollbar_Original = NULL;

LRESULT __stdcall SDlgDialog_Hookproc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
LRESULT __stdcall SDlgStatic_Hookproc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );

#pragma intrinsic( memcmp )
long __stdcall Main_Hookproc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	HWND hwnd_temp;
	char class_name[12];
	RECT rc;
	WINDOWPOS* wndpos;
	WORD key_state;

	switch( uMsg )
	{
		case WM_ACTIVATEAPP: // gain/lost focus from/to another app
		{
			if( wParam == FALSE ){ // lost focus
				if( IsWindowed == FALSE ){ // if fullscreen
					ShowWindow( hWnd, SW_MINIMIZE );
					ChangeDisplaySettings( NULL, 0 );
				} else {
					return 0; // eat message if windowed
				}
			} else { // gained focus
				if( SDlgDialog_count != 0 ){
					key_state = GetKeyState( VK_SNAPSHOT );
					prtscn_toggle = ( key_state & 1 ) ^ ( key_state >> 15 );
				}
				if( IsWindowed == FALSE ){ // if fullscreen
					ShowWindow( hWnd, SW_RESTORE );
					SetResolution_640x480();
				}
			}
			break;
		}
		case WM_ACTIVATE:
		{ 
			if( SDlgDialog_Original == NULL ){ // set late hooks "global subclass"
				hwnd_temp = (HWND)lParam;
				if( GetClassName( hwnd_temp, class_name, 11 ) ){
					if( ! memcmp( class_name, "SDlgDialog", 11 ) ){
						SDlgDialog_Original = (WNDPROC) SetClassLong( hwnd_temp, GCL_WNDPROC, (DWORD)SDlgDialog_Hookproc );		
						SetWindowLong( hwnd_temp, GWL_WNDPROC, (LONG)&SDlgDialog_Hookproc );
						if( GetWindowRect( hwnd_temp, &rc ) ){
							SDlgDialog_cache[SDlgDialog_count].hwnd = hwnd_temp;
							SDlgDialog_cache[SDlgDialog_count].cy = rc.bottom - rc.top;
							SDlgDialog_cache[SDlgDialog_count].cx = rc.right - rc.left;
							SDlgDialog_cache[SDlgDialog_count].y = rc.top;
							SDlgDialog_cache[SDlgDialog_count].x = rc.left;
							key_state = GetKeyState( VK_SNAPSHOT );
							prtscn_toggle = ( key_state & 1 ) ^ ( key_state >> 15 );
							SDlgDialog_count = 1;
						}
						hwnd_temp = CreateWindow( "SDlgStatic", 0, WS_POPUP, 0, 0, 1, 1, NULL, NULL, 0, NULL );
						if( hwnd_temp != NULL ){
							SDlgStatic_Original = (WNDPROC) SetClassLong( hwnd_temp, GCL_WNDPROC, (DWORD)SDlgStatic_Hookproc );
							DestroyWindow( hwnd_temp );
						}
					}
				}
			}
			break;
		}		
		case WM_WINDOWPOSCHANGING: 
		{ // suppress whatever is changing the window size on resolution change
			wndpos = (WINDOWPOS*)lParam;
			if( wndpos->x || wndpos->y || wndpos->cx || wndpos->cy ){
				wndpos->x = 0;
				wndpos->y = 0;
				wndpos->cx = 640;
				wndpos->cy = 480;
				return 0;
			}
			break;
		}
		case WM_SYSKEYDOWN:
		{
			if( wParam == VK_RETURN ){ // alt + enter 
				if( IsWindowed == FALSE ){
					IsWindowed = TRUE;
					ChangeDisplaySettings( NULL, 0 );
				}
				else{
					IsWindowed = FALSE;
					SetResolution_640x480();
				}
				return 0;
			}
			break;
		}
	}

	return CallWindowProc( Main_Original, hWnd, uMsg, wParam, lParam );
}

// track SDlgDialog windows, these are the windows that use gdi drawing
LRESULT __stdcall SDlgDialog_Hookproc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	WINDOWPOS* wndpos;
	CREATESTRUCT* cs;
	DWORD i;
	WORD key_state;

	switch( msg )
	{
		case WM_WINDOWPOSCHANGED:
		{ 
			for( i = 0; i < SDlgDialog_count; i++ ){ 	 
				RedrawWindow( SDlgDialog_cache[i].hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN );
			}
			break;
		}
		case WM_CREATE:
		{		
			if( SDlgDialog_count < _countof( SDlgDialog_cache ) ){
				if( SDlgDialog_count == 0 ){
					key_state = GetKeyState( VK_SNAPSHOT );
					prtscn_toggle = ( key_state & 1 ) ^ ( key_state >> 15 );
				}
				cs = (CREATESTRUCT*)lParam;
				SDlgDialog_cache[SDlgDialog_count].hwnd = hwnd;
				SDlgDialog_cache[SDlgDialog_count].cy = cs->cy;
				SDlgDialog_cache[SDlgDialog_count].cx = cs->cx;
				SDlgDialog_cache[SDlgDialog_count].y = cs->y;
				SDlgDialog_cache[SDlgDialog_count].x = cs->x;
				SDlgDialog_count++;
			}
			break;
		}
		case WM_WINDOWPOSCHANGING: 
		{  // suppress whatever is changing the window size on resolution change
			// this may break something... 
			// for instance: the "banner" gets resized post creation...
			wndpos = (WINDOWPOS*) lParam;
			for( i = 0; i < SDlgDialog_count; i++ ){
				if( hwnd == SDlgDialog_cache[i].hwnd ){
					wndpos->cy = SDlgDialog_cache[i].cy;
					wndpos->cx = SDlgDialog_cache[i].cx;
					wndpos->y = SDlgDialog_cache[i].y;
					wndpos->x = SDlgDialog_cache[i].x;
					return 0;
				}
			}
			break;
		}
		case WM_DESTROY:
		{
			SDlgDialog_count--;
			for( i = SDlgDialog_count; hwnd != SDlgDialog_cache[i].hwnd; i-- ){ ; } // search list
			for( ; i < SDlgDialog_count; i++ ){ // remove entry, compact list (retain order)
				SDlgDialog_cache[i].hwnd = SDlgDialog_cache[i+1].hwnd;
				SDlgDialog_cache[i].cy   = SDlgDialog_cache[i+1].cy;
				SDlgDialog_cache[i].cx   = SDlgDialog_cache[i+1].cx;
				SDlgDialog_cache[i].y    = SDlgDialog_cache[i+1].y;
				SDlgDialog_cache[i].x    = SDlgDialog_cache[i+1].x;
			}
			break;
		}
	}
	return SDlgDialog_Original( hwnd, msg, wParam, lParam );
}

// disable anti-aliased fonts
#pragma intrinsic( memcmp, memcpy )
LRESULT __stdcall Font_Hookproc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, WNDPROC WndProcA_Original )
{
	LOGFONTA lf;
	DWORD i;

	if( msg == WM_SETFONT ){
		if( wParam != NULL ){
			if( GetObject( (HFONT)wParam, sizeof(lf), &lf ) ){
				if( lf.lfQuality != NONANTIALIASED_QUALITY ){
					for( i = 0; i < font_count; i++ ){
						if( ! memcmp( &font_cache[i].lf, &lf, sizeof(lf) ) ){
							wParam = (WPARAM) font_cache[i].hfont;
							return WndProcA_Original( hwnd, msg, wParam, lParam );
						}
					}
					if( font_count < _countof( font_cache ) ){
						memcpy( &font_cache[font_count].lf, &lf, sizeof(lf) );
						lf.lfQuality = NONANTIALIASED_QUALITY;
						wParam = (WPARAM) CreateFontIndirect( &lf ); 
						if( wParam != NULL ){
							font_cache[font_count].hfont = (HFONT)wParam;
							font_count++;
						}
					}
				}
			}
		}
	}
	return WndProcA_Original( hwnd, msg, wParam, lParam );
}

LRESULT __stdcall SDlgStatic_Hookproc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ){
	return Font_Hookproc( hwnd, msg, wParam, lParam, SDlgStatic_Original );
}

LRESULT __stdcall Button_Hookproc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ){
	return Font_Hookproc( hwnd, msg, wParam, lParam, Button_Original );
}

LRESULT __stdcall Edit_Hookproc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ){
	return Font_Hookproc( hwnd, msg, wParam, lParam, Edit_Original );
}

LRESULT __stdcall Listbox_Hookproc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ){
	return Font_Hookproc( hwnd, msg, wParam, lParam, Listbox_Original );
}

LRESULT __stdcall Combobox_Hookproc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ){
	return Font_Hookproc( hwnd, msg, wParam, lParam, Combobox_Original );
}


void HookWndProcs( HWND hwnd ){
	HINSTANCE hInst;
	WNDCLASS wc;

	hInst = GetModuleHandle( NULL );

	// subclass
	Main_Original = (WNDPROC) SetWindowLong( hwnd, GWL_WNDPROC, (LONG)&Main_Hookproc );

	// superclass 
	GetClassInfo( NULL, "Button", &wc );
	wc.hInstance = hInst;
	Button_Original = wc.lpfnWndProc;
	wc.lpfnWndProc = Button_Hookproc;
	RegisterClass( &wc );

	GetClassInfo( NULL, "Edit", &wc );
	wc.hInstance = hInst;
	Edit_Original = wc.lpfnWndProc;
	wc.lpfnWndProc = Edit_Hookproc;
	RegisterClass( &wc );

	GetClassInfo( NULL, "Listbox", &wc );
	wc.hInstance = hInst;
	Listbox_Original = wc.lpfnWndProc;
	wc.lpfnWndProc = Listbox_Hookproc;
	RegisterClass( &wc );

	GetClassInfo( NULL, "Combobox", &wc );
	wc.hInstance = hInst;
	Combobox_Original = wc.lpfnWndProc;
	wc.lpfnWndProc = Combobox_Hookproc;
	RegisterClass( &wc );
}
