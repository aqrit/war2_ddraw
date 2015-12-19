// sub-class the main window so we can listen for WM_ACTIVATEAPP messages

#include <windows.h>
#include "header.h"


WNDPROC FocusWndProc_Original = NULL;

// touchy
long __stdcall FocusWndProc_Hook( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
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
	return CallWindowProc( FocusWndProc_Original, hWnd, uMsg, wParam, lParam );
}

// sub-class
void HookMainWindow( HWND hwnd )
{
	FocusWndProc_Original = (WNDPROC) SetWindowLong( hwnd, GWL_WNDPROC, (LONG)&FocusWndProc_Hook );
}