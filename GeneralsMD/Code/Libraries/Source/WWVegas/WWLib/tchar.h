/*
** WWLib tchar.h — Linux port shim (2026-09-12)
**
** <tchar.h> is a Visual C++ header. The game builds single-byte strings on
** Linux, so TCHAR maps to char and the _tcs* family maps to <string.h>.
*/

#ifndef WWLIB_TCHAR_LINUX_SHIM
#define WWLIB_TCHAR_LINUX_SHIM 1

#include <string.h>
#include <stdio.h>

typedef char TCHAR;
typedef const char* LPCTSTR;
typedef char* LPTSTR;

#ifndef _T
#define _T(x) x
#endif
#ifndef _TEXT
#define _TEXT(x) x
#endif

#ifndef _tcslen
#define _tcslen strlen
#endif
#ifndef _tcsclen
#define _tcsclen strlen
#endif
#ifndef _tcscpy
#define _tcscpy strcpy
#endif
#ifndef _tcsncpy
#define _tcsncpy strncpy
#endif
#ifndef _tcscat
#define _tcscat strcat
#endif
#ifndef _tcscmp
#define _tcscmp strcmp
#endif
#ifndef _tcsncmp
#define _tcsncmp strncmp
#endif
#ifndef _tcsicmp
#define _tcsicmp strcasecmp
#endif
#ifndef _tcsnicmp
#define _tcsnicmp strncasecmp
#endif
#ifndef _stprintf
#define _stprintf sprintf
#endif
#ifndef _sntprintf
#define _sntprintf snprintf
#endif
#ifndef _tfopen
#define _tfopen fopen
#endif

#endif // WWLIB_TCHAR_LINUX_SHIM
