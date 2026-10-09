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
// Linux stub for EABrowserDispatch (web browser plugin).
//
// Web browser functionality is out of scope until Phase 4; this TU only
// provides the GUID data the MIDL-generated BrowserDispatch_i.c would
// contain, so test_support's link-time checks and GameEngine's
// WebBrowser.h see real symbols. Values copied from BrowserDispatch.idl.
/////////////////////////////////////////////////////////////////////////
#include "EABrowserDispatch/BrowserDispatch.h"

/* library BROWSERDISPATCHLib */
const GUID LIBID_BROWSERDISPATCHLib =
  { 0xC92D8250, 0xA628, 0x4CE5, { 0x82, 0x3F, 0x1A, 0x1F, 0x11, 0x6E, 0xFC, 0xC9 } };

/* interface IBrowserDispatch */
const GUID IID_IBrowserDispatch =
  { 0xBC834510, 0xC5BC, 0x4B90, { 0x8C, 0x9A, 0x0E, 0x4B, 0x19, 0x98, 0x79, 0x6F } };

/* standard IUnknown IID */
const GUID IID_IUnknown =
  { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
