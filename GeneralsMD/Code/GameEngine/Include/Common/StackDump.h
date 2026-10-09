/*
**  Cross-platform Stack Dump API
**  Windows: uses DbgHelp (original implementation)
**  Linux: uses libunwind + dladdr
*/

#pragma once

#ifndef __STACKDUMP_H_
#define __STACKDUMP_H_

#if !defined(IG_DEBUG_STACKTRACE)
#define IG_DEBUG_STACKTRACE 1
#endif

#if defined(_DEBUG) || defined(_INTERNAL) || defined(IG_DEBUG_STACKTRACE)

#include "Lib/BaseType.h"
#include "Common/AsciiString.h"

// Callback type for stack walk output
using StackDumpCallback = void (*)(const char*);

// Capture current stack and call callback per frame
void StackDump(StackDumpCallback callback = nullptr);

// Capture stack from specific context (Windows SEH / Linux signal handler)
#if defined(_WIN32)
#include <windows.h>
void StackDumpFromContext(DWORD eip, DWORD esp, DWORD ebp, StackDumpCallback callback = nullptr);
void DumpExceptionInfo(unsigned int u, EXCEPTION_POINTERS* e_info);
#else
// Linux: ucontext_t from signal handler
#include <signal.h>
#include <ucontext.h>
void StackDumpFromContext(ucontext_t* uc, StackDumpCallback callback = nullptr);
void DumpExceptionInfo(int sig, siginfo_t* info, void* ucontext);
void Platform_InstallCrashHandlers();
#endif

// Get raw return addresses (no symbols)
void FillStackAddresses(void** addresses, unsigned int count, unsigned int skip = 0);

// Symbolize pre-captured addresses
void StackDumpFromAddresses(void** addresses, unsigned int count, StackDumpCallback callback = nullptr);

// Get function details for a single address
void GetFunctionDetails(void* pointer, char* name, size_t nameSize,
                        char* filename, size_t filenameSize,
                        unsigned int* linenumber, unsigned int* address);

// Last error dump (for crash reporting)
extern AsciiString g_LastErrorDump;

#else // !DEBUG

inline void StackDump(StackDumpCallback = nullptr) {}
inline void StackDumpFromContext(
#if defined(_WIN32)
    DWORD, DWORD, DWORD,
#else
    ucontext_t*,
#endif
    StackDumpCallback = nullptr) {}
inline void FillStackAddresses(void**, unsigned int, unsigned int = 0) {}
inline void StackDumpFromAddresses(void**, unsigned int, StackDumpCallback = nullptr) {}
inline void GetFunctionDetails(void*, char*, size_t, char*, size_t, unsigned int*, unsigned int*) {}
#if defined(_WIN32)
inline void DumpExceptionInfo(unsigned int, EXCEPTION_POINTERS*) {}
#else
inline void DumpExceptionInfo(int, siginfo_t*, void*) {}
#endif

extern AsciiString g_LastErrorDump;

#endif // DEBUG

#endif // __STACKDUMP_H_