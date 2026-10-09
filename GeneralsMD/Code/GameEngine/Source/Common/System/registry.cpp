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

// Registry.cpp
// Simple interface for storing/retreiving registry values
// Author: Matthew D. Campbell, December 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file int the GameEngine

#include "Common/Registry.h"

#ifdef _INTERNAL
// for occasional debugging...
//#pragma optimize("", off)
//#pragma MESSAGE("************************************** WARNING, optimization disabled for debugging purposes")
#endif

#ifdef _WIN32
Bool  getStringFromRegistry(HKEY root, AsciiString path, AsciiString key, AsciiString& val)
{
	HKEY handle;
	unsigned char buffer[256];
	unsigned long size = 256;
	unsigned long type;
	int returnValue;

	if ((returnValue = RegOpenKeyEx( root, path.str(), 0, KEY_READ, &handle )) == ERROR_SUCCESS)
	{
		returnValue = RegQueryValueEx(handle, key.str(), NULL, &type, (unsigned char *) &buffer, &size);
		RegCloseKey( handle );
	}

	if (returnValue == ERROR_SUCCESS)
	{
		val = (char *)buffer;
		return TRUE;
	}

	return FALSE;
}

Bool getUnsignedIntFromRegistry(HKEY root, AsciiString path, AsciiString key, UnsignedInt& val)
{
	HKEY handle;
	unsigned char buffer[4];
	unsigned long size = 4;
	unsigned long type;
	int returnValue;

	if ((returnValue = RegOpenKeyEx( root, path.str(), 0, KEY_READ, &handle )) == ERROR_SUCCESS)
	{
		returnValue = RegQueryValueEx(handle, key.str(), NULL, &type, (unsigned char *) &buffer, &size);
		RegCloseKey( handle );
	}

	if (returnValue == ERROR_SUCCESS)
	{
		val = *(UnsignedInt *)buffer;
		return TRUE;
	}

	return FALSE;
}

Bool setStringInRegistry( HKEY root, AsciiString path, AsciiString key, AsciiString val)
{
	HKEY handle;
	unsigned long type;
	unsigned long returnValue;
	int size;

	if ((returnValue = RegCreateKeyEx( root, path.str(), 0, "REG_NONE", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &handle, NULL )) == ERROR_SUCCESS)
	{
		type = REG_SZ;
		size = val.getLength()+1;
		returnValue = RegSetValueEx(handle, key.str(), 0, type, (unsigned char *)val.str(), size);
		RegCloseKey( handle );
	}

	return (returnValue == ERROR_SUCCESS);
}

Bool setUnsignedIntInRegistry( HKEY root, AsciiString path, AsciiString key, UnsignedInt val)
{
	HKEY handle;
	unsigned long type;
	unsigned long returnValue;
	int size;

	if ((returnValue = RegCreateKeyEx( root, path.str(), 0, "REG_NONE", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &handle, NULL )) == ERROR_SUCCESS)
	{
		type = REG_DWORD;
		size = 4;
		returnValue = RegSetValueEx(handle, key.str(), 0, type, (unsigned char *)&val, size);
		RegCloseKey( handle );
	}

	return (returnValue == ERROR_SUCCESS);
}

Bool GetStringFromGeneralsRegistry(AsciiString path, AsciiString key, AsciiString& val)
{
	AsciiString fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Generals";

	fullPath.concat(path);
	DEBUG_LOG(("GetStringFromRegistry - looking in %s for key %s\n", fullPath.str(), key.str()));
	if (getStringFromRegistry(HKEY_CURRENT_USER, fullPath.str(), key.str(), val))
	{
		return TRUE;
	}

	return getStringFromRegistry(HKEY_LOCAL_MACHINE, fullPath.str(), key.str(), val);
}

Bool GetStringFromRegistry(AsciiString path, AsciiString key, AsciiString& val)
{
	AsciiString fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour";

	fullPath.concat(path);
	// HKCU first, HKLM second. The machine-wide hive needs administrator rights to write, so a
	// per-user or Steam install writes only HKCU - and a stale HKLM entry left by some other copy of
	// the game used to win, sending this one at another install's language or data folder.
	DEBUG_LOG(("GetStringFromRegistry - looking in %s for key %s\n", fullPath.str(), key.str()));
	if (getStringFromRegistry(HKEY_CURRENT_USER, fullPath.str(), key.str(), val))
	{
		return TRUE;
	}

	return getStringFromRegistry(HKEY_LOCAL_MACHINE, fullPath.str(), key.str(), val);
}

Bool GetUnsignedIntFromRegistry(AsciiString path, AsciiString key, UnsignedInt& val)
{
	AsciiString fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour";

	fullPath.concat(path);
	DEBUG_LOG(("GetUnsignedIntFromRegistry - looking in %s for key %s\n", fullPath.str(), key.str()));
	if (getUnsignedIntFromRegistry(HKEY_CURRENT_USER, fullPath.str(), key.str(), val))
	{
		return TRUE;
	}

	return getUnsignedIntFromRegistry(HKEY_LOCAL_MACHINE, fullPath.str(), key.str(), val);
}

