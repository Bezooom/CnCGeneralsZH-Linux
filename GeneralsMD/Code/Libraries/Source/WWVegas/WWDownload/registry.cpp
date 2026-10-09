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

// Registry.cpp
// Simple interface for storing/retreiving registry values
// Author: Matthew D. Campbell, December 2001

#include <string>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
// Linux: Win32 Registry API over a process-local store (see below).
#include "WinCompat.h"
#include <map>
#include <vector>
#endif

#include "Registry.h"

#if !defined(_MSC_VER)
// ---------------------------------------------------------------------------
// Linux backend for the Win32 Registry API used above and by the test.
// Process-local store: HKCU is fully read/write, HKLM is read-only for a
// normal user (writes fail, reads miss), which is exactly the split the
// tests pin (including the elevated-process escape hatch, which simply
// never triggers here). Value bytes are kept verbatim with their type so
// the type-ignoring getters and the 256-byte buffer edges behave like
// RegQueryValueEx. Cross-process persistence is a Phase-4 item (XDG files).
// ---------------------------------------------------------------------------

struct LinuxRegValue
{
  unsigned long type;
  std::vector<unsigned char> data;
};

// key path -> (value name -> value)
static std::map<std::string, std::map<std::string, LinuxRegValue> > &LinuxRegistry(void)
{
  static std::map<std::string, std::map<std::string, LinuxRegValue> > store;
  return store;
}

struct LinuxOpenKey
{
  std::string path;
};

static bool LinuxIsHKCU(HKEY root) { return root == HKEY_CURRENT_USER; }

LONG RegDeleteKeyA(HKEY hKey, LPCSTR lpSubKey)
{
  if (!LinuxIsHKCU(hKey) || !lpSubKey)
    return ERROR_ACCESS_DENIED;
  // erase the key and everything below it (flat keys in practice)
  std::string prefix(lpSubKey);
  prefix += '\\';
  std::map<std::string, std::map<std::string, LinuxRegValue> > &store = LinuxRegistry();
  size_t erased = 0;
  for (std::map<std::string, std::map<std::string, LinuxRegValue> >::iterator it = store.begin();
       it != store.end();)
  {
    if (it->first == lpSubKey || it->first.compare(0, prefix.size(), prefix) == 0)
    {
      std::map<std::string, std::map<std::string, LinuxRegValue> >::iterator kill = it++;
      store.erase(kill);
      ++erased;
    }
    else
      ++it;
  }
  return erased ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
}

LONG RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass,
                     DWORD dwOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                     PHKEY phkResult, LPDWORD lpdwDisposition)
{
  (void)Reserved; (void)lpClass; (void)dwOptions; (void)samDesired;
  (void)lpSecurityAttributes;
  if (!LinuxIsHKCU(hKey) || !lpSubKey || !phkResult)
    return ERROR_ACCESS_DENIED;
  std::map<std::string, std::map<std::string, LinuxRegValue> > &store = LinuxRegistry();
  bool isNew = store.find(lpSubKey) == store.end();
  if (isNew)
    store[lpSubKey]; // create the key
  LinuxOpenKey *open = new LinuxOpenKey;
  open->path = lpSubKey;
  *phkResult = (HKEY)open;
  if (lpdwDisposition)
    *lpdwDisposition = isNew ? REG_CREATED_NEW_KEY : REG_OPENED_EXISTING_KEY;
  return ERROR_SUCCESS;
}

LONG RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
  (void)ulOptions; (void)samDesired;
  if (!LinuxIsHKCU(hKey) || !lpSubKey || !phkResult)
    return ERROR_FILE_NOT_FOUND;
  if (LinuxRegistry().find(lpSubKey) == LinuxRegistry().end())
    return ERROR_FILE_NOT_FOUND;
  LinuxOpenKey *open = new LinuxOpenKey;
  open->path = lpSubKey;
  *phkResult = (HKEY)open;
  return ERROR_SUCCESS;
}

LONG RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
                      LPBYTE lpData, LPDWORD lpcbData)
{
  (void)lpReserved;
  if (!hKey || !lpValueName || !lpcbData)
    return ERROR_FILE_NOT_FOUND;
  LinuxOpenKey *open = (LinuxOpenKey *)hKey;
  std::map<std::string, std::map<std::string, LinuxRegValue> > &store = LinuxRegistry();
  std::map<std::string, std::map<std::string, LinuxRegValue> >::iterator key = store.find(open->path);
  if (key == store.end())
    return ERROR_FILE_NOT_FOUND;
  std::map<std::string, LinuxRegValue>::iterator val = key->second.find(lpValueName);
  if (val == key->second.end())
    return ERROR_FILE_NOT_FOUND;
  // NOTE: the type is deliberately not checked (matches the getters, pinned
  // by registry_DEFECT_getters_ignore_the_value_type).
  if (lpType)
    *lpType = val->second.type;
  if (*lpcbData < (unsigned long)val->second.data.size())
  {
    *lpcbData = (unsigned long)val->second.data.size();
    return ERROR_MORE_DATA;
  }
  if (!val->second.data.empty())
    memcpy(lpData, &val->second.data[0], val->second.data.size());
  *lpcbData = (unsigned long)val->second.data.size();
  return ERROR_SUCCESS;
}

