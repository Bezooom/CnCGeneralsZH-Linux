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
 *                     $Archive:: /Commando/Code/wwlib/win.h                                  $* 
 *                                                                                             * 
 *                      $Author:: Ian_l                                                       $*
 *                                                                                             * 
 *                     $Modtime:: 10/16/01 2:42p                                              $*
 *                                                                                             * 
 *                    $Revision:: 11                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------* 
 * Functions:                                                                                  * 
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef WIN_H
#define WIN_H

/*
**	This header file includes the Windows headers. If there are any special pragmas that need
**	to occur around this process, they are performed here. Typically, certain warnings will need
**	to be disabled since the Windows headers are repleat with illegal and dangerous constructs.
**
**	Within the windows headers themselves, Microsoft has disabled the warnings 4290, 4514, 
**	4069, 4200, 4237, 4103, 4001, 4035, 4164. Makes you wonder, eh?
*/

// When including windows, lets just bump the warning level back to 3...
#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma warning(push, 3)
#endif

// this define should also be in the DSP just in case someone includes windows stuff directly
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// MSVC-specific macros not available on GCC/Clang
#ifndef __max
#define __max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef __min
#define __min(a,b) ((a)<(b)?(a):(b))
#endif

#if defined(PLATFORM_LINUX) || defined(_UNIX) || defined(__linux__)
 // Linux: windows.h does not exist. Base Win32 types (DWORD/WORD/BYTE/BOOL/
 // UINT) come from bittype.h; provide only the handle/object types missing
 // there. Extend only with proven-missing types.
 // MSVC-specific names that map to POSIX equivalents on Linux.
  #define _strdup strdup
  #define lstrcpyn(dest, src, len) strncpy(dest, src, len)
  #define lstrcat(dest, src) strcat(dest, src)
#ifndef __stdcall
#define __stdcall
#endif
#ifndef _stdcall
#define _stdcall __stdcall
#endif
#ifndef __cdecl
#define __cdecl
#endif
#ifndef _cdecl
#define _cdecl __cdecl
#endif
typedef void* LPWAVEFORMAT;
 #include "bittype.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
#include <errno.h>
#include <time.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <semaphore.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <fnmatch.h>
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
static inline void ZeroMemory(void* ptr, size_t size) { memset(ptr, 0, size); }
// GetLastError() call sites survive into the _UNIX branches (rawfile error
// paths); errno is its POSIX twin.
static inline DWORD GetLastError(void) { return (DWORD)errno; }
// GetTickCount() seeds srandom and times cpudetect; monotonic ms is its twin.
static inline DWORD GetTickCount(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (DWORD)((ts.tv_sec * 1000ULL) + (ts.tv_nsec / 1000000ULL));
}
#define GetCurrentTime GetTickCount
typedef void VOID;
// MSVC CRT string-compare names used across WWLib and GameEngine.
#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif
// MSVC low-level I/O names used by LocalFile.cpp and others.
#ifndef _open
#define _open open
#endif
#ifndef _close
#define _close close
#endif
#ifndef _read
#define _read read
#endif
#ifndef _write
#define _write write
#endif
#ifndef _lseek
#define _lseek lseek
#endif
#ifndef _O_RDONLY
#define _O_RDONLY O_RDONLY
#endif
#ifndef _O_WRONLY
#define _O_WRONLY O_WRONLY
#endif
#ifndef _O_RDWR
#define _O_RDWR O_RDWR
#endif
#ifndef _O_CREAT
#define _O_CREAT O_CREAT
#endif
#ifndef _O_TRUNC
#define _O_TRUNC O_TRUNC
#endif
#ifndef _O_APPEND
#define _O_APPEND O_APPEND
#endif
#ifndef _O_BINARY
#define _O_BINARY 0
#endif
#ifndef _O_TEXT
#define _O_TEXT 0
#endif
#ifndef WWLIB_WIN_LINUX_TYPES
#define WWLIB_WIN_LINUX_TYPES 1
typedef void* HANDLE;
typedef void* HINSTANCE;
typedef void* HWND;
typedef void* HMODULE;
// Opaque GDI handles for declarations only; no Linux GDI implementation.
typedef struct HFONT__ *HFONT;
typedef struct HBITMAP__ *HBITMAP;
typedef struct HDC__ *HDC;
// Dynamic loading (ScriptEngine's dev-debug dialogs: DebugWindow.dll,
// ParticleEditor.dll, vtuneapi.dll - Windows-only tools, absent on Linux,
// so these resolve to NULL and the call sites skip them).
typedef void (*FARPROC)(void);
static inline HMODULE LoadLibrary(const char *file) { return (HMODULE)dlopen(file, RTLD_NOW); }
static inline FARPROC GetProcAddress(HMODULE mod, const char *name) { return (FARPROC)dlsym(mod, name); }
static inline BOOL FreeLibrary(HMODULE mod) { return dlclose(mod) == 0 ? TRUE : FALSE; }
typedef int32_t LONG;
typedef long HRESULT;
// WPARAM/LPARAM for window messages (pointer-sized on LP64)
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef wchar_t WCHAR;
typedef char CHAR;
typedef CHAR *LPSTR;
typedef const CHAR *LPCSTR;
typedef const CHAR *LPCTSTR;
typedef WCHAR *LPWSTR;
typedef const WCHAR *LPCWSTR;
static inline int lstrlenA(LPCSTR s) { return s ? (int)strlen(s) : 0; }
static inline LPSTR lstrcpyA(LPSTR dst, LPCSTR src) { return strcpy(dst, src); }
static inline int lstrcmpiA(LPCSTR s1, LPCSTR s2) { return strcasecmp(s1, s2); }
static inline int lstrlen(LPCSTR s) { return lstrlenA(s); }
static inline LPSTR lstrcpy(LPSTR dst, LPCSTR src) { return lstrcpyA(dst, src); }
static inline int lstrcmpi(LPCSTR s1, LPCSTR s2) { return lstrcmpiA(s1, s2); }

#ifndef RGB
#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#endif
#ifndef COLORREF
typedef DWORD COLORREF;
#endif
typedef void* HGDIOBJ;
typedef struct tagTEXTMETRICA {
  LONG tmHeight;
  LONG tmAscent;
  LONG tmDescent;
  LONG tmInternalLeading;
  LONG tmExternalLeading;
  LONG tmAveCharWidth;
  LONG tmMaxCharWidth;
  LONG tmWeight;
  LONG tmOverhang;
  LONG tmDigitizedAspectX;
  LONG tmDigitizedAspectY;
  BYTE tmFirstChar;
  BYTE tmLastChar;
  BYTE tmDefaultChar;
  BYTE tmBreakChar;
  BYTE tmItalic;
  BYTE tmUnderlined;
  BYTE tmStruckOut;
  BYTE tmPitchAndFamily;
  BYTE tmCharSet;
} TEXTMETRIC, TEXTMETRICA;
static inline BOOL DeleteDC(HDC) { return TRUE; }
static inline BOOL DeleteObject(HGDIOBJ) { return TRUE; }
static inline HGDIOBJ SelectObject(HDC, HGDIOBJ) { return nullptr; }
static inline BOOL GetTextMetricsA(HDC, TEXTMETRICA* tm) { if (tm) memset(tm, 0, sizeof(*tm)); return TRUE; }
#define GetTextMetrics GetTextMetricsA
static inline COLORREF SetTextColor(HDC, COLORREF) { return 0; }
static inline COLORREF SetBkColor(HDC, COLORREF) { return 0; }
// SYSTEMTIME for GameState/Recorder timestamps (wYear..wMilliseconds,
// same field order as Win32).
typedef struct {
  WORD wYear, wMonth, wDayOfWeek, wDay;
  WORD wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME;
static inline void GetLocalTime(SYSTEMTIME *out)
{
  struct timespec tsNow;
  clock_gettime(CLOCK_REALTIME, &tsNow);
  struct tm tmNow;
  localtime_r(&tsNow.tv_sec, &tmNow);
  out->wYear = (WORD)(tmNow.tm_year + 1900);
  out->wMonth = (WORD)(tmNow.tm_mon + 1);
  out->wDayOfWeek = (WORD)tmNow.tm_wday;
  out->wDay = (WORD)tmNow.tm_mday;
  out->wHour = (WORD)tmNow.tm_hour;
  out->wMinute = (WORD)tmNow.tm_min;
  out->wSecond = (WORD)tmNow.tm_sec;
  out->wMilliseconds = (WORD)(tsNow.tv_nsec / 1000000);
}
// CopyFile used by Recorder/asset code; POSIX copy with fail-if-exists
// semantics matching CopyFile(..., TRUE).
static inline BOOL CopyFile(const char *from, const char *to, BOOL failIfExists)
{
  int srcFd = open(from, O_RDONLY);
  if (srcFd < 0)
    return FALSE;
  int flags = O_WRONLY | O_CREAT | (failIfExists ? O_EXCL : O_TRUNC);
  int dstFd = open(to, flags, 0644);
  if (dstFd < 0)
  {
    close(srcFd);
    return FALSE;
  }
  char buf[8192];
  ssize_t n;
  BOOL ok = TRUE;
  while ((n = read(srcFd, buf, sizeof(buf))) > 0)
  {
    ssize_t w = 0;
    while (w < n)
    {
      ssize_t k = write(dstFd, buf + w, (size_t)(n - w));
      if (k <= 0) { ok = FALSE; break; }
      w += k;
    }
    if (!ok)
      break;
  }
  if (n < 0)
    ok = FALSE;
close(srcFd);
  close(dstFd);
  return ok;
}

// CRITICAL_SECTION for GameEngine's CriticalSection.h; thin wrapper
// around pthread_mutex_t with the same semantics (non-recursive by
// default, but InitializeCriticalSection/DeleteCriticalSection/
// EnterCriticalSection/LeaveCriticalSection map 1:1).
typedef pthread_mutex_t CRITICAL_SECTION;
static inline void InitializeCriticalSection(CRITICAL_SECTION *cs)
{
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);
  pthread_mutex_init(cs, &attr);
  pthread_mutexattr_destroy(&attr);
}
static inline void DeleteCriticalSection(CRITICAL_SECTION *cs)
{
  pthread_mutex_destroy(cs);
}
static inline void EnterCriticalSection(CRITICAL_SECTION *cs)
{
  pthread_mutex_lock(cs);
}
static inline void LeaveCriticalSection(CRITICAL_SECTION *cs)
{
  pthread_mutex_unlock(cs);
}

