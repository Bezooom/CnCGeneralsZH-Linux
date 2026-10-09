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

// compress.c
// Compress interface for packets and files
// Author: Jeff Brown, January 1999

#include <stdio.h>
#include <stdlib.h>
#include "Lib/BaseType.h"
#include "NoxCompress.h"
#if defined(PLATFORM_LINUX) || defined(__linux__)
// Linux: LZH-Light SDK headers live here; system zlib provides the codec.
#if defined(ZLIB_LINUX_SYSTEM)
// System zconf.h typedefs Byte; the game has its own Byte (BaseType.h).
#define Byte sys_Byte
#endif
#include <zlib.h>
#if defined(ZLIB_LINUX_SYSTEM)
#undef Byte
#endif
#if defined(ZLIB_LINUX_SYSTEM)
// System libz exports unprefixed symbols only; map the z_* calls below.
#define z_compress2 compress2
#define z_uncompress uncompress
#define z_compressBound compressBound
#endif
#else
#include "CompLibHeader/lzhl.h"
#endif

#ifdef _INTERNAL
// for occasional debugging...
//#pragma optimize("", off)
//#pragma message("************************************** WARNING, optimization disabled for debugging purposes")
#endif

#define BLOCKSIZE 500000
#define NoxRead fread
#define DbgMalloc malloc
#define DbgFree free
#define DEBUG_LOG(x) {}

#if defined(PLATFORM_LINUX) || defined(__linux__)
// ---------------------------------------------------------------------------
// Linux stand-in for the stripped LZH-Light SDK (see README: CompLibHeader /
// CompLibSource ship empty). NoxLZH payloads are zlib streams behind the same
// "NOX\0"+size framing CompressionManager applies, so round-trips are exact.
// NOT format-compatible with LZH-Light SDK data: retail payloads framed as
// NOX that were written by the SDK will not decode here. Revisit with a real
// LZH decoder (or vendored SDK) before trusting game-data loads.
// ---------------------------------------------------------------------------

UnsignedInt CalcNewSize(UnsignedInt rawSize)
{
	return (UnsignedInt)z_compressBound((uLong)rawSize);
}

Bool CompressMemory(void *inBufferVoid, Int inSize, void *outBufferVoid, Int& outSize)
{
	if ((inBufferVoid == NULL) || (outBufferVoid == NULL) || (inSize < 4) || (outSize == 0))
		return FALSE;

	uLongf destLen = (uLongf)outSize;
	int err = z_compress2((Bytef *)outBufferVoid, &destLen,
		(const Bytef *)inBufferVoid, (uLong)inSize, Z_DEFAULT_COMPRESSION);
	if (err != Z_OK)
		return FALSE;

	outSize = (Int)destLen;
	return TRUE;
}

Bool DecompressMemory(void *inBufferVoid, Int inSize, void *outBufferVoid, Int& outSize)
{
	if ((inBufferVoid == NULL) || (outBufferVoid == NULL) || (inSize < 4) || (outSize == 0))
		return FALSE;

	uLongf destLen = (uLongf)outSize;
	int err = z_uncompress((Bytef *)outBufferVoid, &destLen,
		(const Bytef *)inBufferVoid, (uLong)inSize);
	if (err != Z_OK)
		return FALSE;

	outSize = (Int)destLen;
	return TRUE;
}

