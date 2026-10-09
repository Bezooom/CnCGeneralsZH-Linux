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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// CRC.h ///////////////////////////////////////////////////////////////
// A class encapsulating CRC calculation
// Author: Matthew D. Campbell, October 2001

#pragma once

#ifndef _CRC_H_
#define _CRC_H_

#include "Lib/BaseType.h"

// Platform abstraction for CRC32
#include "PAL_Timing.h"

#ifdef _DEBUG

class CRC
{
public:
	CRC() { crc = 0; }

	void computeCRC( const void *buf, Int len );		///< Compute the CRC for a buffer, added into current CRC
	void clear( void ) { crc = 0; }									///< Clears the CRC to 0
	UnsignedInt get( void ); ///< Get the combined CRC

private:
	void addCRC( UnsignedByte val );									///< CRC a 4-byte block

	UnsignedInt crc;
};

#else

// optimized inline only version
class CRC
{
public:
	CRC(void) { crc=0; }

  /// Compute the CRC for a buffer, added into current CRC
	__forceinline void computeCRC( const void *buf, Int len )
  {
    if (!buf||len<1)
      return;
    
    // Portable CRC32 implementation (slicing-by-8)
    crc = Platform_CRC32(buf, len, crc);
  }

  /// Clears the CRC to 0
	void clear( void ) 
  { 
    crc = 0; 
  }									

  ///< Get the combined CRC
	UnsignedInt get( void ) const
  {
    return crc;
  }

private:
	UnsignedInt crc;
};

#endif

#endif // _CRC_H_
