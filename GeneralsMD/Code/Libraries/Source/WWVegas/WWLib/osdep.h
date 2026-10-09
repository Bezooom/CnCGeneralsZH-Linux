/*
** WWLib osdep.h — Linux port shim (2026-09-12)
**
** The tree's _UNIX branches (thread.h, wwstring.h, vector3.h, matrix3.h,
** srandom.cpp, hash.cpp, ...) include "osdep.h" from the original Westwood
** Unix port, but that header was never shipped. This minimal shim unblocks
** the Linux build; extend it only with proven-missing Unix declarations.
*/

#ifndef WWLIB_OSDEP_H
#define WWLIB_OSDEP_H

#include <stdint.h>
#include <stddef.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>

// MSVC string-compare names used across WWLib/WWMath; map to POSIX.
#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif
#ifndef strcmpi
#define strcmpi strcasecmp
#endif

#endif // WWLIB_OSDEP_H
