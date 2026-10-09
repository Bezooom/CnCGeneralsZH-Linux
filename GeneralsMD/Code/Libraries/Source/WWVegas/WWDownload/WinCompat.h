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

// WinCompat.h
// Win32 API surface used by WWDownload (registry wrapper, FTP downloader,
// URL builder) plus the file/time helpers its test needs.
//
// On MSVC this is exactly <windows.h>.  On Linux it declares the subset the
// library and its test touch; the implementations live in registry.cpp
// (Registry API, in-memory backend) and FTP.CPP (file helpers), or map to
// libc/POSIX directly.  Anything also defined by WWLib's bittype.h uses the
// identical spelling so the two headers can mix.

#pragma once

#ifndef __WWDOWNLOAD_WINCOMPAT_H__
#define __WWDOWNLOAD_WINCOMPAT_H__

#ifdef _MSC_VER

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else // Linux

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

// --- base types (bittype.h-identical where overlapping) --------------------
typedef uint32_t DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned long ULONG;
typedef int32_t LONG;
typedef void *HANDLE;
typedef void *HMODULE;
typedef long HRESULT;
typedef char CHAR;
typedef DWORD *LPDWORD;
typedef BYTE *LPBYTE;
typedef const BYTE *LPCBYTE;
typedef char *LPSTR;
typedef const char *LPCSTR;
typedef const char *LPCTSTR;
typedef void *LPVOID;
typedef void *LPSECURITY_ATTRIBUTES;
typedef DWORD REGSAM;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((void *)-1)
#endif
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)0xFFFFFFFF)
#endif
#ifndef FILE_ATTRIBUTE_READONLY
#define FILE_ATTRIBUTE_READONLY 0x00000001
#endif
#ifndef FILE_ATTRIBUTE_DIRECTORY
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#endif
#ifndef FILE_ATTRIBUTE_NORMAL
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#endif
#ifndef S_OK
#define S_OK ((HRESULT)0L)
#endif
#ifndef S_FALSE
#define S_FALSE ((HRESULT)1L)
#endif
#ifndef E_FAIL
#define E_FAIL ((HRESULT)0x80004005L)
#endif
#ifndef E_NOTIMPL
#define E_NOTIMPL ((HRESULT)0x80004001L)
#endif
#ifndef E_NOINTERFACE
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#endif
#ifndef SEVERITY_ERROR
#define SEVERITY_ERROR 1
#endif
#ifndef FACILITY_ITF
#define FACILITY_ITF 4
#endif
#ifndef MAKE_HRESULT
#define MAKE_HRESULT(sev, fac, code) \
  ((HRESULT)(((unsigned long)(sev) << 31) | ((unsigned long)(fac) << 16) | ((unsigned long)(code))))
#endif

// --- registry ---------------------------------------------------------------
typedef void *HKEY;
typedef HKEY *PHKEY;
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

LONG RegDeleteKeyA(HKEY hKey, LPCSTR lpSubKey);
LONG RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass,
                     DWORD dwOptions, REGSAM samDesired,
                     LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult,
                     LPDWORD lpdwDisposition);
LONG RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired,
                   PHKEY phkResult);
LONG RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                      LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
LONG RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType,
                    const BYTE *lpData, DWORD cbData);
LONG RegCloseKey(HKEY hKey);

// ANSI<->W-less aliases the library spells without suffix.
#define RegDeleteKey RegDeleteKeyA
#define RegCreateKeyEx RegCreateKeyExA
#define RegOpenKeyEx RegOpenKeyExA
#define RegQueryValueEx RegQueryValueExA
#define RegSetValueEx RegSetValueExA
#define RegCreateKey RegCreateKeyA

// --- sockets -----------------------------------------------------------------
// Winsock names used by FTP.CPP mapped onto POSIX + errno.
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#ifndef WINAPI
#define WINAPI
#endif
typedef struct hostent HOSTENT;

#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
#ifndef closesocket
#define closesocket close
#endif
// WSAGetLastError is defined in WWLib/win.h
// static inline int WSAGetLastError(void) { return errno; }
#ifndef WSAEWOULDBLOCK
#define WSAEWOULDBLOCK EWOULDBLOCK
#endif
#ifndef WSAEINVAL
#define WSAEINVAL EINVAL
#endif
#ifndef WSAEALREADY
#define WSAEALREADY EALREADY
#endif
#ifndef WSAEISCONN
#define WSAEISCONN EISCONN
#endif
#ifndef WSAECONNRESET
#define WSAECONNRESET ECONNRESET
#endif
#ifndef WSAENOTCONN
#define WSAENOTCONN ENOTCONN
#endif
#ifndef WSAEINPROGRESS
#define WSAEINPROGRESS EINPROGRESS
#endif

