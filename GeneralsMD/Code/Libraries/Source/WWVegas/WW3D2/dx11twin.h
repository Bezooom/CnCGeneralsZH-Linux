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

/*
** The Direct3D 11 copy of a vertex or index buffer, and the heap block both copies are written
** through.
**
** A D3D9 buffer is created write-only, so nothing can read back what the engine put in it and there
** is no way to build the D3D11 copy after the fact.  The twin solves that by moving the write: when
** one exists, a lock hands out the twin's own heap block rather than the D3D9 pointer, and the
** unlock writes that range into both buffers.  The D3D9 side is byte for byte what it was without
** the twin, so a -dx11 run draws the same frame it would have drawn; the difference is that the
** D3D11 buffer now holds the same bytes and a draw can go through the backend.
**
** Nothing here asks whether Direct3D 11 is running.  The device and its context arrive as
** arguments, which keeps the file testable against an offscreen device; dx11runtime answers null
** for the buffers on an ordinary run, and every caller carries that null through to the old path.
*/

#ifndef DX11TWIN_H
#define DX11TWIN_H

#ifndef PLATFORM_LINUX
struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11DeviceContext;

class DX11BufferTwinClass
{
public:
	DX11BufferTwinClass(unsigned char * mirror, unsigned byte_count, ID3D11Buffer * buffer,
		ID3D11DeviceContext * context, bool dynamic);
	~DX11BufferTwinClass();

	unsigned char * Mirror() const { return Mirror_Bytes; }
	unsigned Byte_Count() const { return Byte_Total; }
	ID3D11Buffer * Buffer() const { return D3D11Buffer; }
	ID3D11DeviceContext * Context() const { return D3D11Context; }
	bool Dynamic() const { return IsDynamic; }
	bool Is_Dynamic() const { return IsDynamic; }

	// A dynamic buffer mapped for writing with D3D9's own DISCARD or NOOVERWRITE, the way Upload
	// maps it; null when the device refuses.  The caller writes straight into it, then calls Unmap.
	unsigned char * Map_Write(bool discard);
	void Unmap();

	void Write_Range(unsigned byte_offset, unsigned byte_count, bool discard);

private:
	unsigned char * Mirror_Bytes;
	unsigned Byte_Total;
	ID3D11Buffer * D3D11Buffer;
	ID3D11DeviceContext * D3D11Context;
	bool IsDynamic;
};

DX11BufferTwinClass * DX11Twin_Create_Vertex_Buffer(ID3D11Device * device,
	ID3D11DeviceContext * context, unsigned byte_count, bool dynamic);
DX11BufferTwinClass * DX11Twin_Create_Index_Buffer(ID3D11Device * device,
	ID3D11DeviceContext * context, unsigned byte_count, bool dynamic);

class DX11BufferLockClass
{
public:
	DX11BufferLockClass();

	// lock_flags is the D3DLOCK_* set the caller handed Direct3D 9, so the upload can be written
	// the same way the D3D9 lock was.
	//
	// copy_to_d3d9 false leaves the D3D9 memory as it was.  A dynamic buffer passes it while
	// Direct3D 11 presents: Direct3D 9 skips its indexed draws then and nothing reads a D3D9 buffer
	// back, and a dynamic buffer is written again every frame, so the copy was a second memcpy of
	// every particle vertex for a device that never looks at it.  A dynamic twin locked that way is
	// also written straight into the mapped D3D11 buffer, with no mirror to fill and copy out of.
	void * Begin(DX11BufferTwinClass * twin, void * d3d9_memory, unsigned byte_offset,
		unsigned byte_count, unsigned lock_flags, bool copy_to_d3d9 = true);
	void End();

private:
	DX11BufferTwinClass * Twin;
	void * D3D9Memory;
	unsigned ByteOffset;
	unsigned ByteCount;
	bool Discard;
	bool CopyToD3D9;
	bool Mapped;			// Begin handed out the mapped D3D11 buffer itself, so End only unmaps
};
#else
class DX11BufferTwinClass
{
public:
	~DX11BufferTwinClass() {}
};

class DX11BufferLockClass
{
public:
	DX11BufferLockClass() : Twin(nullptr), D3D9Memory(nullptr), ByteOffset(0), ByteCount(0), Discard(false) {}
	void * Begin(DX11BufferTwinClass * twin, void * d3d9_memory, unsigned, unsigned, unsigned) { return d3d9_memory; }
	void End() {}

private:
	DX11BufferTwinClass * Twin;
	void * D3D9Memory;
	unsigned ByteOffset;
	unsigned ByteCount;
	bool Discard;
};
#endif

#endif // DX11TWIN_H
