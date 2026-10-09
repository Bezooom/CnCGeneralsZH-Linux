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

#include "mutex.h"
#include "wwdebug.h"
#if defined(PLATFORM_LINUX) || defined(__linux__) || defined(_UNIX)
#include <pthread.h>
#include <time.h>
#include <errno.h>
// Windows mutexes/critical sections are recursive; match that so existing
// nested LockClass use keeps working.
static pthread_mutex_t *new_recursive_mutex(void)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_t *m = new pthread_mutex_t;
	pthread_mutex_init(m, &attr);
	pthread_mutexattr_destroy(&attr);
	return m;
}
#else
#include <windows.h>
#endif


// ----------------------------------------------------------------------------

MutexClass::MutexClass(const char* name) : handle(NULL), locked(false)
{
	#ifdef _UNIX
		(void)name;
		handle = new_recursive_mutex();
		WWASSERT(handle);
	#else
		handle=CreateMutex(NULL,false,name);
		WWASSERT(handle);
	#endif
}

MutexClass::~MutexClass()
{
	#ifdef _UNIX
		WWASSERT(!locked); // Can't delete locked mutex!
		pthread_mutex_destroy((pthread_mutex_t *)handle);
		delete (pthread_mutex_t *)handle;
	#else
		WWASSERT(!locked); // Can't delete locked mutex!
		CloseHandle(handle);
	#endif
}

bool MutexClass::Lock(int time)
{
	#ifdef _UNIX
		pthread_mutex_t *m = (pthread_mutex_t *)handle;
		int res;
		if (time == WAIT_INFINITE) {
			res = pthread_mutex_lock(m);
		} else if (time <= 0) {
			res = pthread_mutex_trylock(m);
		} else {
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += time / 1000;
			ts.tv_nsec += (long)(time % 1000) * 1000000L;
			if (ts.tv_nsec >= 1000000000L) {
				ts.tv_sec += 1;
				ts.tv_nsec -= 1000000000L;
			}
			res = pthread_mutex_timedlock(m, &ts);
		}
		if (res != 0)
			return false;
		locked++;
		return true;
	#else
		int res = WaitForSingleObject(handle,time==WAIT_INFINITE ? INFINITE : time);
		if (res!=WAIT_OBJECT_0) return false;
		locked++;
		return true;
	#endif
}

void MutexClass::Unlock()
{
	#ifdef _UNIX
		WWASSERT(locked);
		locked--;
		pthread_mutex_unlock((pthread_mutex_t *)handle);
	#else
		WWASSERT(locked);
		locked--;
		int res=ReleaseMutex(handle);
		res;	// silence compiler warnings
		WWASSERT(res);
	#endif
}

// ----------------------------------------------------------------------------

MutexClass::LockClass::LockClass(MutexClass& mutex_,int time) : mutex(mutex_)
{
	failed=!mutex.Lock(time);
}

MutexClass::LockClass::~LockClass()
{
	if (!failed) mutex.Unlock();
}







// ----------------------------------------------------------------------------

CriticalSectionClass::CriticalSectionClass() : handle(NULL), locked(false)
{
	#ifdef _UNIX
		handle = new_recursive_mutex();
	#else
		handle=W3DNEWARRAY char[sizeof(CRITICAL_SECTION)];
		InitializeCriticalSection((CRITICAL_SECTION*)handle);
	#endif
}

CriticalSectionClass::~CriticalSectionClass()
{
	#ifdef _UNIX
		WWASSERT(!locked); // Can't delete locked mutex!
		pthread_mutex_destroy((pthread_mutex_t *)handle);
		delete (pthread_mutex_t *)handle;
	#else
		WWASSERT(!locked); // Can't delete locked mutex!
		DeleteCriticalSection((CRITICAL_SECTION*)handle);
		delete[] handle;
	#endif
}

void CriticalSectionClass::Lock()
{
	#ifdef _UNIX
		pthread_mutex_lock((pthread_mutex_t *)handle);
		locked++;
	#else
		EnterCriticalSection((CRITICAL_SECTION*)handle);
		locked++;
	#endif
}

void CriticalSectionClass::Unlock()
{
	#ifdef _UNIX
		WWASSERT(locked);
		locked--;
		pthread_mutex_unlock((pthread_mutex_t *)handle);
	#else
		WWASSERT(locked);
		locked--;
		LeaveCriticalSection((CRITICAL_SECTION*)handle);
	#endif
}

// ----------------------------------------------------------------------------

CriticalSectionClass::LockClass::LockClass(CriticalSectionClass& critical_section) : CriticalSection(critical_section)
{
	CriticalSection.Lock();
}

CriticalSectionClass::LockClass::~LockClass()
{
	CriticalSection.Unlock();
}


