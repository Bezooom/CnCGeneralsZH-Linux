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
// Linux twin of the MIDL-generated BrowserDispatch.h.
//
// On Windows this header (plus BrowserDispatch_i.c with the GUID data) is
// produced from BrowserDispatch.idl by MIDL at build time. There is no MIDL
// on Linux, so this checked-in transcription stands in: same GUID values
// (copied from the .idl, which test_support.cpp pins independently),
// same IBrowserDispatch vtable shape (IUnknown trio, then TestMethod).
// GameEngine's WebBrowser.h consumes exactly this surface.
//
// Deployed by CMake to generated/EABrowserDispatch/BrowserDispatch.h, i.e.
// the same path the MIDL step writes on Windows.
/////////////////////////////////////////////////////////////////////////
#ifndef __BROWSERDISPATCH_H__
#define __BROWSERDISPATCH_H__

#include <stdint.h>
#include <string.h>

typedef int32_t LONG;
// NOTE: HRESULT/ULONG deliberately match WWLib's bittype.h spellings
// (long/unsigned long) so TUs including both stay legal; values used fit.
typedef unsigned long ULONG;
typedef long HRESULT;
typedef int BOOL;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define S_OK ((HRESULT)0L)
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#define E_NOTIMPL ((HRESULT)0x80004001L)

#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE
#endif
#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID
{
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t Data4[8];
} GUID;
#endif
typedef GUID IID;

#ifdef __cplusplus
typedef const GUID &REFIID;
typedef const GUID &REFCLSID;

inline bool IsEqualGUID(const GUID &a, const GUID &b)
{
  return memcmp(&a, &b, sizeof(GUID)) == 0;
}
#else
/* Plain-C callers compare GUIDs with memcmp directly. */
typedef const GUID *REFIID;
typedef const GUID *REFCLSID;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* LIBID from `library BROWSERDISPATCHLib` in the .idl. */
extern const GUID LIBID_BROWSERDISPATCHLib;
/* IID from `interface IBrowserDispatch` in the .idl. */
extern const GUID IID_IBrowserDispatch;
/* Standard IUnknown IID (not in the .idl; from unkwn.h). */
extern const GUID IID_IUnknown;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#define STDMETHOD(m) virtual HRESULT STDMETHODCALLTYPE m
#define STDMETHOD_(t, m) virtual t STDMETHODCALLTYPE m
#define STDMETHODIMP HRESULT STDMETHODCALLTYPE
#define STDMETHODIMP_(t) t STDMETHODCALLTYPE
struct IUnknown
{
  STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject) = 0;
  STDMETHOD_(ULONG, AddRef)(void) = 0;
  STDMETHOD_(ULONG, Release)(void) = 0;
};

/* Dual interface from the .idl: IUnknown trio, then TestMethod. */
struct IBrowserDispatch : public IUnknown
{
  STDMETHOD(TestMethod)(int num1) = 0;
};

#endif /* __cplusplus */

#endif /* __BROWSERDISPATCH_H__ */