// --- files --------------------------------------------------------------------
static inline DWORD WinCompat_GetTempPathA(DWORD nBufferLength, LPSTR lpBuffer)
{
  const char *tmp = getenv("TMPDIR");
  if (!tmp || !*tmp)
    tmp = "/tmp";
  // callers append a bare filename, so the separator is required
  snprintf(lpBuffer, nBufferLength, "%s/", tmp);
  return (DWORD)strlen(lpBuffer);
}
#define GetTempPathA WinCompat_GetTempPathA
static inline BOOL WinCompat_CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES unused)
{
  (void)unused;
  return mkdir(lpPathName, 0755) == 0 ? TRUE : FALSE;
}
#define CreateDirectoryA WinCompat_CreateDirectoryA
static inline BOOL WinCompat_RemoveDirectoryA(LPCSTR lpPathName)
{
  return rmdir(lpPathName) == 0 ? TRUE : FALSE;
}
#define RemoveDirectoryA WinCompat_RemoveDirectoryA
#ifndef GetFileAttributesA
static inline DWORD WinCompat_GetFileAttributesA(LPCSTR lpFileName)
{
  struct stat st;
  if (stat(lpFileName, &st) != 0)
    return INVALID_FILE_ATTRIBUTES;
  if (S_ISDIR(st.st_mode))
    return FILE_ATTRIBUTE_DIRECTORY;
  return FILE_ATTRIBUTE_NORMAL;
}
#define GetFileAttributesA WinCompat_GetFileAttributesA
#endif
static inline BOOL WinCompat_DeleteFileA(LPCSTR lpFileName)
{
  return remove(lpFileName) == 0 ? TRUE : FALSE;
}
#define DeleteFileA WinCompat_DeleteFileA
#define DeleteFile WinCompat_DeleteFileA
static inline BOOL WinCompat_MoveFileA(LPCSTR from, LPCSTR to)
{
  return rename(from, to) == 0 ? TRUE : FALSE;
}
#define MoveFileA WinCompat_MoveFileA
#define MoveFile WinCompat_MoveFileA

// --- CRT names ------------------------------------------------------------------
#ifndef _snprintf
#define _snprintf snprintf
#endif
#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#define _mkdir(p) mkdir(p, 0755)
#define _getcwd getcwd
#define _chmod chmod
#ifndef _S_IWRITE
#define _S_IWRITE S_IWUSR
#endif
#ifndef _S_IREAD
#define _S_IREAD S_IRUSR
#endif
#define _unlink unlink
#define _splitpath WinCompat_splitpath
static inline void WinCompat_splitpath(const char *path, char *drive, char *dir,
                                       char *fname, char *ext)
{
  // drive is always empty on Linux; dir keeps its trailing separator,
  // like _splitpath. Both separator kinds split.
  const char *slash, *bslash, *sep, *base, *dot;
  if (drive)
    *drive = 0;
  slash = strrchr(path, '/');
  bslash = strrchr(path, '\\');
  if (slash && bslash)
    sep = slash > bslash ? slash : bslash;
  else
    sep = slash ? slash : bslash;
  base = sep ? sep + 1 : path;
  dot = strrchr(base, '.');
  if (dir)
  {
    if (sep)
    {
      size_t n = (size_t)(sep - path) + 1;
      memcpy(dir, path, n);
      dir[n] = 0;
    }
    else
      *dir = 0;
  }
  if (fname)
  {
    const char *nameEnd = dot ? dot : base + strlen(base);
    size_t n = (size_t)(nameEnd - base);
    memcpy(fname, base, n);
    fname[n] = 0;
  }
  if (ext)
  {
    if (dot)
      strcpy(ext, dot);
    else
      *ext = 0;
  }
}
#define _stat stat
#ifndef MulDiv
static inline int WinCompat_MulDiv(int a, int b, int c)
{
  return c ? (int)(((long long)a * b) / c) : 0;
}
#define MulDiv WinCompat_MulDiv
#endif

// --- time/debug ------------------------------------------------------------------
#ifndef WWLIB_TIMEGETTIME_DEFINED
#define WWLIB_TIMEGETTIME_DEFINED 1
static inline DWORD timeGetTime(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (DWORD)((ts.tv_sec * 1000ULL) + (ts.tv_nsec / 1000000ULL));
}
#endif
#ifndef OutputDebugString
#define OutputDebugString(s) do { fprintf(stderr, "%s", (s)); } while (0)
#endif

#endif // _MSC_VER / Linux

#endif // __WWDOWNLOAD_WINCOMPAT_H__
