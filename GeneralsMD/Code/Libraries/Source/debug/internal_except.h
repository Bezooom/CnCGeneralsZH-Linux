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

/////////////////////////////////////////////////////////////////////////EA-V1
// $File: //depot/GeneralsMD/Staging/code/Libraries/Source/debug/internal_except.h $
// $Author: mhoffe $
// $Revision: #1 $
// $DateTime: 2003/07/03 11:55:26 $
//
// �2003 Electronic Arts
//
// Unhandled exception handler
//////////////////////////////////////////////////////////////////////////////
#ifdef _MSC_VER
#  pragma once
#endif
#ifndef INTERNAL_EXCEPT_H // Include guard
#define INTERNAL_EXCEPT_H

#if !defined(_MSC_VER)
// Linux twins of the Win32 exception records. Only the fields the handler
// actually reads are present; a signal handler (debug_debug.cpp) fills them
// from siginfo_t/ucontext_t before calling ExceptionFilter.
struct _EXCEPTION_RECORD
{
  unsigned ExceptionCode;
  unsigned ExceptionFlags;
  void *ExceptionAddress;
};
struct _CONTEXT
{
  unsigned long Rip, Rsp, Rbp;
  unsigned long Rax, Rbx, Rcx, Rdx, Rsi, Rdi;
  unsigned long R8, R9, R10, R11, R12, R13, R14, R15;
  unsigned long EFlags;
};
struct _EXCEPTION_POINTERS
{
  struct _EXCEPTION_RECORD *ExceptionRecord;
  struct _CONTEXT *ContextRecord;
};
// Signal numbers used as ExceptionCode values on Linux.
#define EXCEPTION_LINUX_SIGNAL(sig) (0x4C580000u | (unsigned)(sig))

// Installs fatal-signal handlers that funnel into ExceptionFilter.
// Called once from Debug::PreStaticInit().
void Debug_InstallSignalHandlers(void);
#endif

/// \internal exception handler
class DebugExceptionhandler
{
  DebugExceptionhandler(const DebugExceptionhandler&);
  DebugExceptionhandler& operator=(const DebugExceptionhandler&);

  // nobody can instantiate us
  DebugExceptionhandler(void);

  /** \internal

    \brief Log exception location.

    \param dbg debug instance
    \param exptr exception pointers
  */
  static void LogExceptionLocation(Debug &dbg, struct _EXCEPTION_POINTERS *exptr);

  /** \internal

    \brief Log regular registers.

    \param dbg debug instance
    \param exptr exception pointers
  */
  static void LogRegisters(Debug &dbg, struct _EXCEPTION_POINTERS *exptr);

  /** \internal

    \brief Log FPU registers.

    \param dbg debug instance
    \param exptr exception pointers
  */
  static void LogFPURegisters(Debug &dbg, struct _EXCEPTION_POINTERS *exptr);

public:
  
  /** \internal

    \brief Determine exception type.

    \param exptr exception pointers
    \param explanation exception explanation, buffer must be 512 chars
    \return exception type as string
  */
  static const char *GetExceptionType(struct _EXCEPTION_POINTERS *exptr, char *explanation);

  /** \internal 
   
    \brief System exception filter
  */
#if defined(_MSC_VER)
  static long __stdcall ExceptionFilter(struct _EXCEPTION_POINTERS* pExPtrs);
#else
  static long ExceptionFilter(struct _EXCEPTION_POINTERS* pExPtrs);
#endif
};

#endif // INTERNAL_EXCEPT_H
