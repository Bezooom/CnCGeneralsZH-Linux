/*
** Command & Conquer Generals ZH Reborn
** Platform Abstraction Layer — File System & Paths
**
** Case-insensitive path resolution, path normalization, and XDG user directory support.
*/

#pragma once

#include <string>
#include <vector>

// Normalize path separators in place: converts backslashes to forward slashes,
// collapses redundant slashes (except leading for UNC paths).
void Platform_NormalizePath(std::string& path);

// Case-insensitive file path lookup.
// On Linux/POSIX filesystems, searches directory components case-insensitively
// to locate the actual existing file on disk. Returns true if found and writes
// the resolved real path into out_resolved.
// On Windows, simply checks file existence.
bool Platform_ResolveCasePath(const std::string& path, std::string& out_resolved);

// Check if a file or directory exists.
bool Platform_PathExists(const std::string& path);
bool Platform_IsDirectory(const std::string& path);

// Recursively ensure all parent directories exist for the specified path.
bool Platform_EnsureDirectoryExists(const std::string& dir_path);

// Return XDG data directory: $XDG_DATA_HOME/CnCGeneralsZH-Reborn
// or $HOME/.local/share/CnCGeneralsZH-Reborn on Linux;
// %APPDATA%/Command and Conquer Generals Zero Hour on Windows.
std::string Platform_GetUserDataDir();

// Return XDG config directory: $XDG_CONFIG_HOME/CnCGeneralsZH-Reborn
// or $HOME/.config/CnCGeneralsZH-Reborn on Linux.
std::string Platform_GetUserConfigDir();