LONG RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType,
                    const BYTE *lpData, DWORD cbData)
{
  (void)Reserved;
  if (!hKey || !lpValueName)
    return ERROR_ACCESS_DENIED;
  LinuxOpenKey *open = (LinuxOpenKey *)hKey;
  LinuxRegValue v;
  v.type = dwType;
  v.data.assign(lpData, lpData + cbData);
  LinuxRegistry()[open->path][lpValueName] = v;
  return ERROR_SUCCESS;
}

LONG RegCloseKey(HKEY hKey)
{
  delete (LinuxOpenKey *)hKey;
  return ERROR_SUCCESS;
}
#endif // !defined(_MSC_VER)

bool  getStringFromRegistry(HKEY root, std::string path, std::string key, std::string& val)
{
	HKEY handle;
	// Registry data is whatever is on disk: a zero-length value leaves buffer
	// untouched, and REG_BINARY/REG_DWORD data carries no NUL at all.  Both used
	// to hand the caller a std::string built from uninitialized stack.
	unsigned char buffer[256] = { 0 };
	DWORD size = sizeof(buffer);
	DWORD type;
	int returnValue;

	if ((returnValue = RegOpenKeyEx( root, path.c_str(), 0, KEY_READ, &handle )) == ERROR_SUCCESS)
	{
		returnValue = RegQueryValueEx(handle, key.c_str(), NULL, &type, (unsigned char *) &buffer, &size);
		RegCloseKey( handle );
	}

	if (returnValue == ERROR_SUCCESS)
	{
		buffer[sizeof(buffer)-1] = '\0';
		val = (char *)buffer;
		return true;
	}

	return false;
}

bool getUnsignedIntFromRegistry(HKEY root, std::string path, std::string key, unsigned int& val)
{
	HKEY handle;
	DWORD buffer = 0;
	DWORD size = sizeof(buffer);
	DWORD type;
	int returnValue;

	if ((returnValue = RegOpenKeyEx( root, path.c_str(), 0, KEY_READ, &handle )) == ERROR_SUCCESS)
	{
		returnValue = RegQueryValueEx(handle, key.c_str(), NULL, &type, (unsigned char *) &buffer, &size);
		RegCloseKey( handle );
	}

	if (returnValue == ERROR_SUCCESS)
	{
		val = buffer;
		return true;
	}

	return false;
}

bool setStringInRegistry( HKEY root, std::string path, std::string key, std::string val)
{
	HKEY handle;
	unsigned long type;
	unsigned long returnValue;
	int size;

	if ((returnValue = RegCreateKeyEx( root, path.c_str(), 0, "REG_NONE", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &handle, NULL )) == ERROR_SUCCESS)
	{
		type = REG_SZ;
		size = val.length()+1;
		returnValue = RegSetValueEx(handle, key.c_str(), 0, type, (unsigned char *)val.c_str(), size);
		RegCloseKey( handle );
	}

	return (returnValue == ERROR_SUCCESS);
}

bool setUnsignedIntInRegistry( HKEY root, std::string path, std::string key, unsigned int val)
{
	HKEY handle;
	unsigned long type;
	unsigned long returnValue;
	int size;

	if ((returnValue = RegCreateKeyEx( root, path.c_str(), 0, "REG_NONE", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &handle, NULL )) == ERROR_SUCCESS)
	{
		type = REG_DWORD;
		size = 4;
		returnValue = RegSetValueEx(handle, key.c_str(), 0, type, (unsigned char *)&val, size);
		RegCloseKey( handle );
	}

	return (returnValue == ERROR_SUCCESS);
}

bool GetStringFromRegistry(std::string path, std::string key, std::string& val)
{
	std::string fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour";

	fullPath.append(path);
	if (getStringFromRegistry(HKEY_LOCAL_MACHINE, fullPath.c_str(), key.c_str(), val))
	{
		return true;
	}

	return getStringFromRegistry(HKEY_CURRENT_USER, fullPath.c_str(), key.c_str(), val);
}

bool GetUnsignedIntFromRegistry(std::string path, std::string key, unsigned int& val)
{
	std::string fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour";

	fullPath.append(path);
	if (getUnsignedIntFromRegistry(HKEY_LOCAL_MACHINE, fullPath.c_str(), key.c_str(), val))
	{
		return true;
	}

	return getUnsignedIntFromRegistry(HKEY_CURRENT_USER, fullPath.c_str(), key.c_str(), val);
}

bool SetStringInRegistry( std::string path, std::string key, std::string val)
{
	std::string fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour";
	fullPath.append(path);

	if (setStringInRegistry( HKEY_LOCAL_MACHINE, fullPath, key, val))
		return true;

	return setStringInRegistry( HKEY_CURRENT_USER, fullPath, key, val );
}

bool SetUnsignedIntInRegistry( std::string path, std::string key, unsigned int val)
{
	std::string fullPath = "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour";
	fullPath.append(path);

	if (setUnsignedIntInRegistry( HKEY_LOCAL_MACHINE, fullPath, key, val))
		return true;

	return setUnsignedIntInRegistry( HKEY_CURRENT_USER, fullPath, key, val );
}

