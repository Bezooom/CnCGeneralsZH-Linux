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

/////////////////////////////////////////////////////////////////////////
// Test-only stand-ins for DownloadDebug.h's DebugLog/DebugCrash.
// GameEngine/Source/Common/System/Debug.cpp provides the real ones; they
// live here (not in wwdownload) so the gameengine link never sees two
// definitions.
/////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdarg.h>

extern "C" {

char *TheCurrentIgnoreCrashPtr = NULL;

void DebugLog(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
}

void DebugCrash(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
  fflush(stderr);
}

} // extern "C"