Bool CompressFile(char *infile, char *outfile)
{
	if ((infile == NULL) || (outfile == NULL))
		return FALSE;

	FILE *inFilePtr = fopen(infile, "rb");
	if (inFilePtr == NULL)
		return FALSE;
	fseek(inFilePtr, 0, SEEK_END);
	long rawSize = ftell(inFilePtr);
	fseek(inFilePtr, 0, SEEK_SET);
	if (rawSize <= 0) {
		fclose(inFilePtr);
		return FALSE;
	}

	char *inBlock = (char *)DbgMalloc((size_t)rawSize);
	if (inBlock == NULL) {
		fclose(inFilePtr);
		return FALSE;
	}
	if (NoxRead(inBlock, 1, (size_t)rawSize, inFilePtr) != (size_t)rawSize) {
		fclose(inFilePtr);
		DbgFree(inBlock);
		return FALSE;
	}
	fclose(inFilePtr);

	UnsignedInt maxOut = CalcNewSize((UnsignedInt)rawSize);
	char *outBlock = (char *)DbgMalloc(maxOut);
	if (outBlock == NULL) {
		DbgFree(inBlock);
		return FALSE;
	}
	Int outSize = (Int)maxOut;
	Bool ok = CompressMemory(inBlock, (Int)rawSize, outBlock, outSize);

	FILE *outFilePtr = NULL;
	if (ok)
		outFilePtr = fopen(outfile, "wb");
	if (ok && outFilePtr != NULL) {
		UnsignedInt raw = (UnsignedInt)rawSize;
		fwrite(&raw, sizeof(UnsignedInt), 1, outFilePtr);
		fwrite(outBlock, (size_t)outSize, 1, outFilePtr);
		fclose(outFilePtr);
	} else {
		ok = FALSE;
		if (outFilePtr != NULL)
			fclose(outFilePtr);
	}

	DbgFree(inBlock);
	DbgFree(outBlock);
	return ok;
}

Bool DecompressFile(char *infile, char *outfile)
{
	if ((infile == NULL) || (outfile == NULL))
		return FALSE;

	FILE *inFilePtr = fopen(infile, "rb");
	if (inFilePtr == NULL)
		return FALSE;
	fseek(inFilePtr, 0, SEEK_END);
	long fileSize = ftell(inFilePtr);
	fseek(inFilePtr, 0, SEEK_SET);
	if (fileSize <= (long)sizeof(UnsignedInt)) {
		fclose(inFilePtr);
		return FALSE;
	}

	UnsignedInt rawSize = 0;
	NoxRead(&rawSize, 1, sizeof(UnsignedInt), inFilePtr);
	long compressedSize = fileSize - (long)sizeof(UnsignedInt);

	char *inBlock = (char *)DbgMalloc((size_t)compressedSize);
	char *outBlock = (char *)DbgMalloc(rawSize != 0 ? rawSize : 1);
	if ((inBlock == NULL) || (outBlock == NULL)) {
		fclose(inFilePtr);
		DbgFree(inBlock);
		DbgFree(outBlock);
		return FALSE;
	}
	if (NoxRead(inBlock, 1, (size_t)compressedSize, inFilePtr) != (size_t)compressedSize) {
		fclose(inFilePtr);
		DbgFree(inBlock);
		DbgFree(outBlock);
		return FALSE;
	}
	fclose(inFilePtr);

	Int outSize = (Int)rawSize;
	Bool ok = DecompressMemory(inBlock, (Int)compressedSize, outBlock, outSize);

	FILE *outFilePtr = NULL;
	if (ok)
		outFilePtr = fopen(outfile, "wb");
	if (ok && outFilePtr != NULL) {
		fwrite(outBlock, (size_t)outSize, 1, outFilePtr);
		fclose(outFilePtr);
	} else {
		ok = FALSE;
		if (outFilePtr != NULL)
			fclose(outFilePtr);
	}

	DbgFree(inBlock);
	DbgFree(outBlock);
	return ok;
}

Bool CompressPacket(char *inPacket, char *outPacket)
{
	if ((inPacket == NULL) || (outPacket == NULL))
		return FALSE;

	return TRUE;
}

Bool DecompressPacket(char *inPacket, char *outPacket)
{
	if ((inPacket == NULL) || (outPacket == NULL))
		return FALSE;
	return TRUE;
}

#else // Windows / SDK path below

