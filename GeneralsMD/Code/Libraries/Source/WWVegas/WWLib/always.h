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
 *                     $Archive:: /Commando/Code/wwlib/always.h                               $*
 *                                                                                             *
 *                      $Author:: Steve_t                                                     $*
 *                                                                                             *
 *                     $Modtime:: 8/28/01 3:21p                                               $*
 *                                                                                             *
 *                    $Revision:: 13                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef ALWAYS_H
#define ALWAYS_H

#include <assert.h>

// Disable warning about exception handling not being enabled. It's used as part of STL - in a part of STL we don't use.
#if defined(_MSC_VER)
#pragma warning(disable : 4530)
#endif

/*
** Define for debug memory allocation to include __FILE__ and __LINE__ for every memory allocation.
** This helps find leaks.
*/
//#define STEVES_NEW_CATCHER
#ifdef _DEBUG
#ifdef _MSC_VER
#ifdef STEVES_NEW_CATCHER

#include	<crtdbg.h>
#include <stdlib.h>
#include <malloc.h>

#define   malloc(s)         _malloc_dbg(s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define   calloc(c, s)      _calloc_dbg(c, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define   realloc(p, s)     _realloc_dbg(p, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define   _expand(p, s)     _expand_dbg(p, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define   free(p)           _free_dbg(p, _NORMAL_BLOCK)
#define   _msize(p)         _msize_dbg(p, _NORMAL_BLOCK)

void* __cdecl operator new(unsigned int s);

#endif	//STEVES_NEW_CATCHER
#endif	//_MSC_VER
#endif	//_DEBUG

// =============================================================================
// Memory management - "killing the Generals Memory Manager" (gth)
// =============================================================================

#if 1

#ifndef _OPERATOR_NEW_DEFINED_
#define _OPERATOR_NEW_DEFINED_

#if defined(_MSC_VER)
	// MSVC-specific operator new/delete with __cdecl calling convention
	extern void * __cdecl operator new		(size_t size);
	extern void __cdecl operator delete		(void *p);

	extern void * __cdecl operator new[]	(size_t size);
	extern void __cdecl operator delete[]	(void *p);

	// additional overloads to account for VC/MFC funky versions
	extern void* __cdecl operator new			(size_t nSize, const char *, int);
	extern void __cdecl operator delete		(void *, const char *, int);

	extern void* __cdecl operator new[]		(size_t nSize, const char *, int);
	extern void __cdecl operator delete[]	(void *, const char *, int);

	// additional overloads for 'placement new'
	//inline void* __cdecl operator new							(size_t s, void *p) { return p; }
	//inline void __cdecl operator delete						(void *, void *p)		{ }
#if !defined(_MSC_VER) || _MSC_VER < 1300	// VC7+ <new> already defines array placement new
	inline void* __cdecl operator new[]						(size_t s, void *p) { return p; }
	inline void __cdecl operator delete[]					(void *, void *p)		{ }
#endif
#else
	// GCC/Clang: standard placement new from <new> is sufficient
	// Array placement new (C++14) - only if not already defined
	#if __cplusplus < 201402L
	inline void* operator new[](size_t, void* p) noexcept { return p; }
	inline void operator delete[](void*, void*) noexcept {}
	#endif
#endif

#endif // _OPERATOR_NEW_DEFINED_

#if (defined(_DEBUG) || defined(_INTERNAL)) 
	#define MSGW3DNEW(MSG)					new( MSG, 0 )
	#define MSGW3DNEWARRAY(MSG)			new( MSG, 0 )
	#define W3DNEW									new("W3D_" __FILE__, 0)
	#define W3DNEWARRAY							new("W3A_" __FILE__, 0)
#else
	#define MSGW3DNEW(MSG)					new
	#define MSGW3DNEWARRAY(MSG)			new
	#define W3DNEW									new
	#define W3DNEWARRAY							new
#endif