AsciiString GetRegistryLanguage(void)
{
	static Bool cached = FALSE;
	// NOTE: static causes a memory leak, but we have to keep it because the value is cached.
	static AsciiString val = "english";
	if (cached) {
		return val;
	} else {
		cached = TRUE;
	}

	GetStringFromRegistry("", "Language", val);
	return val;
}

AsciiString GetRegistryGameName(void)
{
	AsciiString val = "GeneralsMPTest";
	GetStringFromRegistry("", "SKU", val);
	return val;
}

UnsignedInt GetRegistryVersion(void)
{
	UnsignedInt val = 65536;
	GetUnsignedIntFromRegistry("", "Version", val);
	return val;
}

UnsignedInt GetRegistryMapPackVersion(void)
{
	UnsignedInt val = 65536;
	GetUnsignedIntFromRegistry("", "MapPackVersion", val);
	return val;
}
#else
#if defined(__linux__)
#include "PAL_Registry.h"

#include <string>
#include <vector>

namespace {
// The game's own keys live in two sections. A file that holds any other section is
// rejected rather than skipped, so a write cannot drop a section the game never wrote.
const std::vector<std::string> kGameSections{"General", "Options"};

bool address(const AsciiString& path, const AsciiString& key, std::string& section)
{
	const std::string name(path.str());
	if (name.empty() || name == "General") section = "General";
	else if (name == "\\Options" || name == "Options") section = "Options";
	else return false;
	return pal::ValidKey(key.str());
}
} // namespace

Bool GetStringFromRegistry(AsciiString path, AsciiString key, AsciiString& val)
{
	std::string section, encoded;
	if (!address(path, key, section) || !pal::ReadValue(section, key.str(), "SZ:", kGameSections, encoded)) return FALSE;
	std::vector<unsigned char> bytes;
	if (!pal::DecodeHex(encoded, bytes)) return FALSE;
	val = std::string(bytes.begin(), bytes.end()).c_str();
	return TRUE;
}

Bool GetStringFromGeneralsRegistry(AsciiString path, AsciiString key, AsciiString& val)
{
	return GetStringFromRegistry(path, key, val);
}

Bool GetUnsignedIntFromRegistry(AsciiString path, AsciiString key, UnsignedInt& val)
{
	std::string section, encoded;
	if (!address(path, key, section) || !pal::ReadValue(section, key.str(), "DWORD:", kGameSections, encoded)) return FALSE;
	unsigned int number;
	if (!pal::ParseDword(encoded, number)) return FALSE;
	val = number;
	return TRUE;
}

Bool SetStringInRegistry(AsciiString path, AsciiString key, AsciiString val)
{
	std::string section;
	if (!address(path, key, section)) return FALSE;
	if (static_cast<size_t>(val.getLength()) > pal::kMaxStringSize) return FALSE;
	return pal::WriteValue(section, key.str(),
	                       "SZ:" + pal::EncodeHex(reinterpret_cast<const unsigned char*>(val.str()), val.getLength()),
	                       kGameSections) ? TRUE : FALSE;
}

Bool SetUnsignedIntInRegistry(AsciiString path, AsciiString key, UnsignedInt val)
{
	std::string section;
	if (!address(path, key, section)) return FALSE;
	return pal::WriteValue(section, key.str(), "DWORD:" + std::to_string(val), kGameSections) ? TRUE : FALSE;
}

Bool SetBinaryInRegistry(AsciiString path, AsciiString key, const std::vector<unsigned char>& val)
{
	std::string section;
	if (!address(path, key, section)) return FALSE;
	if (val.size() > pal::kMaxBinarySize) return FALSE;
	return pal::WriteValue(section, key.str(), "BINARY:" + pal::EncodeHex(val.data(), val.size()), kGameSections) ? TRUE : FALSE;
}

Bool GetBinaryFromRegistry(AsciiString path, AsciiString key, std::vector<unsigned char>& val)
{
	std::string section, encoded;
	if (!address(path, key, section) || !pal::ReadValue(section, key.str(), "BINARY:", kGameSections, encoded)) return FALSE;
	std::vector<unsigned char> bytes;
	if (!pal::DecodeHex(encoded, bytes)) return FALSE;
	val.swap(bytes);
	return TRUE;
}

AsciiString GetRegistryLanguage(void)
{
	AsciiString val = "english";
	GetStringFromRegistry("", "Language", val);
	return val;
}

AsciiString GetRegistryGameName(void)
{
	AsciiString val = "GeneralsMPTest";
	GetStringFromRegistry("", "SKU", val);
	return val;
}

UnsignedInt GetRegistryVersion(void)
{
	UnsignedInt val = 65536;
	GetUnsignedIntFromRegistry("", "Version", val);
	return val;
}

UnsignedInt GetRegistryMapPackVersion(void)
{
	UnsignedInt val = 65536;
	GetUnsignedIntFromRegistry("", "MapPackVersion", val);
	return val;
}
#endif // __linux__
#endif // _WIN32
