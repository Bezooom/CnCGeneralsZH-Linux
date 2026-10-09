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
// Linux twin of ATL's atlbase.h/atlcom.h, parse-level subset.
//
// The WOL lobby browser (WebBrowser/FEBDispatch) is out of scope until
// multiplayer (Phase 6+): WebBrowser.cpp does not compile on Linux and
// nothing constructs these objects headless. What must keep parsing is
// the headers, which GameEngine.cpp and the WOL menus include for the
// TheWebBrowser pointer and the IBrowserDispatch vtable shape.
//
// COM base types come from EABrowserDispatch/BrowserDispatch.h (same as
// the Windows build arranges); only the ATL templates/macros and the
// automation helpers FEBDispatch's body names are defined here. Template
// bodies are never instantiated on Linux, so LoadTypeLib and friends need
// declarations, not definitions. QueryInterface routing is real enough to
// preserve the vtable layout.
//
// Lives outside every Windows include path on purpose: the real ATL must
// keep winning there.
/////////////////////////////////////////////////////////////////////////
#ifndef WWVEGAS_LINUXDX_ATLBASE_H
#define WWVEGAS_LINUXDX_ATLBASE_H

#include "EABrowserDispatch/BrowserDispatch.h"
#include <string.h>

#ifndef E_POINTER
#define E_POINTER ((HRESULT)0x80004003L)
#endif
#ifndef PURE
#define PURE = 0
#endif

// --- single-threaded model ----------------------------------------------------
struct CComSingleThreadModel
{
  static unsigned long Increment(unsigned long *p) { return ++(*p); }
  static unsigned long Decrement(unsigned long *p) { return --(*p); }
};

// --- module ---------------------------------------------------------------------
struct CComModule
{
  HRESULT Init(void *, void *) { return S_OK; }
  void Term(void) {}
};

// --- object root (QueryInterface routing) -----------------------------------------
template <class ThreadModel>
struct CComObjectRootEx
{
  unsigned long m_dwRef;
  CComObjectRootEx(void) : m_dwRef(0) {}
  unsigned long InternalAddRef(void) { return ThreadModel::Increment(&m_dwRef); }
  unsigned long InternalRelease(void) { return ThreadModel::Decrement(&m_dwRef); }
};

#define BEGIN_COM_MAP(x) \
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) { \
    if (!ppv) return E_POINTER; \
    *ppv = NULL;

#define COM_INTERFACE_ENTRY(x) \
  { (void)sizeof(x); }

#define COM_INTERFACE_ENTRY_AGGREGATE(iid, punk) \
  { (void)sizeof(iid); (void)punk; }

#define END_COM_MAP() \
    return E_NOINTERFACE; \
  }

// --- class factory roots ------------------------------------------------------------
template <class T>
struct CComCoClass
{
  static const GUID *GetObjectCLSID(void) { return NULL; }
};

template <class Base>
struct CComObject : public Base
{
  // noexcept: WebBrowser declares its IUnknown trio noexcept, and a looser
  // override is ill-formed.
  unsigned long m_refcount;
  CComObject(void) : m_refcount(0) {}
  unsigned long AddRef(void) noexcept { return ++m_refcount; }
  unsigned long Release(void) noexcept { unsigned long r = m_refcount ? --m_refcount : 0; if (!r) delete this; return r; }
};

// --- automation types used by FEBDispatch's (never instantiated) body ----------------
struct ITypeInfo : public IUnknown
{
};
struct ITypeLib : public IUnknown
{
  STDMETHOD(GetTypeInfoOfGuid)(const GUID &, ITypeInfo **) PURE;
};
struct IDispatch : public IUnknown
{
};

struct _bstr_t
{
  _bstr_t(const char *) {}
  operator const char *(void) const { return ""; }
};

extern const GUID IID_IDispatch;

inline HRESULT LoadTypeLib(const char *, ITypeLib **)
{
  return E_NOINTERFACE;
}
inline HRESULT CreateStdDispatch(IUnknown *, void *, ITypeInfo *, IUnknown **)
{
  return E_NOINTERFACE;
}
// Only a parse-level declaration for FEBDispatch's (never instantiated)
// body; when the includer already maps the name (e.g. PreRTS.h) the macro
// wins and this stub must not compete with it.
#ifndef GetModuleFileName
inline unsigned long GetModuleFileName(void *, char *, unsigned long)
{
  return 0;
}
#endif

#endif // WWVEGAS_LINUXDX_ATLBASE_H
