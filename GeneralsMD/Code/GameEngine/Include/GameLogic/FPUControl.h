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

// FILE: FPUControl.h /////////////////////////////////////////////////////////////////////////////
// Author: Matthew D. Campbell, June 2002
// Desc:	 Routines for controlling the FPU state
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __FPUCONTROL_H__
#define __FPUCONTROL_H__

#include "Lib/BaseType.h"

#if !defined(_MSC_VER)
// Linux twin of the MSVC x87 control-word API (<float.h> there). Real
// fldcw/fstcw/fninit underneath, same Intel bit layout, so setFPMode(),
// getFPMode() and the tests' mode-poking all behave identically.
#define _MCW_PC 0x300
#define _MCW_RC 0x0C00
#define _MCW_EM 0x003F
#define _PC_24 0x0000
#define _PC_53 0x0200
#define _PC_64 0x0300
#define _RC_NEAR 0x0000
#define _RC_DOWN 0x0400
#define _RC_UP 0x0800
#define _RC_CHOP 0x0C00
static inline unsigned _controlfp(unsigned newVal, unsigned mask)
{
  unsigned short oldCw, newCw;
  __asm__ __volatile__("fstcw %0" : "=m"(oldCw));
  newCw = (unsigned short)((oldCw & ~mask) | (newVal & mask));
  __asm__ __volatile__("fldcw %0" : : "m"(newCw));
  return oldCw;
}
static inline void _fpreset(void)
{
  __asm__ __volatile__("fninit");
}
#endif

/**
  * setFPMode sets the FPU internal precision and rounding mode.  As DirectX is not guaranteed to
	* leave the FPU in a good state, we must call this at the start of GameLogic::update() and
	* anywhere that touches DirectX inside GameLogic loops (LoadScreen).
	*/
void setFPMode( void );

/** The precision and rounding fields of the FPU control word, as they are right now. */
UnsignedInt getFPMode( void );

/** What getFPMode() must read back after setFPMode(): 24-bit precision, round to nearest. */
UnsignedInt expectedFPMode( void );

#endif // __FPUCONTROL_H__