// WinSock 1.1 types and functions used by ControlServer.cpp and
// GameSpy SDK.  POSIX sockets have the same constants and very similar
// signatures; map 1:1 where possible.
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
typedef struct hostent HOSTENT;
#define WSABASEERR 10000
#define WSAEINTR EINTR
// Constants like SOCK_STREAM, IPPROTO_TCP, AF_INET, INADDR_LOOPBACK,
// INADDR_ANY, FIONBIO, SOMAXCONN are already defined by <sys/socket.h>
// and <netinet/in.h> on Linux.
#define FD_READ 1
#define FD_WRITE 2
#define FD_OOB 4
#define FD_ACCEPT 8
#define FD_CONNECT 16
#define FD_CLOSE 32

// fd_set/select: use the system <sys/select.h> bitmask API as-is.  (An
// earlier revision redefined FD_SET/FD_ZERO around an array type; that
// broke TUs like FTP.CPP that use real POSIX select(), and the redirect
// of select() to a stub would have broken them silently at runtime.)
#define MAKEWORD(low, high) ((unsigned short)(((unsigned char)(low)) | ((unsigned short)((unsigned char)(high)) << 8)))
// Byte/word extraction used by DownloadManager's version packing.
#define LOBYTE(w) ((unsigned char)(w))
#define HIBYTE(w) ((unsigned char)(((unsigned short)(w) >> 8) & 0xFF))
#define LOWORD(d) ((unsigned short)(d))
#define HIWORD(d) ((unsigned short)(((unsigned int)(d) >> 16) & 0xFFFF))

static inline int WSAStartup(unsigned short version, void *data) { (void)version; (void)data; return 0; }
static inline int WSACleanup(void) { return 0; }
static inline int WSAGetLastError(void) { return errno; }
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEINPROGRESS EINPROGRESS
#define WSAEALREADY EALREADY
#define WSAENOTSOCK ENOTSOCK
#define WSAEDESTADDRREQ EDESTADDRREQ
#define WSAEMSGSIZE EMSGSIZE
#define WSAEPROTOTYPE EPROTOTYPE
#define WSAENOPROTOOPT ENOPROTOOPT
#define WSAEPROTONOSUPPORT EPROTONOSUPPORT
#define WSAESOCKTNOSUPPORT ESOCKTNOSUPPORT
#define WSAEOPNOTSUPP EOPNOTSUPP
#define WSAEPFNOSUPPORT EPFNOSUPPORT
#define WSAEAFNOSUPPORT EAFNOSUPPORT
#define WSAEADDRINUSE EADDRINUSE
#define WSAEADDRNOTAVAIL EADDRNOTAVAIL
#define WSAENETDOWN ENETDOWN
#define WSAENETUNREACH ENETUNREACH
#define WSAENETRESET ENETRESET
#define WSAECONNABORTED ECONNABORTED
#define WSAECONNRESET ECONNRESET
#define WSAENOBUFS ENOBUFS
#define WSAEISCONN EISCONN
#define WSAENOTCONN ENOTCONN
#define WSAESHUTDOWN ESHUTDOWN
#define WSAETOOMANYREFS ETOOMANYREFS
#define WSAETIMEDOUT ETIMEDOUT
#define WSAECONNREFUSED ECONNREFUSED
#define WSAELOOP ELOOP
#define WSAENAMETOOLONG ENAMETOOLONG
#define WSAEHOSTDOWN EHOSTDOWN
#define WSAEHOSTUNREACH EHOSTUNREACH
#define WSAENOTEMPTY ENOTEMPTY
#define WSAEDQUOT EDQUOT
#define WSAESTALE ESTALE
#define WSAEBADF EBADF
#define WSAEACCES EACCES
#define WSAEFAULT EFAULT
#define WSAEINVAL EINVAL
#define WSAEMFILE EMFILE
// No errno twin on Linux; real Windows values, used only as distinct
// constants (e.g. GameResultsThread's error-name switch).
#define WSAEPROCLIM 10067
#define WSAEUSERS 10068
#define WSAEREMOTE 10071
#define WSAEDISCON 10101
#define WSASYSNOTREADY 10091
#define WSAVERNOTSUPPORTED 10092
#define WSANOTINITIALISED 10093
#define WSAHOST_NOT_FOUND 11001
#define WSATRY_AGAIN 11002
#define WSANO_RECOVERY 11003
#define WSANO_DATA 11004

// These wrappers avoid name clashes with system functions while providing
// the Windows signatures (int for length args, SOCKET type).
static inline SOCKET win_socket(int domain, int type, int protocol) { return ::socket(domain, type, protocol); }
#define socket win_socket
#ifdef closesocket
#undef closesocket
#endif
static inline int closesocket(SOCKET s) { return close(s); }
static inline int win_bind(SOCKET s, const struct sockaddr *addr, int addrlen) { return ::bind(s, addr, (socklen_t)addrlen); }
#define bind win_bind
static inline int win_listen(SOCKET s, int backlog) { return ::listen(s, backlog); }
#define listen win_listen
static inline SOCKET win_accept(SOCKET s, struct sockaddr *addr, int *addrlen) { return ::accept(s, addr, (socklen_t*)addrlen); }
#define accept win_accept
static inline int win_connect(SOCKET s, const struct sockaddr *addr, int addrlen) { return ::connect(s, addr, (socklen_t)addrlen); }
#define connect win_connect
static inline int win_send(SOCKET s, const char *buf, int len, int flags) { return ::send(s, buf, (size_t)len, flags); }
#define send win_send
static inline int win_recv(SOCKET s, char *buf, int len, int flags) { return ::recv(s, buf, (size_t)len, flags); }
#define recv win_recv
static inline int win_ioctlsocket(SOCKET s, long cmd, unsigned long *argp) { return ioctl(s, cmd, argp); }
#define ioctlsocket win_ioctlsocket
// select(): use the real POSIX select() with the system fd_set.  ControlServer
// uses non-blocking I/O without select(); FTP.CPP and the GameSpy SDK need
// the real one.
// htons/htonl/ntohs/ntohl/inet_addr/inet_ntoa/gethostbyname are POSIX; keep names.

typedef struct { unsigned short wVersion; unsigned short wHighVersion; char szDescription[257]; char szSystemStatus[129]; unsigned short iMaxSockets; unsigned short iMaxUdpDg; char *lpVendorInfo; } WSADATA;

// SNMP MIB-II types (StagingRoomGameInfo reads the TCP table through
// INETMIB1.DLL on Windows to find the LAN address facing the chat server).
// The DLL is absent on Linux so LoadLibrary fails and that block returns
// early; these declarations exist only so it compiles.
typedef long AsnInteger32;
typedef long AsnInteger;
typedef struct {
  unsigned idLength;
  unsigned *ids;
} AsnObjectIdentifier;
typedef struct {
  unsigned char *stream;
  unsigned length;
} AsnOctetString;
typedef struct {
  unsigned char asnType;
  union {
    AsnInteger32 number;
    AsnOctetString string;
    AsnOctetString address;
    AsnObjectIdentifier object;
  } asnValue;
} AsnObjectSyntax;
typedef struct {
  AsnObjectIdentifier name;
  AsnObjectSyntax value;
} RFC1157VarBind;
typedef RFC1157VarBind SnmpVarBind;
typedef struct {
  RFC1157VarBind *list;
  unsigned len;
} RFC1157VarBindList;
typedef RFC1157VarBindList SnmpVarBindList;
#define SNMP_PDU_GETNEXT 160

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef _MAX_PATH
#define _MAX_PATH MAX_PATH
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
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((void*)-1)
#endif
// DebugBreak() triggers the debugger; __builtin_trap is its compiler twin.
static inline void DebugBreak(void) { __builtin_trap(); }

