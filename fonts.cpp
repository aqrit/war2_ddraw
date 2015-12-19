// Disable AntiAliased Fonts via hot-patch of gdi's CreateFont/Indirect

#include <windows.h>

typedef HFONT (__stdcall* CREATEFONTINDIRECTA)( CONST LOGFONT* );
typedef HFONT (__stdcall* CREATEFONTA)( int,int,int,int,int,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCTSTR ); 

CREATEFONTINDIRECTA CreateFontIndirectA_origproc;
CREATEFONTA CreateFontA_origproc;

// hook a system dll on Windows XP SP2 or later
// returns the address of the original_proc on success, null on failure
#pragma intrinsic( memcmp )
void* HotPatch( void* target, void* hookproc )
{
	DWORD dwPrevProtect;
	char* patch_address;
	void* original_proc;

	original_proc = NULL;

	patch_address = ((char*)target) - 5;

	// entry point could be at the top of a page if function is not hot-patch-able
	// so VirtualProtect first to make sure patch_address is readable
	if( VirtualProtect( patch_address, 7, PAGE_EXECUTE_WRITECOPY, &dwPrevProtect ) )
	{
		// make sure it is a hotpatchable image... check for 5 nops followed by mov edi,edi
		if( !memcmp( "\x90\x90\x90\x90\x90\x8B\xFF", patch_address, 7 ) || 
			!memcmp( "\xCC\xCC\xCC\xCC\xCC\x8B\xFF", patch_address, 7 ) )
		{
			// overwrite the pad nops above the function entry point with a jump
			patch_address[0] = '\xE9';
			*((DWORD*)(&patch_address[1])) = ((char*)hookproc) - patch_address - 5; // relative address

			// overwrite the functions entry point with a short jmp to the long jmp we just wrote...
			*((WORD*)(&patch_address[5])) = 0xf9eb; // EB F9 == JMP SHORT $-5

			original_proc = (void*) &patch_address[7]; // hop over hook
		}
		VirtualProtect( patch_address, 7, dwPrevProtect, &dwPrevProtect ); // restore protection
	}
	return original_proc;
}

#pragma intrinsic( memcpy )
HFONT __stdcall CreateFontIndirectA_hookproc( CONST LOGFONTA* lplf )
{
	LOGFONTA lf;
	memcpy( &lf, lplf, sizeof(lf) ); 
	lf.lfQuality = NONANTIALIASED_QUALITY;
	return CreateFontIndirectA_origproc( &lf );
}

HFONT __stdcall CreateFontA_hookproc( int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight,
	DWORD fdwItalic, DWORD fdwUnderline, DWORD fdwStrikeOut, DWORD fdwCharSet, 
	DWORD fdwOutputPrecision, DWORD fdwClipPrecision, DWORD fdwQuality, DWORD fdwPitchAndFamily,
    LPCTSTR lpszFace )
{
	fdwQuality = NONANTIALIASED_QUALITY;
	return CreateFontA_origproc( nHeight, nWidth, nEscapement, nOrientation, fnWeight, 
		fdwItalic, fdwUnderline, fdwStrikeOut, fdwCharSet, 
		fdwOutputPrecision, fdwClipPrecision, fdwQuality, fdwPitchAndFamily, 
		lpszFace );
}

void HookFonts(void)
{
	HMODULE gdi_dll;

	gdi_dll = GetModuleHandle("GDI32.dll");

	CreateFontA_origproc = (CREATEFONTA) HotPatch( 
		GetProcAddress( gdi_dll, "CreateFontA" ),
		CreateFontA_hookproc );

	CreateFontIndirectA_origproc = (CREATEFONTINDIRECTA) HotPatch(
		GetProcAddress( gdi_dll, "CreateFontIndirectA" ),
		CreateFontIndirectA_hookproc );
}