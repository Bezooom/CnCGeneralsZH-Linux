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

// This file contains all the header files that shouldn't change frequently.
// Be careful what you stick in here, because putting files that change often in here will 
// tend to cheese people's goats.

#ifndef __PRERTS_H__
#define __PRERTS_H__

//-----------------------------------------------------------------------------
// srj sez: this must come first, first, first.
#define _STLP_USE_NEWALLOC					1
//#define _STLP_USE_CUSTOM_NEWALLOC		STLSpecialAlloc
class STLSpecialAlloc;


// We actually don't use Windows for much other than timeGetTime, but it was included in 40
// different .cpp files, so I bit the bullet and included it here.
// PLEASE DO NOT ABUSE WINDOWS OR IT WILL BE REMOVED ENTIRELY. :-)
//--------------------------------------------------------------------------------- System Includes
#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
// winuser.h declares AnimateWindow() only for WINVER >= 0x0500, which VC6 never
// reached, so GameClient could name a class AnimateWindow.  A modern SDK always
// declares it, and the function name then hides the class at every use.  Rename
// the API on the way in - the game never calls it - instead of renaming a class
// that GameMemory.ini looks up by name.
#define AnimateWindow AnimateWindow_Win32Unused
#include <atlbase.h>
#include <windows.h>
#undef AnimateWindow

#include <assert.h>
#include <ctype.h>
#include <direct.h>
#include <EXCPT.H>
#include <float.h>
// <fstream.h> (pre-standard iostream) was dropped from MSVC long ago.  Nothing
// in GameEngine uses a stream, so it goes rather than becoming <fstream>.
#include <imagehlp.h>
#include <io.h>
#include <limits.h>
#include <lmcons.h>
// <mapicode.h> came with the MAPI SDK, which no Windows SDK carries any more.
// Nothing here references a MAPI symbol.
#include <math.h>
#include <memory.h>
#include <mmsystem.h>
#include <objbase.h>
#include <ocidl.h>
#include <process.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlguid.h>
#include <snmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <TCHAR.H>
#include <time.h>
#include <vfw.h>
#include <winerror.h>
#include <wininet.h>
#include <winreg.h>

#ifndef DIRECTINPUT_VERSION
#	define DIRECTINPUT_VERSION	0x800
#endif

#include <dinput.h>
#else
// Linux: portable CRT subset of the block above plus the compat shims that
// cover what game sources actually use (HANDLE/DWORD/etc. via win.h,
// timeGetTime via mmsys.h, TCHAR via tchar.h, D3DX math via d3dx9math.h).
// ATL/COM/shell/MAPI/SNMP/VFW/WinInet/registry/DirectInput have no Linux
// twin here; their users are ported per-file in later phases.
#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "win.h"
#include "mmsys.h"
#include "tchar.h"
#include "d3dx9math.h"
// MSVC CRT names used across GameEngine (stat family, mkdir, chmod...).
// _stat as a macro (not a typedef) so `struct _stat` also maps to struct stat.
#define _stat stat
#define _mkdir(p) mkdir(p, 0755)
#define _getcwd getcwd
#define _chmod chmod
#define _unlink unlink
#define _rmdir rmdir
#define _S_IFDIR S_IFDIR
#define _S_IFREG S_IFREG
#define _S_IWRITE S_IWUSR
#define _S_IREAD S_IRUSR
#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif
#ifndef _MAX_DIR
#define _MAX_DIR 256
#endif
#ifndef _MAX_DRIVE
#define _MAX_DRIVE 3
#endif
#ifndef _MAX_FNAME
#define _MAX_FNAME 256
#endif
#ifndef _MAX_EXT
#define _MAX_EXT 256
#endif
// Own-exe path (several TUs): /proc/self/exe twin of GetModuleFileName(NULL,...).
static inline DWORD GameGetModuleFileName(HMODULE hModule, char *buf, DWORD size)
{
  (void)hModule;
  if (size == 0)
    return 0;
  ssize_t n = readlink("/proc/self/exe", buf, (size_t)size - 1);
  if (n < 0)
    return 0;
  buf[n] = 0;
  return (DWORD)n;
}
#define GetModuleFileName GameGetModuleFileName
// Double-click time (GlobalData default): the Windows default is 500ms and
// there is no Linux desktop setting to query headless.
static inline unsigned int GameGetDoubleClickTime(void) { return 500; }
#define GetDoubleClickTime GameGetDoubleClickTime
// Screen metrics (borderless-window sizing): 1920x1080, matching WWLib/win.h's
// GetSystemMetrics - the borderless tests assert the parsed resolution equals
// these, so the two spellings must agree. Borderless is the only in-tree user.
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
static inline int GameGetSystemMetrics(int code)
{
  if (code == SM_CXSCREEN)
    return 1920;
  if (code == SM_CYSCREEN)
    return 1080;
  return 0;
}
#define GetSystemMetrics GameGetSystemMetrics
// CreateDirectory returning BOOL like the API (mkdir returns 0 on success).
static inline BOOL GameCreateDirectory(const char *path, void *unused)
{
  (void)unused;
  return mkdir(path, 0755) == 0 ? TRUE : FALSE;
}
#define CreateDirectory GameCreateDirectory
#endif

//------------------------------------------------------------------------------------ STL Includes
// srj sez: no, include STLTypesdefs below, instead, thanks
//#include <algorithm>
//#include <bitset>
//#include <hash_map>
//#include <list>
//#include <map>
//#include <queue>
//#include <set>
//#include <stack>
//#include <string>
//#include <vector>

//------------------------------------------------------------------------------------ RTS Includes
// Icky. These have to be in this order.
#include "Lib/BaseType.h"
#include "Common/STLTypedefs.h"
#include "Common/Errors.h"
#include "Common/Debug.h"
#include "Common/AsciiString.h"
#include "Common/SubsystemInterface.h"

#include "Common/GameCommon.h"
#include "Common/GameMemory.h"
#include "Common/GameType.h"
#include "Common/GlobalData.h"

// You might not want Kindof in here because it seems like it changes frequently, but the problem
// is that Kindof is included EVERYWHERE, so it might as well be precompiled.
#include "Common/INI.h"
#include "Common/KindOf.h"
#include "Common/DisabledTypes.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/ClientRandomValue.h"
#include "GameLogic/LogicRandomValue.h"
#include "Common/ObjectStatusTypes.h"

#include "Common/Thing.h"
#include "Common/UnicodeString.h"

// The bounded string calls are meant to be the default vocabulary, so they are reachable from every
// engine translation unit rather than included one file at a time as each one gets swept.
#include "stringex.h"

#endif /* __PRERTS_H__ */