// GetKeyboardLayout: Keyboard.cpp reads the low word to detect French
// AZERTY for key names; the C locale's language tag is the POSIX twin
// (0x0409 US English default, 0x040c French).
typedef void *HKL;
static inline HKL GetKeyboardLayout(DWORD idThread)
{
  (void)idThread;
  const char *loc = setlocale(LC_CTYPE, NULL);
  if (loc && (loc[0] == 'f' || loc[0] == 'F') && (loc[1] == 'r' || loc[1] == 'R'))
    return (HKL)(uintptr_t)0x040c;
  return (HKL)(uintptr_t)0x0409;
}

// GetCommandLineW returns the wide command line (argv[0] + args) as a
// single UTF-16 string.  /proc/self/cmdline carries the same bytes the
// kernel gave main(), NUL-separated; joining them with spaces reproduces
// the flat command line EarlyCommandLine parses.
static inline const wchar_t *GetCommandLineW(void)
{
  static wchar_t *cached = NULL;
  if (cached)
    return cached;
  FILE *fp = fopen("/proc/self/cmdline", "rb");
  if (!fp)
    return L"";
  char utf8[8192];
  size_t n = fread(utf8, 1, sizeof(utf8) - 1, fp);
  fclose(fp);
  // NUL separators become spaces; the trailing NUL becomes the terminator.
  for (size_t i = 0; i < n; ++i)
  {
    if (utf8[i] == 0)
      utf8[i] = ' ';
  }
  while (n > 0 && utf8[n - 1] == ' ')
    --n;
  utf8[n] = 0;
  size_t wlen = mbstowcs(NULL, utf8, 0);
  if (wlen == (size_t)-1)
    return L"";
  cached = (wchar_t *)malloc((wlen + 1) * sizeof(wchar_t));
  if (!cached)
    return L"";
  mbstowcs(cached, utf8, wlen + 1);
  return cached;
}

// MessageBox and related constants used by Debug.cpp and other UI code.
// Headless/stub implementation: logs to stderr and returns IDIGNORE/IDYES
// based on the button set, so unattented runs never block.
#define MB_OK 0x00000000
#define MB_OKCANCEL 0x00000001
#define MB_ABORTRETRYIGNORE 0x00000002
#define MB_YESNOCANCEL 0x00000003
#define MB_YESNO 0x00000004
#define MB_RETRYCANCEL 0x00000005
#define MB_CANCELTRYCONTINUE 0x00000006
#define MB_ICONHAND 0x00000010
#define MB_ICONQUESTION 0x00000020
#define MB_ICONEXCLAMATION 0x00000030
#define MB_ICONASTERISK 0x00000040
#define MB_ICONWARNING MB_ICONEXCLAMATION
#define MB_ICONERROR MB_ICONHAND
#define MB_ICONINFORMATION MB_ICONASTERISK
#define MB_ICONSTOP MB_ICONHAND
#define MB_DEFBUTTON1 0x00000000
#define MB_DEFBUTTON2 0x00000100
#define MB_DEFBUTTON3 0x00000200
#define MB_DEFBUTTON4 0x00000300
#define MB_APPLMODAL 0x00000000
#define MB_SYSTEMMODAL 0x00001000
#define MB_TASKMODAL 0x00002000
#define MB_NOFOCUS 0x00008000
#define MB_SETFOREGROUND 0x00010000
#define MB_DEFAULT_DESKTOP_ONLY 0x00020000
#define MB_TOPMOST 0x00040000
#define MB_RIGHT 0x00080000
#define MB_RTLREADING 0x00100000

#define IDOK 1
#define IDCANCEL 2
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
#define IDYES 6
#define IDNO 7
#define IDCLOSE 8
#define IDHELP 9
#define IDTRYAGAIN 10
#define IDCONTINUE 11

// HWND and related for Debug.cpp
typedef void *HWND;
#define NULL_HWND ((HWND)0)
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_SHOWNOACTIVATE 4
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_SHOWMINNOACTIVE 7
#define SW_SHOWNA 8
#define SW_RESTORE 9
#define SW_SHOWDEFAULT 10
#define SW_MAX 10

static inline int ShowWindow(HWND hWnd, int nCmdShow) { (void)hWnd; (void)nCmdShow; return 1; }
// Display size for GetSystemMetrics (borderless-window tests assert the
// parsed resolution equals these; any fixed pair is self-consistent).
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
static inline int GetSystemMetrics(int index)
{
  if (index == SM_CXSCREEN)
    return 1920;
  if (index == SM_CYSCREEN)
    return 1080;
  return 0;
}
static inline BOOL SetWindowText(HWND hWnd, const char *text) { (void)hWnd; (void)text; return TRUE; }
static inline BOOL SetWindowTextW(HWND hWnd, const wchar_t *text) { (void)hWnd; (void)text; return TRUE; }

// Font registration (GlobalLanguage adds/removes game fonts; fontconfig owns
// fonts on Linux, so these are inert - AddFontResource returns nonzero so
// the caller's == 0 failure assert never fires).
static inline int AddFontResource(const char *file) { (void)file; return 1; }
static inline int AddFontResourceW(const wchar_t *file) { (void)file; return 1; }
static inline BOOL RemoveFontResource(const char *file) { (void)file; return TRUE; }
static inline BOOL RemoveFontResourceW(const wchar_t *file) { (void)file; return TRUE; }

// MEMORYSTATUS/GlobalMemoryStatus (GameClient::preloadAssets logs the delta;
// sysinfo(2) is the POSIX twin).
typedef struct {
  DWORD dwLength;
  DWORD dwMemoryLoad;
  DWORD dwTotalPhys;
  DWORD dwAvailPhys;
  DWORD dwTotalPageFile;
  DWORD dwAvailPageFile;
  DWORD dwTotalVirtual;
  DWORD dwAvailVirtual;
} MEMORYSTATUS;
static inline void GlobalMemoryStatus(MEMORYSTATUS *out)
{
  struct sysinfo si;
  sysinfo(&si);
  unsigned long long total = (unsigned long long)si.totalram * si.mem_unit;
  unsigned long long avail = (unsigned long long)si.freeram * si.mem_unit;
  out->dwLength = sizeof(MEMORYSTATUS);
  out->dwTotalPhys = (DWORD)total;
  out->dwAvailPhys = (DWORD)avail;
  out->dwMemoryLoad = total ? (DWORD)((total - avail) * 100 / total) : 0;
  out->dwTotalPageFile = 0;
  out->dwAvailPageFile = 0;
  out->dwTotalVirtual = (DWORD)total;
  out->dwAvailVirtual = (DWORD)avail;
}
static inline int MessageBox(HWND hWnd, const char *lpText, const char *lpCaption, unsigned int uType)
{
  (void)hWnd;
  fprintf(stderr, "MessageBox: %s - %s\n", lpCaption ? lpCaption : "", lpText ? lpText : "");
  if (uType & MB_ABORTRETRYIGNORE)
    return IDIGNORE;
  return IDYES;
}
static inline int MessageBoxA(HWND hWnd, const char *lpText, const char *lpCaption, unsigned int uType)
{
  return MessageBox(hWnd, lpText, lpCaption, uType);
}
static inline int MessageBoxW(HWND hWnd, const wchar_t *lpText, const wchar_t *lpCaption, unsigned int uType)
{
  (void)hWnd;
  fprintf(stderr, "MessageBoxW: %ls - %ls\n", lpCaption ? lpCaption : L"", lpText ? lpText : L"");
  if (uType & MB_ABORTRETRYIGNORE)
    return IDIGNORE;
  return IDYES;
}
#ifndef HWND_NOTOPMOST
#define HWND_NOTOPMOST ((HWND)-2)
#endif
#ifndef SWP_NOSIZE
#define SWP_NOSIZE 0x0001
#endif
#ifndef SWP_NOMOVE
#define SWP_NOMOVE 0x0002
#endif
static inline BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT) { return TRUE; }
#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER {
  WORD bfType;
  DWORD bfSize;
  WORD bfReserved1;
  WORD bfReserved2;
  DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER, *LPBITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagBITMAPINFOHEADER {
  DWORD biSize;
  LONG biWidth;
  LONG biHeight;
  WORD biPlanes;
  WORD biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG biXPelsPerMeter;
  LONG biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER, *LPBITMAPINFOHEADER;

#ifndef BI_RGB
#define BI_RGB 0L
#endif
typedef struct tagRGBQUAD {
  BYTE rgbBlue;
  BYTE rgbGreen;
  BYTE rgbRed;
  BYTE rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO {
  BITMAPINFOHEADER bmiHeader;
  RGBQUAD bmiColors[1];
} BITMAPINFO, *PBITMAPINFO, *LPBITMAPINFO;

#define DIB_RGB_COLORS 0

#define FW_NORMAL 400
#define FW_BOLD   700

#define DEFAULT_CHARSET     1
#define OUT_DEFAULT_PRECIS  0
#define CLIP_DEFAULT_PRECIS 0
#define ANTIALIASED_QUALITY 4
#define VARIABLE_PITCH      2

typedef struct HFONT__ *HFONT;
static inline HFONT CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR) { return nullptr; }
#ifndef CreateFont
#define CreateFont CreateFontA
#endif

static inline HBITMAP CreateDIBSection(HDC, const BITMAPINFO* pbmi, UINT, void** ppvBits, HANDLE, DWORD) {
	int w = (pbmi && pbmi->bmiHeader.biWidth > 0) ? pbmi->bmiHeader.biWidth : 256;
	int h = (pbmi && pbmi->bmiHeader.biHeight > 0) ? pbmi->bmiHeader.biHeight : 256;
	int stride = (((w * 3) + 3) & ~3);
	void* bits = calloc(1, stride * h + 1024);
	if (ppvBits) {
		*ppvBits = bits;
	}
	return (HBITMAP)bits;
}
static inline HDC CreateCompatibleDC(HDC) { return nullptr; }

#ifndef _RECT_DEFINED
#define _RECT_DEFINED
typedef struct tagRECT {
  LONG left;
  LONG top;
  LONG right;
  LONG bottom;
} RECT, *PRECT, *LPRECT;
#endif

static inline BOOL GetWindowRect(HWND, LPRECT lpRect)
{
  if (lpRect) {
    lpRect->left = 0;
    lpRect->top = 0;
    lpRect->right = 800;
    lpRect->bottom = 600;
  }
  return TRUE;
}
#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID {
  unsigned long  Data1;
  unsigned short Data2;
  unsigned short Data3;
  unsigned char  Data4[8];
} GUID;
#ifdef __cplusplus
static inline bool operator==(const _GUID& a, const _GUID& b) {
  return __builtin_memcmp(&a, &b, sizeof(_GUID)) == 0;
}
static inline bool operator!=(const _GUID& a, const _GUID& b) {
  return !(a == b);
}
#endif
#endif

