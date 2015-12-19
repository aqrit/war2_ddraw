// sub-class the main window so we can listen for WM_ACTIVATEAPP messages

#include <windows.h>
#include "header.h"


WNDPROC Main_Original = NULL;
WNDPROC SDlgDialog_Original = NULL;
WNDPROC SDlgStatic_Original = NULL;
WNDPROC Button_Original = NULL;
WNDPROC Edit_Original = NULL;
WNDPROC Listbox_Original = NULL;
WNDPROC Combobox_Original = NULL;
// WNDPROC Scrollbar_Original = NULL;

void InstallLateHooks(void);

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
	}
	return CallWindowProc( Main_Original, hWnd, uMsg, wParam, lParam );
}

// todo: track SDlgDialog windows, these are the windows that use gdi drawing
LRESULT __stdcall SDlgDialog_Hookproc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	// hack // force redraw after leaving "player profile" screen
	if( msg == WM_DESTROY ) RedrawWindow( NULL, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN );
	
	return CallWindowProc( SDlgDialog_Original, hwnd, msg, wParam, lParam );
}

// disable anti-aliased fonts
LRESULT __stdcall Font_Hookproc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, WNDPROC WndProcA_Original )
{
	LOGFONTA lf;

	if( msg == WM_CREATE ){
		if( SDlgDialog_Original == NULL ) InstallLateHooks();
	}

	if( msg == WM_SETFONT ){
		if( wParam != NULL ){
			if( GetObject( (HFONT)wParam, sizeof(lf), &lf ) ){
				if( lf.lfQuality != NONANTIALIASED_QUALITY ){
					lf.lfQuality = NONANTIALIASED_QUALITY;
					wParam = (WPARAM) CreateFontIndirect( &lf ); // leak here
				}
			}
		}
	}
	return CallWindowProc( WndProcA_Original, hwnd, msg, wParam, lParam );
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

// subclass
void HookMainWindow( HWND hwnd ){
	Main_Original = (WNDPROC) SetWindowLong( hwnd, GWL_WNDPROC, (LONG)&Main_Hookproc );
}

// superclass 
void SuperClassSysWindows(void)
{
	HINSTANCE hInst;
	WNDCLASS wc;

	hInst = GetModuleHandle( NULL );

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

// global subclass
void InstallLateHooks(void)
{
	HWND hwnd;
		
	hwnd = FindWindowEx( HWND_DESKTOP, NULL, "SDlgDialog", NULL );
	if( hwnd != NULL ){
		SDlgDialog_Original = (WNDPROC) SetClassLong( hwnd, GCL_WNDPROC, (DWORD)SDlgDialog_Hookproc );		
		do{
			SetWindowLong( hwnd, GWL_WNDPROC, (LONG)&SDlgDialog_Hookproc );
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