// ----------------------------------------------------------------------------
extern void* createW3DMemPool(const char *poolName, int allocationSize);
extern void* allocateFromW3DMemPool(void* p, int allocationSize);
extern void* allocateFromW3DMemPool(void* p, int allocationSize, const char* msg, int unused);
extern void freeFromW3DMemPool(void* pool, void* p);

// ----------------------------------------------------------------------------
#define W3DMPO_GLUE(ARGCLASS) \
private: \
	static void* getClassMemoryPool() \
	{ \
		/* \
			Note that this static variable will be initialized exactly once: the first time \
			control flows over this section of code. This allows us to neatly resolve the \
			order-of-execution problem for static variables, ensuring this is not executed \
			prior to the initialization of TheMemoryPoolFactory. \
		*/ \
		static void* The##ARGCLASS##Pool = createW3DMemPool(#ARGCLASS, sizeof(ARGCLASS)); \
		return The##ARGCLASS##Pool; \
	} \
protected: \
	virtual void glueEnforcer() const { } \
public: \
	inline void* operator new(size_t s) { return allocateFromW3DMemPool(getClassMemoryPool(), s); } \
	inline void operator delete(void *p) { freeFromW3DMemPool(getClassMemoryPool(), p); } \
	inline void* operator new(size_t s, const char* msg, int unused) { return allocateFromW3DMemPool(getClassMemoryPool(), s, msg, unused); } \
	inline void operator delete(void *p, const char* msg, int unused) { freeFromW3DMemPool(getClassMemoryPool(), p); } \

// ----------------------------------------------------------------------------
class W3DMPO
{
private:
	static void* getClassMemoryPool()
	{
		assert(0);	// must replace this via W3DMPO_GLUE
		return 0;
	}
protected:
	// we never call this; it is present to cause compile errors in descendent classes
	virtual void glueEnforcer() const = 0;
public:
	virtual ~W3DMPO() { /* nothing */ }
};
// ----------------------------------------------------------------------------

#else // #if 1 - alternative memory manager (disabled)

// Memory manager disabled - use plain new/delete
#define MSGW3DNEW(MSG)					new
#define MSGW3DNEWARRAY(MSG)			new
#define W3DNEW									new
#define W3DNEWARRAY							new

#define W3DMPO_GLUE(ARGCLASS)

// Minimal W3DMPO for compatibility
class W3DMPO { };

#endif // (gth) removing the generals memory stuff from W3D


// =============================================================================
// Compiler-specific inlining
// =============================================================================
// Jani: MSVC doesn't necessarily inline code with inline keyword. Using __forceinline results better inlining
// and also prints out a warning if inlining wasn't possible. __forceinline is MSVC specific.
#if defined(_MSC_VER)
#define WWINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define WWINLINE __attribute__((always_inline)) inline
#else
#define WWINLINE inline
#endif

// Cross-platform calling convention
#ifndef __cdecl
#define __cdecl
#endif
#ifndef _cdecl
#define _cdecl
#endif