typedef struct tagSIZE {
  LONG cx;
  LONG cy;
} SIZE, *PSIZE, *LPSIZE;

#define ETO_OPAQUE 0x0002

static inline BOOL ExtTextOutW(HDC, int, int, UINT, const RECT*, const wchar_t*, UINT, const int*) { return TRUE; }
static inline BOOL GetTextExtentPoint32W(HDC, const wchar_t*, int, SIZE* psz) { if (psz) { psz->cx = 8; psz->cy = 16; } return TRUE; }
static inline HDC GetDC(HWND) { return nullptr; }
static inline int ReleaseDC(HWND, HDC) { return 1; }
static inline HWND GetDesktopWindow() { return nullptr; }
static inline BOOL SetDeviceGammaRamp(HDC, void*) { return TRUE; }
#ifndef MulDiv
#define MulDiv MulDiv
static inline int MulDiv(int nNumber, int nNumerator, int nDenominator) {
  if (nDenominator == 0) return -1;
  return (int)(((int64_t)nNumber * (int64_t)nNumerator) / nDenominator);
}
#endif

#define GWL_STYLE (-16)
#ifndef HWND_TOPMOST
#define HWND_TOPMOST ((HWND)(intptr_t)-1)
#endif
#ifndef HWND_TOP
#define HWND_TOP ((HWND)0)
#endif
#define SWP_NOZORDER 0x0004
#define SWP_NOACTIVATE 0x0010

static inline LONG GetWindowLong(HWND, int) { return 0; }
static inline BOOL AdjustWindowRect(LPRECT, DWORD, BOOL) { return TRUE; }
static inline BOOL GetClientRect(HWND, LPRECT lpRect) {
  if (lpRect) {
    lpRect->left = 0;
    lpRect->top = 0;
    lpRect->right = 800;
    lpRect->bottom = 600;
  }
  return TRUE;
}
#define SEM_FAILCRITICALERRORS 0x0001
static inline UINT SetErrorMode(UINT) { return 0; }
static inline BOOL IsIconic(HWND) { return FALSE; }
#ifndef _POINT_DEFINED
#define _POINT_DEFINED
typedef struct tagPOINT {
  LONG x;
  LONG y;
} POINT, *PPOINT, *LPPOINT;
#endif

typedef intptr_t LONG_PTR;
typedef intptr_t LRESULT;

typedef struct tagMSG {
  HWND   hwnd;
  UINT   message;
  WPARAM wParam;
  LPARAM lParam;
  DWORD  time;
  POINT  pt;
} MSG, *PMSG, *LPMSG;

#define PM_NOREMOVE 0x0000
#define PM_REMOVE   0x0001

static inline BOOL PeekMessageA(LPMSG, HWND, UINT, UINT, UINT) { return FALSE; }
static inline BOOL GetMessageA(LPMSG, HWND, UINT, UINT) { return FALSE; }
static inline BOOL TranslateMessage(const MSG*) { return TRUE; }
static inline LRESULT DispatchMessageA(const MSG*) { return 0; }
#ifndef PeekMessage
#define PeekMessage PeekMessageA
#endif
#ifndef GetMessage
#define GetMessage GetMessageA
#endif
#ifndef DispatchMessage
#define DispatchMessage DispatchMessageA
#endif

// Window class, window creation, and the messages the game's own WndProc switches
// on.  There is no window manager to talk to on Linux, so the window is an opaque
// handle and every call here is inert; Phase 3.1 (SDL3) replaces this block with
// real handles.  Only the messages WndProc actually handles are defined - the
// DEBUG_WINDOWS_MESSAGES name table is off by default and would need the rest.
#define APIENTRY
#define CALLBACK
#define WINAPI

#ifndef TEXT
#define TEXT(x) (x)
#endif
#ifndef MAKEINTRESOURCE
#define MAKEINTRESOURCE(id) ((const char*)(intptr_t)(id))
#endif

typedef void* HMENU;
typedef struct HICON__ *HICON;
typedef struct HCURSOR__ *HCURSOR;
typedef struct HBRUSH__ *HBRUSH;
extern HWND ApplicationHWnd;
static inline BOOL ScreenToClient(HWND, LPPOINT) { return TRUE; }
static inline HWND WindowFromPoint(POINT) { return ApplicationHWnd; }
static inline HCURSOR LoadCursorFromFileA(LPCSTR) { return (HCURSOR)(intptr_t)1; }
#ifndef LoadCursorFromFile
#define LoadCursorFromFile LoadCursorFromFileA
#endif
static inline HCURSOR SetCursor(HCURSOR) { return nullptr; }

typedef struct tagWNDCLASSA {
  UINT style;
  LRESULT (*lpfnWndProc)(HWND, UINT, WPARAM, LPARAM);
  int cbClsExtra;
  int cbWndExtra;
  HINSTANCE hInstance;
  HICON hIcon;
  HCURSOR hCursor;
  HBRUSH hBackground;
  LPCSTR lpszMenuName;
  LPCSTR lpszClassName;
} WNDCLASSA, *LPWNDCLASSA;
#ifndef WNDCLASS
#define WNDCLASS WNDCLASSA
#endif

#define CS_HREDRAW 0x0001
#define CS_VREDRAW 0x0002
#define CS_DBLCLKS 0x0008

#define WS_CAPTION 0x00C00000
#define WS_SYSMENU 0x00080000
#define WS_DLGFRAME 0x00400000
#define WS_MINIMIZEBOX 0x00020000
#define WS_POPUP 0x80000000
#define WS_VISIBLE 0x10000000
#define WS_EX_TOPMOST 0x00000008

#define HTCLIENT 2
#define WA_INACTIVE 0
#define SC_KEYMENU 0xF100
#define SC_MOVE 0xF010
#define SC_SIZE 0xF120
#define SC_MAXIMIZE 0xF030
#define SC_MONITORPOWER 0xF170

#define WM_NCHITTEST 0x0084
#define WM_POWERBROADCAST 0x0218
#define WM_SYSCOMMAND 0x0111
#define WM_QUERYENDSESSION 0x0011
#define WM_CLOSE 0x0010
#define WM_SETFOCUS 0x0007
#define WM_SIZE 0x0005
#define WM_KILLFOCUS 0x0008
#define WM_ACTIVATEAPP 0x001C
#define WM_ACTIVATE 0x0006
#define WM_KEYDOWN 0x0100
#define WM_MOUSEMOVE 0x0200
#define WM_MOUSEWHEEL 0x020A
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MBUTTONDBLCLK 0x0209
#define WM_SETCURSOR 0x0020
#define WM_PAINT 0x000F
#define WM_ERASEBKGND 0x0014

static inline BOOL RegisterClass(const WNDCLASS*) { return TRUE; }
static inline HWND CreateWindow(LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, void*) { return nullptr; }
static inline LRESULT DefWindowProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
static inline void PostQuitMessage(int nExitCode) { (void)nExitCode; }
static inline HWND FindWindow(LPCSTR, LPCSTR) { return nullptr; }
static inline HWND SetForegroundWindow(HWND) { return nullptr; }
static inline BOOL SetFocus(HWND) { return TRUE; }
static inline BOOL UpdateWindow(HWND) { return TRUE; }
static inline BOOL GetCursorPos(LPPOINT lpPoint) {
  if (lpPoint) { lpPoint->x = 0; lpPoint->y = 0; }
  return TRUE;
}
static inline BOOL PtInRect(const RECT* r, POINT p) {
  return r && p.x >= r->left && p.x <= r->right && p.y >= r->top && p.y <= r->bottom;
}
static inline BOOL ClipCursor(const RECT*) { return TRUE; }

// Icons and stock brushes are resource lookups with no Linux equivalent here: the
// window class fields they fill are never read.
static inline HICON LoadIcon(HINSTANCE, LPCSTR) { return nullptr; }
#define BLACK_BRUSH 4
static inline HGDIOBJ GetStockObject(int) { return nullptr; }

