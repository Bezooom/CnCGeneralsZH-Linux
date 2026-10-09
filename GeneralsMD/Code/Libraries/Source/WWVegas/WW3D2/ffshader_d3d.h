// Numeric input vocabulary for the standalone combiner-to-HLSL generator.
// This is not a Direct3D runtime shim: no devices, interfaces or renderer types.
#ifndef FFSHADER_D3D_H
#define FFSHADER_D3D_H

#ifdef _WIN32
#include <d3d9.h>
#else
#include <cstdint>

// D3D9's DWORD is always unsigned 32-bit, including on LP64 hosts.
using DWORD = std::uint32_t;

// Values from the D3D9 SDK's D3DTEXTUREOP and D3DCMPFUNC vocabulary.
// Only inputs used by ffshader and its standalone contract are exposed.
enum : DWORD
{
	D3DTOP_DISABLE = 1,
	D3DTOP_SELECTARG1 = 2,
	D3DTOP_SELECTARG2 = 3,
	D3DTOP_MODULATE = 4,
	D3DTOP_MODULATE2X = 5,
	D3DTOP_MODULATE4X = 6,
	D3DTOP_ADD = 7,
	D3DTOP_ADDSIGNED = 8,
	D3DTOP_ADDSIGNED2X = 9,
	D3DTOP_SUBTRACT = 10,
	D3DTOP_ADDSMOOTH = 11,
	D3DTOP_BLENDDIFFUSEALPHA = 12,
	D3DTOP_BLENDTEXTUREALPHA = 13,
	D3DTOP_BLENDFACTORALPHA = 14,
	D3DTOP_BLENDTEXTUREALPHAPM = 15,
	D3DTOP_BLENDCURRENTALPHA = 16,
	D3DTOP_MODULATEALPHA_ADDCOLOR = 18,
	D3DTOP_MODULATECOLOR_ADDALPHA = 19,
	D3DTOP_MODULATEINVALPHA_ADDCOLOR = 20,
	D3DTOP_MODULATEINVCOLOR_ADDALPHA = 21,
	D3DTOP_BUMPENVMAP = 22,
	D3DTOP_DOTPRODUCT3 = 24,
	D3DTOP_MULTIPLYADD = 25,
	D3DTOP_LERP = 26,

	D3DCMP_NEVER = 1,
	D3DCMP_LESS = 2,
	D3DCMP_EQUAL = 3,
	D3DCMP_LESSEQUAL = 4,
	D3DCMP_GREATER = 5,
	D3DCMP_NOTEQUAL = 6,
	D3DCMP_GREATEREQUAL = 7,
	D3DCMP_ALWAYS = 8,

	D3DTA_DIFFUSE = 0x0,
	D3DTA_CURRENT = 0x1,
	D3DTA_TEXTURE = 0x2,
	D3DTA_TFACTOR = 0x3,
	D3DTA_SPECULAR = 0x4,
	D3DTA_SELECTMASK = 0xf,
	D3DTA_COMPLEMENT = 0x10,
	D3DTA_ALPHAREPLICATE = 0x20,

	D3DTSS_TCI_CAMERASPACEPOSITION = 0x20000
};
#endif

static_assert(sizeof(DWORD) == 4, "Combiner input words must retain the D3D9 width");
static_assert(static_cast<DWORD>(-1) > 0, "Combiner input words must be unsigned");

#endif
