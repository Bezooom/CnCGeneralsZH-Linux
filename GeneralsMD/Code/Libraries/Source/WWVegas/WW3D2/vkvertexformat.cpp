/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ww3d_d3d9.h"
#include "vkvertexformat.h"

// The two bits per set that D3DFVF_TEXCOORDSIZE writes start here.
static const unsigned TEXTURE_COORDINATE_SIZE_SHIFT = 16;
static const DWORD TEXTURE_COORDINATE_SIZE_MASK = 0x03;

// Same encoding as dx11layout.cpp: 0 means two floats, 1 means three, 2 means four and 3 means one.
static unsigned texture_coordinate_float_count(DWORD fvf, unsigned set)
{
	const DWORD code = (fvf >> (TEXTURE_COORDINATE_SIZE_SHIFT + set * 2)) & TEXTURE_COORDINATE_SIZE_MASK;
	switch (code) {
	case 0:  return 2;
	case 1:  return 3;
	case 2:  return 4;
	default: return 1;
	}
}

bool VKLayout_From_FVF(DWORD fvf, VKVertexLayout &layout)
{
	layout.positionOffset = -1;
	layout.uvOffset = -1;
	layout.colorOffset = -1;
	layout.uvFloatCount = 0;

	unsigned offset = 0;

	const DWORD position = fvf & D3DFVF_POSITION_MASK;
	if (position == D3DFVF_XYZ) {
		layout.positionOffset = (int)offset;
		offset += 12;
	} else if (position == D3DFVF_XYZRHW) {
		// Screen-space vertices carry a fourth float for w, which the decoder does not need.
		layout.positionOffset = (int)offset;
		offset += 16;
	} else if (position != 0) {
		// Every other D3DFVF_POSITION_MASK value is a blend-weight form the engine never fills.
		return false;
	}

	if ((fvf & D3DFVF_NORMAL) != 0) {
		offset += 12;
	}
	if ((fvf & D3DFVF_PSIZE) != 0) {
		offset += 4;
	}
	if ((fvf & D3DFVF_DIFFUSE) != 0) {
		layout.colorOffset = (int)offset;
		offset += 4;
	}
	if ((fvf & D3DFVF_SPECULAR) != 0) {
		offset += 4;
	}

	const unsigned set_count = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	if (set_count > 0) {
		layout.uvOffset = (int)offset;
		layout.uvFloatCount = texture_coordinate_float_count(fvf, 0);
		// The engine writes every set, so the stride has to cover them all even though only set 0
		// reaches the pipeline. A stride that stops at set 0 reads the next vertex from the wrong
		// bytes.
		for (unsigned set = 0; set < set_count; ++set)
			offset += texture_coordinate_float_count(fvf, set) * 4;
	}

	if (offset == 0) {
		return false;
	}
	layout.stride = offset;
	return true;
}

void VKDecodeVertex(const unsigned char *source, const VKVertexLayout &layout, VKCanonicalVertex &out)
{
	for (int i = 0; i < 3; ++i) out.position[i] = 0.0f;
	for (int i = 0; i < 2; ++i) out.uv[i] = 0.0f;
	for (int i = 0; i < 4; ++i) out.color[i] = 1.0f;

	if (layout.positionOffset >= 0) {
		const float *position = (const float *)(source + layout.positionOffset);
		out.position[0] = position[0];
		out.position[1] = position[1];
		out.position[2] = position[2];
	}

	if (layout.uvOffset >= 0) {
		const float *uv = (const float *)(source + layout.uvOffset);
		out.uv[0] = uv[0];
		out.uv[1] = layout.uvFloatCount >= 2 ? uv[1] : 0.0f;
	}

	if (layout.colorOffset >= 0) {
		// A D3DCOLOR is BGRA in memory, so the bytes come out in the order the shader wants them.
		out.color[0] = source[layout.colorOffset + 2] / 255.0f;
		out.color[1] = source[layout.colorOffset + 1] / 255.0f;
		out.color[2] = source[layout.colorOffset + 0] / 255.0f;
		out.color[3] = source[layout.colorOffset + 3] / 255.0f;
	}
}