// The splash bitmap is loaded from a loose file next to the exe on Windows.  There
// is no image loader here, so the handle stays NULL and the WM_PAINT branch skips
// it - the window is never painted on Linux anyway.
#define IMAGE_BITMAP 0
#define LR_SHARED 0x00000010
#define LR_LOADFROMFILE 0x00000080
static inline HBITMAP LoadImage(HINSTANCE, LPCSTR, UINT, UINT, UINT, UINT) { return nullptr; }

typedef struct tagPAINTSTRUCT {
  HDC hdc;
  BOOL fErase;
  RECT rcPaint;
  BOOL fRestore;
  BOOL fIncUpdate;
  BYTE rgbReserved[32];
} PAINTSTRUCT, *PPAINTSTRUCT, *LPPAINTSTRUCT;
static inline HDC BeginPaint(HWND, LPPAINTSTRUCT ps) {
  if (ps) memset(ps, 0, sizeof(*ps));
  return nullptr;
}
static inline BOOL EndPaint(HWND, const PAINTSTRUCT*) { return TRUE; }
static inline int SaveDC(HDC) { return 1; }
static inline BOOL RestoreDC(HDC, int) { return TRUE; }
#define SRCCOPY 0x00CC0020
static inline BOOL BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD) { return TRUE; }

// The exe's own path: /proc/self/exe is the twin of GetModuleFileName(NULL, ...),
// which the entry point uses to put the working directory beside the .big files.
static inline DWORD GetModuleFileName(HMODULE, char* buf, DWORD size) {
  if (!buf || size == 0) return 0;
  ssize_t n = readlink("/proc/self/exe", buf, size);
  if (n <= 0) { buf[0] = 0; return 0; }
  if ((DWORD)n >= size) n = (DWORD)size - 1;
  buf[n] = 0;
  return (DWORD)n;
}

static inline DWORD GetCurrentThreadId(void) { return (DWORD)pthread_self(); }

// _spawnl(_P_NOWAIT, ...) launches an external tool without waiting (the
// MainMenu's WorldBuilder button).  fork+execvp is the POSIX twin; the
// child's pid plays the "process handle", -1 the launch failure.
#define _P_NOWAIT 1
static inline intptr_t _spawnl(int mode, const char *cmd, ...)
{
  (void)mode;
  const char *args[64];
  int n = 0;
  args[n++] = cmd;
  va_list ap;
  va_start(ap, cmd);
  const char *a = NULL;
  while (n < 63 && (a = va_arg(ap, const char *)) != NULL)
    args[n++] = a;
  va_end(ap);
  args[n] = NULL;
  pid_t pid = fork();
  if (pid == 0)
  {
    execvp(cmd, (char *const *)args);
    _exit(127);
  }
  return (pid > 0) ? (intptr_t)pid : -1;
}

// FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, ...) renders a GetLastError()
// code as text for the PopupReplay error boxes; strerror is its twin,
// converted to wide on the way out.
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000
static inline DWORD FormatMessageW(DWORD flags, const void *src, DWORD err, DWORD lang, wchar_t *buf, DWORD size, void *args)
{
  (void)flags; (void)src; (void)lang; (void)args;
  if (!buf || size == 0)
    return 0;
  const char *text = strerror((int)err);
  size_t n = mbstowcs(buf, text ? text : "", (size_t)size);
  if (n == (size_t)-1)
  {
    buf[0] = 0;
    return 0;
  }
  return (DWORD)n;
}

// ANSI twin of the above (ReplayMenu's delete-replay error box).
static inline DWORD FormatMessageA(DWORD flags, const void *src, DWORD err, DWORD lang, char *buf, DWORD size, void *args)
{
  (void)flags; (void)src; (void)lang; (void)args;
  if (!buf || size == 0)
    return 0;
  const char *text = strerror((int)err);
  strncpy(buf, text ? text : "", (size_t)size);
  buf[size - 1] = 0;
  return (DWORD)strlen(buf);
}
#ifndef FormatMessage
#define FormatMessage FormatMessageA
#endif

// Shell folder lookup used by ReplayMenu's copy-to-desktop.  $HOME/Desktop
// is the XDG-conventional twin of CSIDL_DESKTOPDIRECTORY.
#define CSIDL_DESKTOPDIRECTORY 0x0010
typedef void *LPITEMIDLIST;
static inline HRESULT SHGetSpecialFolderLocation(void *owner, int folder, LPITEMIDLIST *pidl)
{
  (void)owner; (void)folder;
  char *p = (char *)malloc(1024);
  if (!p)
    return (HRESULT)1;
  const char *home = getenv("HOME");
  if (!home || !*home)
    home = "/tmp";
  snprintf(p, 1024, "%s/Desktop", home);
  *pidl = (LPITEMIDLIST)p;
  return (HRESULT)0;
}
static inline BOOL SHGetPathFromIDList(LPITEMIDLIST pidl, char *path)
{
  if (!pidl || !path)
    return FALSE;
  strcpy(path, (const char *)pidl);
  return TRUE;
}

// _wtoi: wide atoi (WOLBuddyOverlay parses the location string with it).
static inline int _wtoi(const wchar_t *s) { return (int)wcstol(s, NULL, 10); }

// iswascii has no glibc wide twin (isascii is char-only); the Language.h
// GameIsAscii macro needs it.
static inline int iswascii(wint_t c) { return (c < 128) ? 1 : 0; }

// Charset conversion (ThreadUtils: UTF-8 <-> wide).  mbstowcs/wcstombs are
// the POSIX twins; -1 lengths mean NUL-terminated, as on Windows.
#define CP_ACP 0
#define CP_UTF8 65001
static inline int MultiByteToWideChar(unsigned codePage, DWORD flags, const char *src, int srcLen, wchar_t *dst, int dstLen)
{
  (void)codePage; (void)flags;
  if (!src || !dst || dstLen <= 0)
    return 0;
  size_t n;
  if (srcLen < 0)
    n = mbstowcs(dst, src, (size_t)dstLen);
  else
  {
    char *tmp = (char *)malloc((size_t)srcLen + 1);
    if (!tmp)
      return 0;
    memcpy(tmp, src, (size_t)srcLen);
    tmp[srcLen] = 0;
    n = mbstowcs(dst, tmp, (size_t)dstLen);
    free(tmp);
  }
  return (n == (size_t)-1) ? 0 : (int)n;
}
static inline int WideCharToMultiByte(unsigned codePage, DWORD flags, const wchar_t *src, int srcLen, char *dst, int dstLen, const char *defChar, int *usedDef)
{
  (void)codePage; (void)flags; (void)defChar; (void)usedDef;
  if (!src || !dst || dstLen <= 0)
    return 0;
  size_t n;
  if (srcLen < 0)
    n = wcstombs(dst, src, (size_t)dstLen);
  else
  {
    wchar_t *tmp = (wchar_t *)malloc(((size_t)srcLen + 1) * sizeof(wchar_t));
    if (!tmp)
      return 0;
    memcpy(tmp, src, (size_t)srcLen * sizeof(wchar_t));
    tmp[srcLen] = 0;
    n = wcstombs(dst, tmp, (size_t)dstLen);
    free(tmp);
  }
  return (n == (size_t)-1) ? 0 : (int)n;
}

// Virtual-key codes (GadgetTextEntry checks VK_RETURN); standard values.
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_CANCEL 0x03
#define VK_MBUTTON 0x04
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E

// Sleep(ms): ScoreScreen's pacing delay; nanosleep is the POSIX twin.
static inline void Sleep(DWORD ms)
{
  struct timespec ts;
  ts.tv_sec = (time_t)(ms / 1000);
  ts.tv_nsec = (long)((ms % 1000) * 1000000L);
  nanosleep(&ts, NULL);
}

// Windows locale/date/time formatting API used by GameState.cpp.
// Linux implementation uses strftime/wcsftime with the current locale.
#define LOCALE_USER_DEFAULT 0x0400
#define LOCALE_SYSTEM_DEFAULT 0x0800
#define VER_PLATFORM_WIN32_WINDOWS 1
#define DATE_SHORTDATE 0x00000001
#define TIME_NOSECONDS 0x00000002
#define TIME_FORCE24HOURFORMAT 0x00000008
#define TIME_NOTIMEMARKER 0x00000010

typedef struct {
  DWORD dwOSVersionInfoSize;
  DWORD dwMajorVersion;
  DWORD dwMinorVersion;
  DWORD dwBuildNumber;
  DWORD dwPlatformId;
  char szCSDVersion[128];
} OSVERSIONINFO;

static inline BOOL GetVersionEx(OSVERSIONINFO *lpVersionInformation)
{
  (void)lpVersionInformation;
  // Linux: pretend we're NT-based, not Win9x
  lpVersionInformation->dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  lpVersionInformation->dwMajorVersion = 5;
  lpVersionInformation->dwMinorVersion = 1;
  lpVersionInformation->dwBuildNumber = 0;
  lpVersionInformation->dwPlatformId = 2; // VER_PLATFORM_WIN32_NT
  return TRUE;
}

