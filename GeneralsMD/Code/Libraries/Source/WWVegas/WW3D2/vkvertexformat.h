/*
** A D3D9 flexible vertex format, read out as the one vertex layout the Vulkan pipeline accepts.
**
** The engine writes its vertices by hand through the structs in dx8fvf.h, so the bytes do not
** change. What changes is that Vulkan has no notion of an FVF: it is told, per attribute, which
** bytes to read and as what type. The pipeline therefore reads exactly one layout - position, two
** texture coordinate sets, one colour, and two projected coordinates the host fills in for
** camera-space texture stages - and every FVF the engine uses is decoded into it.
**
** Two things here are worth testing rather than eyeballing.
**
** The offsets. D3D9 packs a vertex as position, blend weights, normal, point size, diffuse,
** specular, then the texture coordinates, and the engine's own FVFInfoClass computes the same
** offsets from the same bits. A wrong offset reads the normal as the colour, which is why a
** mis-decoded vertex renders green: the normal is usually (0,1,0) and a shader that treats it as a
** colour paints the model green.
**
** The colour type. A D3DCOLOR is four bytes, BGRA in memory. Reading it as a float gives a value
** near zero, so a decoded vertex that skips the unpack renders nearly black. The canonical layout
** stores four normalised floats instead.
**
** A format that carries no colour or no texture coordinates is not an error: the decoder writes
** white and (0,0), which is what a fixed-function stage with those inputs disabled produces.
*/

#ifndef VKVERTEXFORMAT_H
#define VKVERTEXFORMAT_H

// Self-contained on purpose. ww3d_d3d9.h includes this header, so including it back would close
// this guard before the structs exist. The FVF constants live in ww3d_d3d9.h and are only needed
// by the implementation, which includes it.

// The layout the Vulkan pipeline reads. Offsets are byte offsets into the engine's vertex bytes;
// -1 means the format does not carry that field.
struct VKVertexLayout {
	unsigned stride;
	unsigned uvFloatCount;
	unsigned uv1FloatCount;
	int positionOffset;
	int uvOffset;
	int uv1Offset;
	int normalOffset; // Source-only: the lighting pass consumes it before upload.
	int colorOffset;
};

// Position, texture coordinate set 0, one colour as four normalised floats, texture coordinate
// set 1, and the two projected coordinates. 60 bytes; the pipeline's attribute offsets are 0, 12,
// 20, 36, 44 and 52, in this order.
struct VKCanonicalVertex {
	float position[3];
	float uv[2];
	float color[4];
	float uv1[2];
	float projected0[2];
	float projected1[2];
};

// Canonical stride, shared with Platform/PAL_Vulkan.h so the pipeline's attribute offsets and the
// decoder agree.
const unsigned VK_CANONICAL_VERTEX_STRIDE = 60;

bool VKLayout_From_FVF(unsigned fvf, VKVertexLayout &layout, unsigned texCoordIndex = 0);
void VKDecodeVertex(const unsigned char *source, const VKVertexLayout &layout, VKCanonicalVertex &out);

#endif // VKVERTEXFORMAT_H
