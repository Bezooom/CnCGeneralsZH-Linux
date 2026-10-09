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

#define _WIN32_WINNT 0x0400

#include "thread.h"
#include "except.h"
#include "wwdebug.h"
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>
static inline unsigned long thread_gettid(void)
{
	return (unsigned long)syscall(SYS_gettid);
}
#else
#include <process.h>
#include <windows.h>
#endif
#if defined(_MSC_VER)
#pragma warning ( push )
#pragma warning ( disable : 4201 )
#endif
#include "systimer.h"
#if defined(_MSC_VER)
#pragma warning ( pop )
#endif


ThreadClass::ThreadClass(const char *thread_name, ExceptionHandlerType exception_handler) : handle(0), running(false), thread_priority(0)
{
	if (thread_name) {
		assert(strlen(thread_name) < sizeof(ThreadName) - 1);
		strcpy(ThreadName, thread_name);
	} else {
		strcpy(ThreadName, "No name");;
	}

	ExceptionHandler = exception_handler;
}

ThreadClass::~ThreadClass()
{
	Stop();
}

void __cdecl ThreadClass::Internal_Thread_Function(void* params)
{
	ThreadClass* tc=reinterpret_cast<ThreadClass*>(params);
	tc->running=true;
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
	tc->ThreadID = thread_gettid();
#else
	tc->ThreadID = GetCurrentThreadId();
#endif

#ifdef _WIN32
	Register_Thread_ID(tc->ThreadID, tc->ThreadName);

	if (tc->ExceptionHandler != NULL) {
		__try {
			tc->Thread_Function();
		} __except(tc->ExceptionHandler(GetExceptionCode(), GetExceptionInformation())) {};
	} else {
		tc->Thread_Function();
	}

#else //_WIN32
	tc->Thread_Function();
#endif //_WIN32

#ifdef _WIN32
	Unregister_Thread_ID(tc->ThreadID, tc->ThreadName);
#endif // _WIN32
	tc->handle=0;
	tc->ThreadID = 0;
}

void ThreadClass::Execute()
{
	WWASSERT(!handle);	// Only one thread at a time!
	#ifdef _UNIX
	#if defined(PLATFORM_LINUX) || defined(__linux__)
	{
		pthread_t tid;
		if (pthread_create(&tid, NULL, Pthread_Entry, this) == 0) {
			handle = (unsigned long)tid;
			WWDEBUG_SAY(("ThreadClass::Execute: Started thread %s, thread ID is %lX\n", ThreadName, handle));
		}
	}
	#else
		// assert(0);
		return;
	#endif
	#else
		handle=_beginthread(&Internal_Thread_Function,0,this);
		SetThreadPriority((HANDLE)handle,THREAD_PRIORITY_NORMAL+thread_priority);
		WWDEBUG_SAY(("ThreadClass::Execute: Started thread %s, thread ID is %X\n", ThreadName, handle));
	#endif
}

#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
void *ThreadClass::Pthread_Entry(void *params)
{
	Internal_Thread_Function(params);
	return NULL;
}
#endif

void ThreadClass::Set_Priority(int priority)
{
	#ifdef _UNIX
	#if defined(PLATFORM_LINUX) || defined(__linux__)
		thread_priority = priority;
		// pthread priority mapping is a Phase 5 polish item; stored for now.
		(void)priority;
	#else
		// assert(0);
		return;
	#endif
	#else
		thread_priority=priority;
		if (handle) SetThreadPriority((HANDLE)handle,THREAD_PRIORITY_NORMAL+thread_priority);
	#endif
}

void ThreadClass::Stop(unsigned ms)
{
	#ifdef _UNIX
	#if defined(PLATFORM_LINUX) || defined(__linux__)
	{
		running = false;
		unsigned time = TIMEGETTIME();
		struct timespec slice = { 0, 1000000L };
		while (handle) {
			if ((TIMEGETTIME() - time) > ms) {
				pthread_cancel((pthread_t)handle);
				pthread_join((pthread_t)handle, NULL);
				handle = 0;
				break;
			}
			nanosleep(&slice, NULL);
		}
	}
	#else
		// assert(0);
		return;
	#endif
	#else
		running=false;
		unsigned time=TIMEGETTIME();
		while (handle) {
			if ((TIMEGETTIME()-time)>ms) {
				int res=TerminateThread((HANDLE)handle,0);
				res;	// just to silence compiler warnings
				WWASSERT(res);	// Thread still not killed!
				handle=0;
			}
			Sleep(0);
		}
	#endif
}

void ThreadClass::Sleep_Ms(unsigned ms)
{
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
#else
	Sleep(ms);
#endif
}

#ifndef _UNIX
HANDLE test_event = ::CreateEvent (NULL, FALSE, FALSE, "");
#endif

void ThreadClass::Switch_Thread()
{
	#ifdef _UNIX
	#if defined(PLATFORM_LINUX) || defined(__linux__)
		sched_yield();
	#else
		return;
	#endif
	#else
		//	::SwitchToThread ();
		::WaitForSingleObject (test_event, 1);
		//	Sleep(1);	// Note! Parameter can not be 0 (or the thread switch doesn't occur)
	#endif
}

// Return calling thread's unique thread id
unsigned ThreadClass::_Get_Current_Thread_ID()
{
	#ifdef _UNIX
	#if defined(PLATFORM_LINUX) || defined(__linux__)
		return thread_gettid();
	#else
		return 0;
	#endif
	#else
		return GetCurrentThreadId();
	#endif
}

bool ThreadClass::Is_Running()
{
	return !!handle;
}