// GetDateFormatW / GetDateFormatA - format date using strftime
// Returns number of chars written (not including null), or 0 on error.
static inline int GetDateFormatW(unsigned int Locale, unsigned int dwFlags,
  const SYSTEMTIME *lpDate, const wchar_t *lpFormat,
  wchar_t *lpDateStr, int cchDate)
{
  (void)Locale; (void)dwFlags; (void)lpFormat;
  if (!lpDate || !lpDateStr || cchDate <= 0)
    return 0;
  struct tm tm = {0};
  tm.tm_year = (int)lpDate->wYear - 1900;
  tm.tm_mon = (int)lpDate->wMonth - 1;
  tm.tm_mday = (int)lpDate->wDay;
  size_t n = wcsftime(lpDateStr, (size_t)cchDate, L"%x", &tm);
  return (n > 0) ? (int)n : 0;
}

static inline int GetDateFormat(unsigned int Locale, unsigned int dwFlags,
  const SYSTEMTIME *lpDate, const char *lpFormat,
  char *lpDateStr, int cchDate)
{
  (void)Locale; (void)dwFlags; (void)lpFormat;
  if (!lpDate || !lpDateStr || cchDate <= 0)
    return 0;
  struct tm tm = {0};
  tm.tm_year = (int)lpDate->wYear - 1900;
  tm.tm_mon = (int)lpDate->wMonth - 1;
  tm.tm_mday = (int)lpDate->wDay;
  size_t n = strftime(lpDateStr, (size_t)cchDate, "%x", &tm);
  return (n > 0) ? (int)n : 0;
}

// GetTimeFormatW / GetTimeFormatA - format time using strftime
static inline int GetTimeFormatW(unsigned int Locale, unsigned int dwFlags,
  const SYSTEMTIME *lpTime, const wchar_t *lpFormat,
  wchar_t *lpTimeStr, int cchTime)
{
  (void)Locale; (void)dwFlags; (void)lpFormat;
  if (!lpTime || !lpTimeStr || cchTime <= 0)
    return 0;
  struct tm tm = {0};
  tm.tm_hour = (int)lpTime->wHour;
  tm.tm_min = (int)lpTime->wMinute;
  tm.tm_sec = (int)lpTime->wSecond;
  // Format as HH:MM (24-hour, no seconds, no AM/PM)
  const wchar_t *fmt = (dwFlags & TIME_FORCE24HOURFORMAT) ? L"%H:%M" : L"%I:%M %p";
  size_t n = wcsftime(lpTimeStr, (size_t)cchTime, fmt, &tm);
  return (n > 0) ? (int)n : 0;
}

static inline int GetTimeFormat(unsigned int Locale, unsigned int dwFlags,
  const SYSTEMTIME *lpTime, const char *lpFormat,
  char *lpTimeStr, int cchTime)
{
  (void)Locale; (void)dwFlags; (void)lpFormat;
  if (!lpTime || !lpTimeStr || cchTime <= 0)
    return 0;
  struct tm tm = {0};
  tm.tm_hour = (int)lpTime->wHour;
  tm.tm_min = (int)lpTime->wMinute;
  tm.tm_sec = (int)lpTime->wSecond;
  const char *fmt = (dwFlags & TIME_FORCE24HOURFORMAT) ? "%H:%M" : "%I:%M %p";
  size_t n = strftime(lpTimeStr, (size_t)cchTime, fmt, &tm);
  return (n > 0) ? (int)n : 0;
}

// _wcsnicmp -> wcsncasecmp (POSIX case-insensitive wide compare)
#ifndef _wcsnicmp
#define _wcsnicmp wcsncasecmp
#endif
#ifndef wcsicmp
#define wcsicmp wcscasecmp
#endif

// _isnan -> isnan (POSIX)
#ifndef _isnan
#define _isnan isnan
#endif

// _access -> access (POSIX)
#ifndef _access
#define _access access
#endif

// itoa -> snprintf (POSIX doesn't have itoa)
#ifndef itoa
static inline char *itoa(int value, char *str, int base)
{
  if (base == 10)
    sprintf(str, "%d", value);
  else if (base == 16)
    sprintf(str, "%x", value);
  else if (base == 8)
    sprintf(str, "%o", value);
  else
    sprintf(str, "%d", value);
  return str;
}
#endif

// File system API used by GameState.cpp and others.
// Linux implementation uses dirent.h/stat.h.
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif
#ifndef GetFileAttributesA
static inline DWORD GetFileAttributesA(LPCSTR lpFileName)
{
  struct stat st;
  if (!lpFileName || stat(lpFileName, &st) != 0)
    return INVALID_FILE_ATTRIBUTES;
  if (S_ISDIR(st.st_mode))
    return FILE_ATTRIBUTE_DIRECTORY;
  return FILE_ATTRIBUTE_NORMAL;
}
#define GetFileAttributesA GetFileAttributesA
#define GetFileAttributes GetFileAttributesA
#endif
#define MAX_PATH 260

