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

/*********************************************************************************************** 
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               *** 
 *********************************************************************************************** 
 *                                                                                             * 
 *                 Project Name : Command & Conquer                                            * 
 *                                                                                             * 
 *                     $Archive:: /Commando/Code/wwlib/mmsys.h                                $* 
 *                                                                                             * 
 *                      $Author:: Tom_s                                                       $*
 *                                                                                             * 
 *                     $Modtime:: 6/12/00 2:36p                                               $*
 *                                                                                             * 
 *                    $Revision:: 2                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------* 
 * Functions:                                                                                  * 
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef MMSYS_H
#define MMSYS_H

/*
** This header just includes mmsystem.h with warning 4201 disabled
** Linux: mmsystem.h is Win32-only; provide clock_gettime-based shims so
** timeGetTime/timeBeginPeriod call sites compile without winmm.
*/

#if defined(_MSC_VER)
#pragma warning(disable:4201)
#include <mmsystem.h>
#pragma warning(default:4201)
#else
#ifndef WWLIB_MMSYS_LINUX_SHIM
#define WWLIB_MMSYS_LINUX_SHIM 1
#include "bittype.h"
#include <time.h>
#ifndef MMRESULT
typedef unsigned int MMRESULT;
#endif
// Performance-counter twins (GameEngine frame timing). Nanoseconds since
// boot at 10^9 ticks/second. LARGE_INTEGER carries QuadPart for Windows
// compatibility; use sites access .QuadPart or cast to/from long long.
typedef struct { long long QuadPart; } LARGE_INTEGER;
static inline int QueryPerformanceCounter(LARGE_INTEGER *out)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	out->QuadPart = (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
	return 1;
}
static inline int QueryPerformanceFrequency(LARGE_INTEGER *out)
{
	out->QuadPart = 1000000000LL;
	return 1;
}
#ifndef WWLIB_TIMEGETTIME_DEFINED
#define WWLIB_TIMEGETTIME_DEFINED 1
#ifndef TIMERR_NOERROR
#define TIMERR_NOERROR 0
#endif
static inline DWORD timeGetTime(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (DWORD)((ts.tv_sec * 1000ULL) + (ts.tv_nsec / 1000000ULL));
}
static inline MMRESULT timeBeginPeriod(unsigned int) { return TIMERR_NOERROR; }
static inline MMRESULT timeEndPeriod(unsigned int) { return TIMERR_NOERROR; }
#endif
#endif
#endif

#endif // MMSYS_H
