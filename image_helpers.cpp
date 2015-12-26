#include <windows.h>
#include "header.h"

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
		sub DWORD PTR [ebp], 1
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

// for each 64 byte chunk:
// composite dib_bits onto client_bits using 0xFE as the transparent color-key
// then fill dib_bits with 0xFE,
// then color convert client_bits into d3d_bits
__declspec(naked) void __stdcall multiblt( DWORD pitch, DWORD* d3d_bits )
{
	__asm {
	pcmpeqw xmm7, xmm7 
	push edi
	push esi
	psllw xmm7, 1
	push ebp
	push ebx
	packsswb xmm7, xmm7 // 0xFEFEFEFEFEFEFEFEFEFEFEFEFEFEFEFE
	sub DWORD PTR 20[esp],640*4 
	push 480
	push 640/64
	sub esp,4 	
	mov ebp, dib_bits
	mov edi, 36[esp] // d3d_bits
	lea esi, [client_bits]
	mov 36[esp],ebp
/*
esi == client_bits_cp
edi == d3d_bits_cp
ebp == dib_bits_cp or palette
[esp+36] == preserve dib_bits_cp while ebp is set to palette
[esp+32] == d3d_bits->pitch_in_bytes - width_in_bytes
[esp+8] == height count down
[esp+4] == width count down
[esp] == 64/4 count down
*/
	align 8
L1:
	// read src 
	movdqa xmm0, [ebp]
	movdqa xmm1, 16[ebp]
	movdqa xmm2, 32[ebp]
	movdqa xmm3, 48[ebp]
	// erase src
	movdqa [ebp], xmm7
	movdqa 16[ebp], xmm7 
	movdqa 32[ebp], xmm7
	movdqa 48[ebp], xmm7
	// mask = ( src == 0xFE ) ? 0xFF : 0x00
	// dst = ( !mask & src ) | ( mask & dst )	
	movdqa xmm4, [esi] // read 0[dst]
	movdqa xmm6, 16[esi]
	movdqa xmm5, xmm0  // preserve [src]
	pcmpeqb xmm0, xmm7 // generate mask for [src]
	pand xmm4, xmm0    // mask & [dst]
	pandn xmm0, xmm5   // !mask & [src]
	movdqa xmm5, xmm1
	pcmpeqb xmm1, xmm7
	por xmm0, xmm4     // new [dst]
	movdqa xmm4, 32[esi]
	pand xmm6, xmm1
	pandn xmm1, xmm5
	movdqa xmm5, xmm2
	pcmpeqb xmm2, xmm7
	por xmm1, xmm6
	movdqa xmm6, 48[esi]
	pand xmm4, xmm2
	pandn xmm2, xmm5
	movdqa xmm5, xmm3
	pcmpeqb xmm3, xmm7
	por xmm2, xmm4
	pand xmm6, xmm3
	pandn xmm3, xmm5
	add DWORD PTR 36[esp], 64 // move pointer ahead 64 bytes
	por xmm3, xmm6
	lea ebp, [bmi.palette]
	// fastest?? to write back to cache line then movzx to r32?
	movdqa [esi], xmm0
	movdqa 16[esi], xmm1
	movdqa 32[esi], xmm2
	movdqa 48[esi], xmm3
	mov DWORD PTR [esp],64/4
	align 8
L2:
	// read src image
	movzx eax, byte ptr [esi]
	movzx ebx, byte ptr 1[esi]
	movzx ecx, byte ptr 2[esi]
	movzx edx, byte ptr 3[esi]
	add esi, 4
	// read colors from palette
	mov eax, [ebp+eax*4] 
	mov ebx, [ebp+ebx*4] 
	mov ecx, [ebp+ecx*4] 
	mov edx, [ebp+edx*4]
	sub DWORD PTR [esp], 1 // loop cnt
	// write colors to dst
	mov   [edi], eax
	mov  4[edi], ebx
	mov  8[edi], ecx
	mov 12[edi], edx
	//
	lea edi, [edi+16]
	jnz L2 // do 64 bytes
	mov ebp, 36[esp]
	sub DWORD PTR 4[esp], 1
	jnz L1 // do 640 bytes
	add edi, 32[esp] // padding at end of scanline
	sub DWORD PTR 8[esp], 1
	mov DWORD PTR 4[esp], 640/64	
	jnz L1 // do 480 scanline
	add esp, 12 
	pop ebx
	pop ebp
	pop esi
	pop edi
	ret 8
	}
}

/*  composite for 386
	// add 1, cmp < 0xFF, then gen mask with sbb?
	cmp
	sbb edx, edx ; = (b > a) ? 0xFFFFFFFF : 0
	and edx, eax ; 
	add ebx, edx ;
*/

/*
// not tested
// memcchr( p, 0xFE, 640 )...
__declspec(naked) bool __cdecl is_scanline_dirty( BYTE* p )
{	
	pcmpeqw xmm7, xmm7 
	mov edx,4[esp]
	psllw xmm7, 1
	mov ecx, 640/64
	packsswb xmm7, xmm7 // 0xFEFEFEFEFEFEFEFEFEFEFEFEFEFEFEFE
L1:
	movdqa xmm0, [edx]
	movdqa xmm1, 16[edx]
	movdqa xmm2, 32[edx]
	movdqa xmm3, 48[edx]
	add edx,64
	sub ecx,1
	pcmpeqd xmm0, xmm7 // pcmpeqd should be just as fast as pxor ?
	pcmpeqd xmm1, xmm7
	pcmpeqd xmm2, xmm7
	pcmpeqd xmm3, xmm7
	por xmm0,xmm1
	por xmm4,xmm2
	por xmm5,xmm3
	por xmm6,xmm0
	jnz L1
	por xmm4,xmm5
	por xmm4,xmm6
	pmovmskb eax,xmm4
	ret
}
*/