// MSVC CRT string-compare names used across WWLib/WWMath; map to POSIX.
#if !defined(_MSC_VER)
#include <strings.h>
#include <string.h>
#include <ctype.h>
#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif
#ifndef strcmpi
#define strcmpi strcasecmp
#endif
// strupr/strlwr modify in place and return the input, like the MSVC originals.
// NOTE: no _strupr/_strlwr aliases here - the GameSpy SDK declares those
// itself (C linkage), and a macro would hijack its declarations.
static inline char* strupr(char* s)
{
	if (s != NULL) {
		for (char* p = s; *p != '\0'; ++p)
			*p = (char)toupper((unsigned char)*p);
	}
	return s;
}
static inline char* strlwr(char* s)
{
	if (s != NULL) {
		for (char* p = s; *p != '\0'; ++p)
			*p = (char)tolower((unsigned char)*p);
	}
	return s;
}
// _lrotl is an MSVC CRT intrinsic; crc.h/crc.cpp rotate the accumulator with
// it. Full-width rotate stays self-consistent on LP64 (long is 64-bit there,
// 32-bit on Windows). Network/save compat with Windows-built values is a
// later-phase concern (multiplayer is out of scope per PLAN).
// NOTE: GCC's ia32intrin.h #defines _lrotl to __rolq, so undef it first -
// depending on header order the macro may or may not be active here.
#ifdef _lrotl
#undef _lrotl
#endif
static inline unsigned long _lrotl(unsigned long value, unsigned int shift)
{
	const unsigned int bits = (unsigned int)(sizeof(unsigned long) * 8);
	shift &= (bits - 1u);
	return shift ? (unsigned long)((value << shift) | (value >> (bits - shift))) : value;
}
// _snprintf/_alloca are MSVC spellings; snprintf/alloca are their C99 twins
// (_snprintf's missing NUL guarantee is not relied on at the call sites).
#include <stdio.h>
#include <alloca.h>
#include <stdarg.h>
#include <wchar.h>
#ifndef _snprintf
#define _snprintf snprintf
#endif
#ifndef _alloca
#define _alloca alloca
#endif
// _vsnprintf/_vsnwprintf with MSVC semantics: NUL-termination is NOT
// guaranteed and truncation returns -1 (vsnprintf would return the needed
// size instead). Call sites branch on "< 0" and hand the return to Write,
// so the mapping must preserve the contract, not just the name.
static inline int ww_msvc_vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
	if (n == 0)
		return -1;
	int r = vsnprintf(buf, n, fmt, ap);
	return (r < 0 || (size_t)r >= n) ? -1 : r;
}
static inline int ww_msvc_vsnwprintf(wchar_t *buf, size_t n, const wchar_t *fmt, va_list ap)
{
	if (n == 0)
		return -1;
	int r = vswprintf(buf, n, fmt, ap);
	return (r < 0 || (size_t)r >= n) ? -1 : r;
}
#ifndef _vsnprintf
#define _vsnprintf ww_msvc_vsnprintf
#endif
#ifndef _vsnwprintf
#define _vsnwprintf ww_msvc_vsnwprintf
#endif
// No DBWIN-style viewer on Linux; keep the diagnostics on stderr.
#ifndef OutputDebugString
static inline void OutputDebugString(const char *s) { if (s) fprintf(stderr, "%s", s); }
#endif
#endif

/*
** Define the MIN and MAX macros.
** NOTE: Joe used to #include <minmax.h> in the various compiler header files.  This
** header defines 'min' and 'max' macros which conflict with the surrender code so
** I'm replacing all occurances of 'min' and 'max with 'MIN' and 'MAX'.  For code which
** is out of our domain (e.g. Max sdk) I'm declaring template functions for 'min' and 'max'
*/
#define NOMINMAX

#ifndef MAX
#define MAX(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

// min/max templates for code that uses std::min/max style (e.g. Max SDK)
template <class T> T min(T a,T b)
{
	if (a<b) {
		return a;
	} else {
		return b;
	}
}

template <class T> T max(T a,T b)
{
	if (a>b) {
		return a;
	} else {
		return b;
	}
}


/*
**	This includes the minimum set of compiler defines and pragmas in order to bring the
**	various compilers to a common behavior such that the C&C engine will compile without
**	error or warning.
*/
#if defined(__BORLANDC__)
#include	"borlandc.h"
#endif

#if defined(_MSC_VER)
#include	"visualc.h"
#endif

#if defined(__WATCOMC__)
#include	"watcom.h"
#endif


#ifndef	NULL
#define	NULL		0
#endif

/**********************************************************************
**	This macro serves as a general way to determine the number of elements
**	within an array.
*/
#ifndef ARRAY_SIZE
#define	ARRAY_SIZE(x)		int(sizeof(x)/sizeof(x[0]))
#endif

#ifndef size_of
#define size_of(typ,id) sizeof(((typ*)0)->id)
#endif

#endif // ALWAYS_H