#include <windows.h>
__declspec(naked) void __cdecl 
color_convert( BYTE* src, RGBQUAD* pal, DWORD* dst, DWORD cnt )
{
	__asm
	{
		push edi
		push esi
		push ebp
		push ebx

		mov esi,0x14[esp] // src
		mov ebp,0x18[esp] // pal
		mov edi,0x1C[esp] // dst
	lbl_loop:
		// read src image
		movzx eax, byte ptr [esi]
		movzx ebx, byte ptr 1[esi]
		movzx ecx, byte ptr 2[esi]
		movzx edx, byte ptr 3[esi]

		// read colors from palette
		mov eax, [ebp+eax*4] 
		mov ebx, [ebp+ebx*4] 
		mov ecx, [ebp+ecx*4] 
		mov edx, [ebp+edx*4] 

		lea ebp,0x20[esp] // cnt

		// write colors to dst
		mov   [edi], eax
		mov  4[edi], ebx
		mov  8[edi], ecx
		mov 12[edi], edx

		// upkeep
		sub [ebp], 1
		lea esi, [esi+4]
		lea edi, [edi+16]
		mov ebp,0x18[esp] // pal
		jnz lbl_loop

		pop ebx
		pop ebp
		pop esi
		pop edi
		ret
	}
}