Bool DecompressFile		(char *infile, char *outfile)
{
	UnsignedInt	rawSize = 0, compressedSize = 0;
	FILE *inFilePtr = NULL;
	FILE *outFilePtr= NULL;
	char *inBlock		= NULL;
	char *outBlock	= NULL;
	LZHL_DHANDLE decompress;
	Int ok = 0;
	UnsignedInt srcSz, dstSz;

	// Parameter checking
	 
	if (( infile == NULL ) || ( outfile == NULL ))
		return FALSE;

	inFilePtr = fopen( infile, "rb" );
	if ( inFilePtr )
	{
		// Allocate the appropriate amount of memory
		// Get compressed size of file.
		fseek( inFilePtr, 0, SEEK_END );
		compressedSize = ftell( inFilePtr );
		fseek( inFilePtr, 0, SEEK_SET );

		compressedSize -= sizeof(UnsignedInt);

		// Get uncompressed size. Don't worry about endian, 
		// this is always INTEL baby!
		NoxRead(&rawSize, 1, sizeof(UnsignedInt), inFilePtr);

		// This is ick, but allocate a BIIIIG chunk o' memory x 2
		inBlock = (char *) DbgMalloc( compressedSize );
		outBlock= (char *) DbgMalloc( rawSize );

		if (( inBlock == NULL ) || ( outBlock == NULL ))
			return FALSE;

		// Read in a big chunk o file
		NoxRead(inBlock, 1, compressedSize, inFilePtr);

		fclose(inFilePtr);

		// Decompress
		srcSz = compressedSize;
		dstSz = rawSize;
		
		// Just Do it!
		decompress = LZHLCreateDecompressor();

		for (;;)
		{
			ok = LZHLDecompress( decompress, outBlock + rawSize - dstSz, &dstSz, 
																			 inBlock + compressedSize - srcSz, &srcSz);

			if ( !ok )
				break;

			if (srcSz <= 0)
				break;
		}

		DEBUG_LOG(("Decompressed %s to %s, output size = %d\n", infile, outfile, rawSize));

		LZHLDestroyDecompressor(decompress);
		outFilePtr = fopen(outfile, "wb");
		if (outFilePtr)
		{
			fwrite (outBlock, rawSize, 1, outFilePtr);
			fclose(outFilePtr);
		}
		else
			return FALSE;

		// Clean up this mess
		DbgFree(inBlock);
		DbgFree(outBlock);
		return TRUE;

	} // End of if fileptr

	return FALSE;
}


Bool CompressFile			(char *infile, char *outfile)
{
	UnsignedInt	rawSize = 0;
	UnsignedInt compressedSize = 0, compressed = 0, i = 0;
	FILE *inFilePtr = NULL;
	FILE *outFilePtr= NULL;
	char *inBlock		= NULL;
	char *outBlock	= NULL;
	LZHL_CHANDLE compressor;
	UnsignedInt blocklen;

	// Parameter checking
	 
	if (( infile == NULL ) || ( outfile == NULL ))
		return FALSE;

	// Allocate the appropriate amount of memory
	inFilePtr = fopen( infile, "rb" );
	if ( inFilePtr )
	{
		// Get size of file.
		fseek( inFilePtr, 0, SEEK_END );
		rawSize = ftell( inFilePtr );
		fseek( inFilePtr, 0, SEEK_SET );

		// This is ick, but allocate a BIIIIG chunk o' memory x 2
		inBlock = (char *) DbgMalloc(rawSize);
		outBlock= (char *) DbgMalloc( LZHLCompressorCalcMaxBuf( rawSize ));

		if (( inBlock == NULL ) || ( outBlock == NULL ))
			return FALSE;

		// Read in a big chunk o file
		NoxRead(inBlock, 1, rawSize, inFilePtr);

		fclose(inFilePtr);

		// Compress
		compressor = LZHLCreateCompressor();
		for ( i = 0; i < rawSize; i += BLOCKSIZE )
		{
			blocklen = min((UnsignedInt)BLOCKSIZE, rawSize - i);
			compressed = LZHLCompress(compressor, outBlock + compressedSize, inBlock + i, blocklen);
			compressedSize += compressed;
		}

		LZHLDestroyCompressor(compressor);

		outFilePtr = fopen(outfile, "wb");
		if (outFilePtr)
		{
			// write out the uncompressed size first.
			fwrite(&rawSize, sizeof(UnsignedInt), 1, outFilePtr);
			fwrite(outBlock, compressedSize, 1, outFilePtr);
			fclose(outFilePtr);
		}
		else
			return FALSE;

		// Clean up
		DbgFree(inBlock);
		DbgFree(outBlock);
		return TRUE;
	}

	return FALSE;
}

