#include <windows.h>

BOOL sse2_supported;

// convert scanline from 8bpp to 32bpp
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

// TransparentBlt scanline, clear_color is index 0xFE 
__declspec(naked) void __cdecl composite( BYTE* src, BYTE* dst )
{
	__asm {
	mov eax, 4[esp]
	mov edx, 8[esp]
	mov ecx, 640*480/64 // loop cnt

	// load xmm7 with the transparent color index
	// aka. 0xFEFEFEFEFEFEFEFEFEFEFEFEFEFEFEFE
	pcmpeqw xmm7,xmm7 
	psllw xmm7,1 
	packsswb xmm7,xmm7 
	
	// mask = ( is_transparent ) ? 0xFF : 0x00
	// dst = ( !mask & src ) | ( mask & dst )
	align 8
lbl_loop:
	movdqa xmm0, [eax]
	  movdqa xmm1, 16[eax]
	    movdqa xmm2, 32[eax]
	      movdqa xmm3, 48[eax]
	movdqa xmm5, [edx]         // read 0[edi]
	movdqa xmm4, xmm0          // preserve [esi]
	pcmpeqb xmm0, xmm7         // generate mask for [esi]
	  movdqa xmm6, 16[edx]       // read 16[edi]
	pandn xmm0, xmm4           // !mask & [esi] 
	pand xmm4, xmm5            // mask & [edi]
	  movdqa xmm5, xmm1          // preserve 16[esi]
	  pcmpeqb xmm1, xmm7         // generate mask for 16[esi]
	por xmm0, xmm4             // new [edi]
	    movdqa xmm4, 32[edx]       // read 32[edi]
	  pandn xmm5, xmm1           // !mask & 16[esi]
	  pand xmm1, xmm6            // mask & 16[edi]
	    movdqa xmm6, xmm2          // preserve 32[esi]
	    pcmpeqb xmm2, xmm7         // generate mask for 32[esi]
	  por xmm1,xmm5              // new 16[edi]
	      movdqa xmm5, 48[edx]       // read 48[edi]
	    pandn xmm6, xmm2           // !mask & 32[esi]
	    pand xmm2, xmm4            // mask & 32[edi]
	      movdqa xmm4, xmm3          // preserve 48[esi]
	      pcmpeqb xmm3, xmm7         // generate mask for 48[esi]
	    por xmm2,xmm6              // new 32[edi]
	      pandn xmm4, xmm3           // !mask & 48[esi]
	      pand xmm3, xmm5            // mask & 48[edi]
	      por xmm3, xmm4             // new 48[edi]
	// fastest?? to write back to cache line then mov to r32
	movdqa [edx], xmm0
	  movdqa 16[edx], xmm1
	    movdqa 32[edx], xmm2
	      movdqa 48[edx], xmm3

	// upkeep
	dec ecx
	jnz lbl_loop
	ret
	}

/*  composite for 386
	// add 1, cmp < 0xFF, then gen mask with sbb?
	cmp
	sbb edx, edx ; = (b > a) ? 0xFFFFFFFF : 0
	and edx, eax ; 
	add ebx, edx ;
*/
}

// memset( src, 0xFE, 640*480 )
__declspec(naked) void __cdecl erase( BYTE* src )
{
	__asm{
		mov edx, sse2_supported
		mov eax, 4[esp]
		mov ecx, 640*480/64
		test edx,edx
		jz lbl_386

	// sse2
		pcmpeqw xmm0,xmm0 
		psllw xmm0,1 
		packsswb xmm0,xmm0 
		align 8
	lbl_loop_sse2:
		movntdq [eax], xmm0
		movntdq 16[eax], xmm0
		movntdq 32[eax], xmm0
		movntdq 48[eax], xmm0
		add eax,64
		dec ecx
		jnz lbl_loop_sse2
		ret

	// 386
	lbl_386:
		mov edx, 0xFEFEFEFE
		align 8
	lbl_loop_386:
		mov [eax],edx
		mov 0x04[eax],edx
		mov 0x08[eax],edx
		mov 0x0C[eax],edx
		mov 0x10[eax],edx
		mov 0x14[eax],edx
		mov 0x18[eax],edx
		mov 0x1C[eax],edx
		mov 0x20[eax],edx
		mov 0x24[eax],edx
		mov 0x28[eax],edx
		mov 0x2C[eax],edx
		mov 0x30[eax],edx
		mov 0x34[eax],edx
		mov 0x38[eax],edx
		mov 0x3C[eax],edx
		dec ecx
		jnz lbl_loop_386
		ret
	}
}



