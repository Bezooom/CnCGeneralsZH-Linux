/*
** Command & Conquer Generals ZH Reborn
** Platform Abstraction Layer — File System & Paths
*/

#include "PAL_FileSystem.h"

#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace fs = std::filesystem;

void Platform_NormalizePath(std::string& path)
{
    for (char& c : path) {
        if (c == '\\') {
            c = '/';
        }
    }

    // Collapse multiple consecutive slashes, preserving leading double-slash for UNC/network if needed
    std::string normalized;
    normalized.reserve(path.size());
    bool last_was_slash = false;
    for (size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        if (c == '/') {
            if (!last_was_slash || i == 1) { // allow double slash at start
                normalized.push_back(c);
            }
            last_was_slash = true;
        } else {
            normalized.push_back(c);
            last_was_slash = false;
        }
    }
    path = std::move(normalized);
}

bool Platform_PathExists(const std::string& path)
{
    std::error_code ec;
    return fs::exists(path, ec);
}

bool Platform_IsDirectory(const std::string& path)
{
    std::error_code ec;
    return fs::is_directory(path, ec);
}

bool Platform_EnsureDirectoryExists(const std::string& dir_path)
{
    std::error_code ec;
    return fs::create_directories(dir_path, ec);
}

bool Platform_ResolveCasePath(const std::string& in_path, std::string& out_resolved)
{
    if (in_path.empty()) {
        out_resolved.clear();
        return false;
    }

    std::string norm = in_path;
    Platform_NormalizePath(norm);

    std::error_code ec;
    if (fs::exists(norm, ec)) {
        out_resolved = norm;
        return true;
    }

    fs::path p(norm);
    fs::path current;
    auto it = p.begin();

    if (p.is_absolute()) {
        current = *it;
        ++it;
    } else {
        current = ".";
    }

    for (; it != p.end(); ++it) {
        const std::string comp = it->string();
        if (comp == "." || comp.empty()) {
            continue;
        }
        if (comp == "..") {
            current = current.parent_path();
            continue;
        }

        fs::path exact = current / comp;
        if (fs::exists(exact, ec)) {
            current = exact;
            continue;
        }

        bool matched = false;
        if (fs::is_directory(current, ec)) {
            for (const auto& entry : fs::directory_iterator(current, fs::directory_options::skip_permission_denied, ec)) {
                if (strcasecmp(entry.path().filename().string().c_str(), comp.c_str()) == 0) {
                    current = entry.path();
                    matched = true;
                    break;
                }
            }
        }

        if (!matched) {
            return false;
        }
    }

    out_resolved = current.lexically_normal().string();
    return true;
}

std::string Platform_GetUserDataDir()
{
    const char* xdg_data = getenv("XDG_DATA_HOME");
    if (xdg_data && *xdg_data) {
        return std::string(xdg_data) + "/CnCGeneralsZH-Reborn";
    }
    const char* home = getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.local/share/CnCGeneralsZH-Reborn";
    }
    return "/tmp/CnCGeneralsZH-Reborn";
}

std::string Platform_GetUserConfigDir()
{
    const char* xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config && *xdg_config) {
        return std::string(xdg_config) + "/CnCGeneralsZH-Reborn";
    }
    const char* home = getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.config/CnCGeneralsZH-Reborn";
    }
    return "/tmp/CnCGeneralsZH-Reborn";
}