#ifndef _FILETIME_
#define _FILETIME_
typedef struct _FILETIME {
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;
#endif

typedef struct {
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD dwReserved0;
  DWORD dwReserved1;
  char cFileName[MAX_PATH];
  char cAlternateFileName[14];
} WIN32_FIND_DATA;
typedef WIN32_FIND_DATA WIN32_FIND_DATAA;

#ifndef CreateDirectoryA
static inline BOOL CreateDirectoryA(LPCSTR lpPathName, void* lpSecurityAttributes)
{
  (void)lpSecurityAttributes;
  if (!lpPathName) return FALSE;
  return mkdir(lpPathName, 0755) == 0 || errno == EEXIST;
}
#define CreateDirectory CreateDirectoryA
#endif
#ifndef DRIVE_CDROM
#define DRIVE_CDROM 5
#endif
#ifndef GetDriveTypeA
static inline UINT GetDriveTypeA(LPCSTR) { return 0; }
#define GetDriveType GetDriveTypeA
#endif
#ifndef GetVolumeInformationA
static inline BOOL GetVolumeInformationA(LPCSTR, LPSTR, DWORD, DWORD*, DWORD*, DWORD*, LPSTR, DWORD)
{
  return FALSE;
}
#define GetVolumeInformation GetVolumeInformationA
#endif
// Directory enumeration for FindFirstFileA/FindNextFileA (the
// simulation_uses_no_runtime_trig test walks the source tree with them).
// Only the "*" mask the callers use is supported; the pattern's directory
// part accepts either separator.
typedef struct {
  DIR *dir;
  char dirPath[MAX_PATH];
  char pattern[MAX_PATH];
} WinFindState;

static inline BOOL FindNextFileA(HANDLE hFindFile, WIN32_FIND_DATAA *lpFindFileData)
{
  WinFindState *st = (WinFindState *)hFindFile;
  struct dirent *e;
  if (!st || !st->dir || !lpFindFileData)
    return FALSE;
  while ((e = readdir(st->dir)) != NULL)
  {
    if (e->d_name[0] == 0 || strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
      continue;
    if (st->pattern[0] != 0 && strcmp(st->pattern, "*") != 0 && strcmp(st->pattern, "*.*") != 0)
    {
      if (fnmatch(st->pattern, e->d_name, FNM_CASEFOLD) != 0)
        continue;
    }
    strncpy(lpFindFileData->cFileName, e->d_name, MAX_PATH - 1);
    lpFindFileData->cFileName[MAX_PATH - 1] = 0;
    lpFindFileData->cAlternateFileName[0] = 0;
    lpFindFileData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    char full[MAX_PATH * 2];
    snprintf(full, sizeof(full), "%s/%s", st->dirPath, e->d_name);
    struct stat stbuf;
    if (stat(full, &stbuf) == 0) {
      if (S_ISDIR(stbuf.st_mode))
        lpFindFileData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
      lpFindFileData->nFileSizeLow = (DWORD)(stbuf.st_size & 0xFFFFFFFF);
      lpFindFileData->nFileSizeHigh = (DWORD)((stbuf.st_size >> 32) & 0xFFFFFFFF);
      uint64_t ft = (static_cast<uint64_t>(stbuf.st_mtime) + 11644473600ULL) * 10000000ULL;
      lpFindFileData->ftLastWriteTime.dwLowDateTime = (DWORD)(ft & 0xFFFFFFFF);
      lpFindFileData->ftLastWriteTime.dwHighDateTime = (DWORD)((ft >> 32) & 0xFFFFFFFF);
    }
    return TRUE;
  }
  return FALSE;
}

static inline HANDLE FindFirstFileA(const char *lpFileName, WIN32_FIND_DATAA *lpFindFileData)
{
  if (!lpFileName)
    return INVALID_HANDLE_VALUE;
  const char *slash = strrchr(lpFileName, '/');
  const char *back = strrchr(lpFileName, '\\');
  if (back && (!slash || back > slash))
    slash = back;
  char dir[MAX_PATH];
  char pattern[MAX_PATH];
  if (!slash)
  {
    strcpy(dir, ".");
    strncpy(pattern, lpFileName, sizeof(pattern) - 1);
    pattern[sizeof(pattern) - 1] = 0;
  }
  else if (slash == lpFileName)
  {
    strcpy(dir, "/");
    strncpy(pattern, slash + 1, sizeof(pattern) - 1);
    pattern[sizeof(pattern) - 1] = 0;
  }
  else
  {
    size_t n = (size_t)(slash - lpFileName);
    if (n >= sizeof(dir))
      n = sizeof(dir) - 1;
    memcpy(dir, lpFileName, n);
    dir[n] = 0;
    strncpy(pattern, slash + 1, sizeof(pattern) - 1);
    pattern[sizeof(pattern) - 1] = 0;
  }
  if (pattern[0] == 0)
    strcpy(pattern, "*");
  DIR *d = opendir(dir);
  if (!d)
    return INVALID_HANDLE_VALUE;
  WinFindState *st = (WinFindState *)malloc(sizeof(WinFindState));
  if (!st)
  {
    closedir(d);
    return INVALID_HANDLE_VALUE;
  }
  st->dir = d;
  strncpy(st->dirPath, dir, sizeof(st->dirPath) - 1);
  st->dirPath[sizeof(st->dirPath) - 1] = 0;
  strncpy(st->pattern, pattern, sizeof(st->pattern) - 1);
  st->pattern[sizeof(st->pattern) - 1] = 0;
  if (!FindNextFileA((HANDLE)st, lpFindFileData))
  {
    closedir(d);
    free(st);
    return INVALID_HANDLE_VALUE;
  }
  return (HANDLE)st;
}

static inline BOOL FindClose(HANDLE hFindFile)
{
  if (!hFindFile || hFindFile == INVALID_HANDLE_VALUE)
    return FALSE;
  WinFindState *st = (WinFindState *)hFindFile;
  if (st->dir)
    closedir(st->dir);
  free(st);
  return TRUE;
}
#define FindFirstFile FindFirstFileA
#define FindNextFile FindNextFileA

#ifdef DeleteFile
#undef DeleteFile
#endif
#ifdef DeleteFileA
#undef DeleteFileA
#endif
static inline BOOL DeleteFile(const char *lpFileName)
{
  return unlink(lpFileName) == 0;
}

static inline DWORD GetCurrentDirectory(DWORD nBufferLength, char *lpBuffer)
{
  if (!lpBuffer || nBufferLength == 0)
    return 0;
  char *cwd = getcwd(lpBuffer, nBufferLength);
  return cwd ? (DWORD)strlen(lpBuffer) : 0;
}

static inline BOOL SetCurrentDirectory(const char *lpPathName)
{
  return chdir(lpPathName) == 0;
}

// GlobalAlloc/GlobalFree used by GameMemory.cpp - simple malloc/free wrappers

// GlobalAlloc/GlobalFree used by GameMemory.cpp - simple malloc/free wrappers
#define GMEM_FIXED 0x0000
#define GMEM_MOVEABLE 0x0002
#define GMEM_ZEROINIT 0x0040
static inline void *GlobalAlloc(unsigned int flags, size_t bytes) { (void)flags; return malloc(bytes); }
static inline void *GlobalFree(void *mem) { free(mem); return NULL; }

// Windows threading API used by JobSystem.cpp - pthreads wrappers
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0
#define WAIT_FAILED 0xFFFFFFFF

// HANDLE is defined by rawfile.h (as FILE* on Linux, HANDLE on Windows);
// do NOT define it here.  JobSystem.cpp provides its own local typedef.
typedef void *LPVOID;

typedef struct { DWORD dwNumberOfProcessors; } SYSTEM_INFO;
static inline void GetSystemInfo(SYSTEM_INFO *si)
{
  si->dwNumberOfProcessors = (DWORD)sysconf(_SC_NPROCESSORS_ONLN);
  if (si->dwNumberOfProcessors == 0)
    si->dwNumberOfProcessors = 1;
}

// Sync handles are tagged: WaitForSingleObject must tell semaphores, events
// and threads apart (on Windows the kernel does).  An earlier untagged
// revision overlaid the event struct on raw semaphores and corrupted them -
// the pool hung in stageRandomMap with every thread parked in futex.
#define WAIT_TIMEOUT 258
enum WinHandleKind { WH_SEM = 1, WH_EVENT = 2, WH_THREAD = 3, WH_MUTEX = 4 };
typedef struct {
  int kind;
  sem_t sem;
  int manual;
  int state;
  pthread_t tid;
  pthread_mutex_t mutex;
} WinHandle;

// Process-local recursive mutexes. Named kernel objects/security attributes
// have no implementation here and must fail rather than silently lose semantics.
static inline HANDLE CreateMutex(void *security, BOOL owner, const char *name)
{
  (void)security; (void)name;
  WinHandle *h = (WinHandle *)malloc(sizeof(WinHandle));
  if (!h) return NULL;
  pthread_mutexattr_t attr;
  int rc = pthread_mutexattr_init(&attr);
  if (rc) { free(h); errno = rc; return NULL; }
  rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  if (!rc) rc = pthread_mutex_init(&h->mutex, &attr);
  pthread_mutexattr_destroy(&attr);
  if (rc) { free(h); errno = rc; return NULL; }
  h->kind = WH_MUTEX;
  if (owner && (rc = pthread_mutex_lock(&h->mutex)) != 0)
  {
    pthread_mutex_destroy(&h->mutex);
    free(h); errno = rc; return NULL;
  }
  return (HANDLE)h;
}

static inline BOOL ReleaseMutex(HANDLE handle)
{
  WinHandle *h = (WinHandle *)handle;
  if (!h || h->kind != WH_MUTEX) { errno = EINVAL; return FALSE; }
  int rc = pthread_mutex_unlock(&h->mutex);
  if (rc) errno = rc;
  return rc == 0 ? TRUE : FALSE;
}

static inline HANDLE CreateSemaphore(void *lpSecurityAttributes, LONG lInitialCount, LONG lMaximumCount, const char *lpName)
{
  (void)lpSecurityAttributes; (void)lMaximumCount; (void)lpName;
  WinHandle *h = (WinHandle *)malloc(sizeof(WinHandle));
  if (!h)
    return (HANDLE)NULL;
  h->kind = WH_SEM;
  h->manual = 0;
  h->state = 0;
  sem_init(&h->sem, 0, (unsigned int)lInitialCount);
  return (HANDLE)h;
}

static inline HANDLE CreateEvent(void *lpSecurityAttributes, BOOL bManualReset, BOOL bInitialState, const char *lpName)
{
  (void)lpSecurityAttributes; (void)lpName;
  WinHandle *h = (WinHandle *)malloc(sizeof(WinHandle));
  if (!h)
    return (HANDLE)NULL;
  h->kind = WH_EVENT;
  sem_init(&h->sem, 0, bInitialState ? 1 : 0);
  h->manual = bManualReset ? 1 : 0;
  h->state = bInitialState ? 1 : 0;
  return (HANDLE)h;
}

static inline DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
  WinHandle *h = (WinHandle *)hHandle;
  if (!h)
    return WAIT_FAILED;
  if (h->kind == WH_MUTEX)
  {
    int rc;
    if (dwMilliseconds == INFINITE)
      rc = pthread_mutex_lock(&h->mutex);
    else if (dwMilliseconds == 0)
      rc = pthread_mutex_trylock(&h->mutex);
    else
    {
      struct timespec ts;
      if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return WAIT_FAILED;
      ts.tv_sec += dwMilliseconds / 1000;
      ts.tv_nsec += (dwMilliseconds % 1000) * 1000000L;
      if (ts.tv_nsec >= 1000000000L) { ++ts.tv_sec; ts.tv_nsec -= 1000000000L; }
      rc = pthread_mutex_timedlock(&h->mutex, &ts);
    }
    if (!rc) return WAIT_OBJECT_0;
    errno = rc;
    return (rc == ETIMEDOUT || rc == EBUSY) ? WAIT_TIMEOUT : WAIT_FAILED;
  }
  if (h->kind == WH_SEM)
  {
    // Timeout waits on plain semaphores are not needed by the callers;
    // only the INFINITE fork-join path uses them.
    sem_wait(&h->sem);
    return WAIT_OBJECT_0;
  }
  if (h->kind == WH_THREAD)
  {
    if (dwMilliseconds == INFINITE)
      return pthread_join(h->tid, NULL) == 0 ? WAIT_OBJECT_0 : WAIT_FAILED;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)(dwMilliseconds / 1000);
    ts.tv_nsec += (long)((dwMilliseconds % 1000) * 1000000L);
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000L; }
    return pthread_timedjoin_np(h->tid, NULL, &ts) == 0 ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
  }
  if (h->kind != WH_EVENT)
    return WAIT_FAILED;
  if (dwMilliseconds == INFINITE)
  {
    if (h->manual)
    {
      while (!h->state)
        sem_wait(&h->sem);
    }
    else
    {
      sem_wait(&h->sem);
      h->state = 0;
    }
    return WAIT_OBJECT_0;
  }
  // Bounded event wait: poll the count down in slices so a late SetEvent
  // still lands (the only bounded event wait is the pool's exit join).
  for (;;)
  {
    if (h->manual ? h->state : sem_trywait(&h->sem) == 0)
    {
      if (!h->manual)
        h->state = 0;
      return WAIT_OBJECT_0;
    }
    if (dwMilliseconds == 0)
      return WAIT_TIMEOUT;
    struct timespec slice;
    slice.tv_sec = 0;
    slice.tv_nsec = 1000000L; // 1 ms
    nanosleep(&slice, NULL);
    if (dwMilliseconds > 10)
      dwMilliseconds -= 10;
    else if (dwMilliseconds > 0)
      dwMilliseconds -= 1;
  }
}

