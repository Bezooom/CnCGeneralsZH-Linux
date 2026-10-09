/*
** Command & Conquer Generals ZH Reborn
** Platform Abstraction Layer — Dynamic Library Loading
**
** Portable abstractions for LoadLibrary / dlopen, GetProcAddress / dlsym,
** and FreeLibrary / dlclose.
*/

#pragma once

#if defined(_WIN32)
  #define PLATFORM_WINDOWS 1
  #include <windows.h>
#else
  #define PLATFORM_LINUX 1
  #include <dlfcn.h>
#endif

#include <cstddef>

inline void* Platform_LoadLibrary(const char* path)
{
    if (!path || !*path) return nullptr;
#if defined(PLATFORM_WINDOWS)
    return static_cast<void*>(LoadLibraryA(path));
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

inline void* Platform_GetProcAddress(void* handle, const char* symbol)
{
    if (!handle || !symbol || !*symbol) return nullptr;
#if defined(PLATFORM_WINDOWS)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), symbol));
#else
    return dlsym(handle, symbol);
#endif
}

inline bool Platform_FreeLibrary(void* handle)
{
    if (!handle) return false;
#if defined(PLATFORM_WINDOWS)
    return FreeLibrary(static_cast<HMODULE>(handle)) != 0;
#else
    return dlclose(handle) == 0;
#endif
}

inline const char* Platform_GetDynamicLibraryError()
{
#if defined(PLATFORM_WINDOWS)
    static thread_local char err_buf[256];
    DWORD err = GetLastError();
    if (err == 0) return "No error";
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, 0, err_buf, sizeof(err_buf), nullptr);
    return err_buf;
#else
    const char* err = dlerror();
    return err ? err : "No error";
#endif
}