Bool CompressPacket		(char *inPacket, char *outPacket)
{
	// Parameter checking
	 
	if (( inPacket == NULL ) || ( outPacket == NULL ))
		return FALSE;

	return TRUE;
}


Bool DecompressPacket	(char *inPacket, char *outPacket)
{
	// Parameter checking
	 
	if (( inPacket == NULL ) || ( outPacket == NULL ))
		return FALSE;
	return TRUE;
}


UnsignedInt CalcNewSize		(UnsignedInt rawSize)
{
	return LZHLCompressorCalcMaxBuf(rawSize);
}

Bool DecompressMemory		(void *inBufferVoid, Int inSize, void *outBufferVoid, Int& outSize)
{
	UnsignedByte *inBuffer = (UnsignedByte *)inBufferVoid;
	UnsignedByte *outBuffer = (UnsignedByte *)outBufferVoid;
	UnsignedInt	rawSize = 0, compressedSize = 0;
	LZHL_DHANDLE decompress;
	Int ok = 0;
	UnsignedInt srcSz, dstSz;

	// Parameter checking
	 
	if (( inBuffer == NULL ) || ( outBuffer == NULL ) || ( inSize < 4 ) || ( outSize == 0 ))
		return FALSE;

	// Get compressed size of file.
	compressedSize = inSize;

	// Get uncompressed size.
	rawSize = outSize;

	// Decompress
	srcSz = compressedSize;
	dstSz = rawSize;
	
	// Just Do it!
	decompress = LZHLCreateDecompressor();

	for (;;)
	{
		ok = LZHLDecompress( decompress, outBuffer + rawSize - dstSz, &dstSz, 
																		 inBuffer + compressedSize - srcSz, &srcSz);

		if ( !ok )
			break;

		if (srcSz <= 0)
			break;
	}

	LZHLDestroyDecompressor(decompress);

	outSize = rawSize;

	return TRUE;

}

Bool CompressMemory			(void *inBufferVoid, Int inSize, void *outBufferVoid, Int& outSize)
{
	UnsignedByte *inBuffer = (UnsignedByte *)inBufferVoid;
	UnsignedByte *outBuffer = (UnsignedByte *)outBufferVoid;
	UnsignedInt	rawSize = 0;
	UnsignedInt compressedSize = 0, compressed = 0, i = 0;
	LZHL_CHANDLE compressor;
	UnsignedInt blocklen;

	// Parameter checking
	 
	if (( inBuffer == NULL ) || ( outBuffer == NULL ) || ( inSize < 4 ) || ( outSize == 0 ))
		return FALSE;

	rawSize = inSize;

	// Compress
	compressor = LZHLCreateCompressor();
	for ( i = 0; i < rawSize; i += BLOCKSIZE )
	{
		blocklen = min((UnsignedInt)BLOCKSIZE, rawSize - i);
		compressed = LZHLCompress(compressor, outBuffer + compressedSize, inBuffer + i, blocklen);
		compressedSize += compressed;
	}

	LZHLDestroyCompressor(compressor);

	outSize = compressedSize;

	return TRUE;
}

#endif // defined(PLATFORM_LINUX) || defined(__linux__)