static inline BOOL ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LONG *lpPreviousCount)
{
  (void)lpPreviousCount;
  WinHandle *h = (WinHandle *)hSemaphore;
  if (!h || h->kind != WH_SEM)
    return FALSE;
  for (LONG i = 0; i < lReleaseCount; ++i)
    sem_post(&h->sem);
  return TRUE;
}

static inline BOOL SetEvent(HANDLE hEvent)
{
  WinHandle *h = (WinHandle *)hEvent;
  if (!h || h->kind != WH_EVENT)
    return FALSE;
  h->state = 1;
  sem_post(&h->sem);
  return TRUE;
}

static inline BOOL CloseHandle(HANDLE hObject)
{
  WinHandle *h = (WinHandle *)hObject;
  if (!h)
    return FALSE;
  if (h->kind == WH_THREAD)
  {
    pthread_detach(h->tid);
    free(h);
    return TRUE;
  }
  if (h->kind == WH_MUTEX)
  {
    int rc = pthread_mutex_destroy(&h->mutex);
    if (rc) { errno = rc; return FALSE; }
    free(h);
    return TRUE;
  }
  if (h->kind != WH_SEM && h->kind != WH_EVENT)
    return FALSE;
  sem_destroy(&h->sem);
  free(h);
  return TRUE;
}

// TerminateThread (MainMenuUtils kills the async DNS thread on shutdown).
// pthread_cancel is the POSIX twin - equally abrupt, same last-resort role.
static inline BOOL TerminateThread(HANDLE hThread, DWORD exitCode)
{
  (void)exitCode;
  WinHandle *h = (WinHandle *)hThread;
  if (!h || h->kind != WH_THREAD)
    return FALSE;
  return pthread_cancel(h->tid) == 0 ? TRUE : FALSE;
}

// Thread creation wrapper - creates a pthread
static inline HANDLE CreateThread(void *lpThreadAttributes, size_t dwStackSize,
  DWORD (*lpStartAddress)(LPVOID), LPVOID lpParameter, DWORD dwCreationFlags, DWORD *lpThreadId)
{
  (void)lpThreadAttributes; (void)dwStackSize; (void)dwCreationFlags;
  WinHandle *h = (WinHandle *)malloc(sizeof(WinHandle));
  if (!h)
    return (HANDLE)NULL;
  h->kind = WH_THREAD;
  h->manual = 0;
  h->state = 0;
  int rc = pthread_create(&h->tid, NULL, (void *(*)(void *))lpStartAddress, lpParameter);
  if (rc != 0)
  {
    free(h);
    return (HANDLE)NULL;
  }
  if (lpThreadId)
    *lpThreadId = (DWORD)h->tid;
  return (HANDLE)h;
}

static inline DWORD InterlockedExchangeAdd(LONG *addend, LONG value)
{
  return __sync_fetch_and_add(addend, value);
}

// __declspec(thread) -> __thread for GCC.  Thread-locality is load-bearing
// (JobSystem::isWorkerThread reads it); an empty mapping once turned every
// thread into a "worker" after the first pool ran.  Other spellings are inert.
#ifndef __declspec
#define __declspec(x) __declspec_##x
#define __declspec_thread __thread
#define __declspec_naked
#define __declspec_dllexport
#define __declspec_dllimport
#endif
#define TLS __thread

// Interlocked* refcounting (AsciiString et al.): GCC atomic twins with
// identical return-the-new-value semantics. Both spellings: LONG for
// existing call sites, long for AsciiString's (long *) casts.
static inline LONG InterlockedIncrement(LONG *v) { return __sync_add_and_fetch(v, 1); }
static inline LONG InterlockedDecrement(LONG *v) { return __sync_sub_and_fetch(v, 1); }
static inline LONG InterlockedExchange(LONG *v, LONG newVal) { return __sync_lock_test_and_set(v, newVal); }
static inline LONG InterlockedCompareExchange(volatile LONG *v, LONG exchange, LONG comparand)
{
  return __sync_val_compare_and_swap(v, comparand, exchange);
}
static inline long InterlockedIncrement(long *v) { return __sync_add_and_fetch(v, 1); }
static inline long InterlockedDecrement(long *v) { return __sync_sub_and_fetch(v, 1); }
// Calling conventions used across both trees.
#ifndef __cdecl
#define __cdecl
#endif
#ifndef _cdecl
#define _cdecl
#endif
#ifndef FAR
#define FAR
#endif
#ifndef NEAR
#define NEAR
#endif
// SAL annotations (IN/OUT) are no-ops outside MSVC static analysis.
#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif
#ifndef WINAPI
#define WINAPI
#endif
#ifndef __stdcall
#define __stdcall
#endif
// Registry API used by WWDownload/registry.cpp and GameEngine's
// Common/System/registry.cpp; implemented once in wwdownload (in-memory
// backend on Linux, real Registry on Windows).
typedef void *HKEY;
typedef HKEY *PHKEY;
typedef DWORD REGSAM;
#ifndef HKEY_CURRENT_USER
#define HKEY_CURRENT_USER ((HKEY)(intptr_t)1)
#endif
#ifndef HKEY_LOCAL_MACHINE
#define HKEY_LOCAL_MACHINE ((HKEY)(intptr_t)2)
#endif
#ifndef KEY_READ
#define KEY_READ 0x00020019
#endif
#ifndef KEY_WRITE
#define KEY_WRITE 0x00020006
#endif
#ifndef KEY_ALL_ACCESS
#define KEY_ALL_ACCESS 0x000F003F
#endif
#ifndef REG_NONE
#define REG_NONE 0
#endif
#ifndef REG_SZ
#define REG_SZ 1
#endif
#ifndef REG_BINARY
#define REG_BINARY 3
#endif
#ifndef REG_DWORD
#define REG_DWORD 4
#endif
#ifndef REG_OPTION_VOLATILE
#define REG_OPTION_VOLATILE 0x00000001
#endif
#ifndef REG_OPTION_NON_VOLATILE
#define REG_OPTION_NON_VOLATILE 0x00000000
#endif
#ifndef REG_CREATED_NEW_KEY
#define REG_CREATED_NEW_KEY 1
#endif
#ifndef REG_OPENED_EXISTING_KEY
#define REG_OPENED_EXISTING_KEY 2
#endif
#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0
#endif
#ifndef ERROR_MORE_DATA
#define ERROR_MORE_DATA 234
#endif
#ifndef ERROR_FILE_EXISTS
#define ERROR_FILE_EXISTS 80
#endif
#ifndef ERROR_ALREADY_EXISTS
#define ERROR_ALREADY_EXISTS 183
#endif
#ifndef ERROR_ACCESS_DENIED
#define ERROR_ACCESS_DENIED 5
#endif
#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND 2
#endif
LONG RegDeleteKeyA(HKEY hKey, const char *lpSubKey);
LONG RegCreateKeyExA(HKEY hKey, const char *lpSubKey, DWORD Reserved, char *lpClass,
                     DWORD dwOptions, REGSAM samDesired,
                     void *lpSecurityAttributes, PHKEY phkResult,
                     DWORD *lpdwDisposition);
LONG RegOpenKeyExA(HKEY hKey, const char *lpSubKey, DWORD ulOptions, REGSAM samDesired,
                   PHKEY phkResult);
LONG RegQueryValueExA(HKEY hKey, const char *lpValueName, DWORD *lpReserved,
                      DWORD *lpType, unsigned char *lpData, DWORD *lpcbData);
LONG RegSetValueExA(HKEY hKey, const char *lpValueName, DWORD Reserved, DWORD dwType,
                    const unsigned char *lpData, DWORD cbData);
LONG RegCloseKey(HKEY hKey);
#define RegDeleteKey RegDeleteKeyA
#define RegCreateKeyEx RegCreateKeyExA
#define RegOpenKeyEx RegOpenKeyExA
#define RegQueryValueEx RegQueryValueExA
#define RegSetValueEx RegSetValueExA
#ifndef S_OK
#define S_OK ((HRESULT)0)
#endif
#ifndef FAILED
#define FAILED(hr) ((hr) < 0)
#endif
#ifndef SUCCEEDED
#define SUCCEEDED(hr) ((hr) >= 0)
#endif
#endif
#else
#include	<windows.h>
//#include <mmsystem.h>
//#include	<windowsx.h>
//#include	<winnt.h>
//#include	<winuser.h>
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma warning(pop)
#endif

#ifdef _WINDOWS
extern HINSTANCE	ProgramInstance;
extern HWND			MainWindow;
extern bool GameInFocus;

#ifdef _DEBUG

void __cdecl Print_Win32Error(unsigned long win32Error);

#else // _DEBUG

#define Print_Win32Error

#endif // _DEBUG

#else // _WINDOWS
//#include <unistd.h>	// file does not exist
#ifndef WWLIB_TIMEGETTIME_DEFINED
#define WWLIB_TIMEGETTIME_DEFINED 1
#ifndef TIMERR_NOERROR
#define TIMERR_NOERROR 0
#endif
#ifndef MMRESULT
typedef unsigned int MMRESULT;
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
#endif // _WINDOWS

#endif // WIN_H
