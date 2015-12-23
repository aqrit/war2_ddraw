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

void HookLateWndProcs(void);

// touchy
long __stdcall Main_Hookproc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if( uMsg == WM_ACTIVATEAPP ){  // gain/lost focus from/to another app
		if( IsWindowed == FALSE ){ // if fullscreen
			if( wParam == FALSE ){
				ShowWindow( hWnd, SW_MINIMIZE );
				ChangeDisplaySettings( NULL, 0 );
			}
			else{
				ShowWindow( hWnd, SW_RESTORE );
				SetResolution_640x480();
			}
		}
	}
	if( ( uMsg == WM_SYSKEYDOWN ) && (wParam == VK_RETURN) ){ // alt + enter 
		// note: why doesn't it work correctly with out SetWindowPos???
		if( IsWindowed == FALSE ){
			IsWindowed = TRUE;
			ChangeDisplaySettings( NULL, 0 );
			SetWindowPos( hwnd_main, HWND_TOPMOST, 0, 0, 640, 480, SWP_SHOWWINDOW );
		}
		else{
			IsWindowed = FALSE;
			SetResolution_640x480();
			SetWindowPos( hwnd_main, HWND_TOPMOST, 0, 0, 640, 480, SWP_SHOWWINDOW );		
		}
		return 0;
	}
	return CallWindowProc( Main_Original, hWnd, uMsg, wParam, lParam );
}

// track SDlgDialog windows, these are the windows that use gdi drawing
LRESULT __stdcall SDlgDialog_Hookproc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	WINDOWPOS* wndpos;
	CREATESTRUCT* cs;
	DWORD i;

	switch( msg )
	{
		case WM_CREATE:
		{
			cs = (CREATESTRUCT*)lParam;
			if( SDlgDialog_count < _countof( SDlgDialog_cache ) ){
				SDlgDialog_cache[SDlgDialog_count].hwnd = hwnd;
				SDlgDialog_cache[SDlgDialog_count].cy = cs->cy;
				SDlgDialog_cache[SDlgDialog_count].cx = cs->cx;
				SDlgDialog_cache[SDlgDialog_count].y = cs->y;
				SDlgDialog_cache[SDlgDialog_count].x = cs->x;
				SDlgDialog_count++;
			}
			break;
		}
		case WM_WINDOWPOSCHANGED: 
		{
			wndpos = (WINDOWPOS*) lParam;
			for( i = 0; i < SDlgDialog_count; i++ ){
				if( hwnd == SDlgDialog_cache[i].hwnd ){
					SDlgDialog_cache[i].cy = wndpos->cy;
					SDlgDialog_cache[i].cx = wndpos->cx;
					SDlgDialog_cache[i].y = wndpos->y;
					SDlgDialog_cache[i].x = wndpos->x;
					break;
				}
			}
			break;
		}
		case WM_DESTROY:
		{
			for( i = 0; i < SDlgDialog_count; i++ ){
				if( hwnd == SDlgDialog_cache[i].hwnd ){
					SDlgDialog_count--;
					if( i != SDlgDialog_count ){
						SDlgDialog_cache[i].hwnd = SDlgDialog_cache[SDlgDialog_count].hwnd;
						SDlgDialog_cache[i].cy = SDlgDialog_cache[SDlgDialog_count].cy;
						SDlgDialog_cache[i].cx = SDlgDialog_cache[SDlgDialog_count].cx;
						SDlgDialog_cache[i].y = SDlgDialog_cache[SDlgDialog_count].y;
						SDlgDialog_cache[i].x = SDlgDialog_cache[SDlgDialog_count].x;
					}
					break;
				}
			}
		
			// hack //
			// force redraw after leaving "player profile" screen
			RedrawWindow( NULL, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN );
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

	if( msg == WM_CREATE ){
		if( SDlgDialog_Original == NULL ) HookLateWndProcs();
	}

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


// global subclass
void HookLateWndProcs(void){
	HWND hwnd;
	RECT rc;
		
	hwnd = FindWindowEx( HWND_DESKTOP, NULL, "SDlgDialog", NULL );
	if( hwnd != NULL ){
		SDlgDialog_Original = (WNDPROC) SetClassLong( hwnd, GCL_WNDPROC, (DWORD)SDlgDialog_Hookproc );		
		do{
			SetWindowLong( hwnd, GWL_WNDPROC, (LONG)&SDlgDialog_Hookproc );
			if( GetWindowRect( hwnd, &rc ) ){
				if( SDlgDialog_count < _countof( SDlgDialog_cache ) ){
					SDlgDialog_cache[SDlgDialog_count].hwnd = hwnd;
					SDlgDialog_cache[SDlgDialog_count].cy = rc.bottom - rc.top;
					SDlgDialog_cache[SDlgDialog_count].cx = rc.right - rc.left;
					SDlgDialog_cache[SDlgDialog_count].y = rc.top;
					SDlgDialog_cache[SDlgDialog_count].x = rc.left;
					SDlgDialog_count++;
				}
			}
			hwnd = FindWindowEx( HWND_DESKTOP, hwnd, "SDlgDialog", NULL );
		} while( hwnd != NULL );

		hwnd = FindWindowEx( HWND_DESKTOP, NULL, "SDlgStatic", NULL );
		if( hwnd == NULL ){
			hwnd = CreateWindow( "SDlgStatic", 0, WS_POPUP, 0, 0, 1, 1, NULL, NULL, 0, NULL );
			if( hwnd != NULL ){
				SDlgStatic_Original = (WNDPROC) SetClassLong( hwnd, GCL_WNDPROC, (DWORD)SDlgStatic_Hookproc );
				DestroyWindow( hwnd );
			}
			// else error
		}
		else{ // not reached probably
			SDlgStatic_Original = (WNDPROC) SetClassLong( hwnd, GCL_WNDPROC, (DWORD)SDlgStatic_Hookproc );
			do{
				SetWindowLong( hwnd, GWL_WNDPROC, (LONG)&SDlgStatic_Hookproc );
				// todo: InvalidateRect( hwnd, NULL, TRUE ); // ???
				hwnd = FindWindowEx( HWND_DESKTOP, hwnd, "SDlgStatic", NULL );
			} while( hwnd != NULL );
		}
	}
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
