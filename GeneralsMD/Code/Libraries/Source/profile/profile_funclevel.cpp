/*
**  Function-level Profiling ? Cross-platform
**  Windows: MSVC _penter/_pleave naked functions
**  Linux: GCC/Clang -finstrument-functions (__cyg_profile_func_enter/exit)
*/

#include "_pch.h"
#include "../debug/debug.h"
#include <new>

#ifdef HAS_PROFILE

// Platform abstraction
#include "PAL_Timing.h"

#if defined(_WIN32)
  // ========== WINDOWS: MSVC _penter/_pleave ==========
  #include <windows.h>

  // TLS index (-1 if not yet initialized)
  static DWORD TLSIndex = TLS_OUT_OF_INDEXES;

  // our own fast critical section
  static ProfileFastCS cs;

  // MSVC naked function for function exit
  static void __declspec(naked) _pleave(void)
  {
    ProfileFuncLevelTracer *p;
    unsigned curESP, leaveAddr;

    __asm
    {
      push ebp
      push ebp
      push eax
      mov  ebp, esp
      mov  eax, esp
      sub  esp, 3*4
      pushad
      mov  [curESP], eax
    }

    curESP += 8;
    p = (ProfileFuncLevelTracer *)TlsGetValue(TLSIndex);
    leaveAddr = p->Leave(curESP);
    *(unsigned *)(curESP) = leaveAddr;

    __asm
    {
      popad
      add  esp, 3*4
      pop  eax
      pop  ebp
      ret
    }
  }

  extern "C" void __declspec(naked) _cdecl _penter(void)
  {
    unsigned callerFunc, ESPonReturn, callerRet;
    ProfileFuncLevelTracer *p;

    __asm
    {
      push ebp
      push eax
      mov  ebp, esp
      mov  eax, esp
      sub  esp, 4*4
      pushad

      add  eax, 4+4
      mov  ebx, [eax]
      mov  [callerFunc], ebx

      add  eax, 4
      mov  [ESPonReturn], eax
      mov  ebx, [eax]
      mov  [callerRet], ebx

      mov  dword ptr [eax], offset _pleave
    }

    if (TLSIndex == TLS_OUT_OF_INDEXES)
      TLSIndex = TlsAlloc();
    p = (ProfileFuncLevelTracer *)TlsGetValue(TLSIndex);
    if (!p)
    {
      p = (ProfileFuncLevelTracer *)ProfileAllocMemory(sizeof(ProfileFuncLevelTracer));
      new (p) ProfileFuncLevelTracer;
      TlsSetValue(TLSIndex, p);
    }

    p->Enter(callerFunc - 5, ESPonReturn, callerRet);

    __asm
    {
      popad
      add  esp, 4*4
      pop  eax
      pop  ebp
      ret
    }
  }

#else
  // ========== LINUX: GCC/Clang -finstrument-functions ==========
  // Compile with: -finstrument-functions -finstrument-functions-exclude-file-list=...

  #include <pthread.h>
  #include <dlfcn.h>

  // TLS key for per-thread tracer
  static pthread_key_t tls_key;
  static bool tls_key_initialized = false;
  static ProfileFastCS cs;

  // Forward declaration
  class ProfileFuncLevelTracer;

  // Thread-local tracer accessor
  static ProfileFuncLevelTracer* get_thread_tracer() {
    if (!tls_key_initialized) {
      pthread_key_create(&tls_key, [](void* ptr) {
        delete static_cast<ProfileFuncLevelTracer*>(ptr);
      });
      tls_key_initialized = true;
    }
    ProfileFuncLevelTracer* p = static_cast<ProfileFuncLevelTracer*>(pthread_getspecific(tls_key));
    if (!p) {
      p = new ProfileFuncLevelTracer();
      pthread_setspecific(tls_key, p);
    }
    return p;
  }

  // Called at function entry by GCC/Clang
  extern "C" void __cyg_profile_func_enter(void* func, void* caller) {
    ProfileFuncLevelTracer* p = get_thread_tracer();
    if (p) {
      // func = address of function being entered
      // caller = return address (caller)
      p->Enter(reinterpret_cast<uintptr_t>(func), 0, reinterpret_cast<uintptr_t>(caller));
    }
  }

  // Called at function exit by GCC/Clang
  extern "C" void __cyg_profile_func_exit(void* func, void* caller) {
    ProfileFuncLevelTracer* p = static_cast<ProfileFuncLevelTracer*>(pthread_getspecific(tls_key));
    if (p) {
      p->Leave(0); // ESP not easily available on Linux
    }
  }

#endif // _WIN32

// ========== COMMON IMPLEMENTATION ==========

ProfileFuncLevelTracer *ProfileFuncLevelTracer::head = NULL;
bool ProfileFuncLevelTracer::shuttingDown = false;
int ProfileFuncLevelTracer::curFrame = 0;
unsigned ProfileFuncLevelTracer::frameRecordMask;
bool ProfileFuncLevelTracer::recordCaller = false;

ProfileFuncLevelTracer::ProfileFuncLevelTracer(void):
  stack(NULL), usedStack(0), totalStack(0), maxDepth(0)
{
  ProfileFastCS::Lock lock(cs);
  next = head;
  head = this;
}

ProfileFuncLevelTracer::~ProfileFuncLevelTracer()
{
  // yes, I know we leak...
}

void ProfileFuncLevelTracer::Enter(unsigned addr, unsigned esp, unsigned ret)
{
  // must stack grow?
  if (usedStack >= totalStack)
    stack = (StackEntry *)ProfileReAllocMemory(stack, (totalStack += 100) * sizeof(StackEntry));

  // save info
  Function *f = func.Find(addr);
  if (!f)
  {
    // new function
    f = (Function *)ProfileAllocMemory(sizeof(Function));
    new (f) Function(this);
    f->addr = addr;
    f->glob.callCount = f->glob.tickPure = f->glob.tickTotal = 0;
    for (int i = 0; i < MAX_FRAME_RECORDS; i++)
      f->cur[i].callCount = f->cur[i].tickPure = f->cur[i].tickTotal = 0;
    f->depth = 0;
    func.Insert(f);
  }

  StackEntry &s = stack[usedStack++];
  s.func = f;
  s.esp = esp;
  s.retVal = ret;
  ProfileGetTime(s.tickEnter);
  s.tickSubTime = 0;
  f->depth++;

  // new max depth?
  if (usedStack >= maxDepth)
    maxDepth = usedStack;

  DLOG_GROUP(profile_stack, Debug::RepeatChar(' ', usedStack - 1)
                           << Debug::Hex() << this
                           << " Enter " << Debug::Width(8) << addr
                           << " ESP "   << Debug::Width(8) << esp
                           << " return "   << Debug::Width(8) << ret
                           << " level " << Debug::Dec() << usedStack);
}

unsigned ProfileFuncLevelTracer::Leave(unsigned esp)
{
  // get current "time"
  __int64 cur;
  ProfileGetTime(cur);

  while (usedStack > 0)
  {
    // leave current function
    usedStack--;
    StackEntry &s = stack[usedStack],
               &sPrev = stack[usedStack - 1];

    Function *f = s.func;

    // decrease call depth
    f->depth--;

    // insert caller
    if (recordCaller && usedStack)
      f->glob.caller.Insert(sPrev.func->addr, 1);

    // inc call counter
    f->glob.callCount++;

    // add total time
    __int64 delta = cur - s.tickEnter;
    if (!f->depth)
      f->glob.tickTotal += delta;

    // add pure time
    f->glob.tickPure += delta - s.tickSubTime;

    // add sub time for higher function
    if (usedStack)
      sPrev.tickSubTime += delta;

    // frame based profiling?
    if (frameRecordMask)
    {
      unsigned mask = frameRecordMask;
      for (unsigned i = 0; i < MAX_FRAME_RECORDS; i++)
      {
        if (mask & 1)
        {
          if (recordCaller && usedStack > 0)
            f->cur[i].caller.Insert(sPrev.func->addr, 1);
          f->cur[i].callCount++;
          if (!f->depth)
            f->cur[i].tickTotal += delta;
          f->cur[i].tickPure += delta - s.tickSubTime;
        }
        if (!(mask >>= 1))
          break;
      }
    }

    // exit if address match (somewhat...)
    if (s.esp == esp)
      break;

    // catching those nasty ret<n>...
    if (s.esp < esp &&
        (esp - s.esp) % 4 == 0 &&
        (esp - s.esp) < 256)
      break;

    // emit warning
    DCRASH("ESP " << Debug::Hex() << esp << " does not match " << stack[usedStack].esp << Debug::Dec());
  }

  DLOG_GROUP(profile_stack, Debug::RepeatChar(' ', usedStack - 1)
                           << Debug::Hex() << this
                           << " Leave " << Debug::Width(8) << ""
                           << " ESP "   << Debug::Width(8) << stack[usedStack].esp
                           << " return "   << Debug::Width(8) << stack[usedStack].retVal
                           << " level " << Debug::Dec() << usedStack);

  return stack[usedStack].retVal;
}

void ProfileFuncLevelTracer::Shutdown(void)
{
  if (frameRecordMask)
  {
    for (unsigned i = 0; i < MAX_FRAME_RECORDS; i++)
      if (frameRecordMask & (1 << i))
        for (ProfileFuncLevelTracer *p = head; p; p = p->next)
          p->FrameEnd(i, -1);
  }
}

int ProfileFuncLevelTracer::FrameStart(void)
{
  ProfileFastCS::Lock lock(cs);

  for (unsigned i = 0; i < MAX_FRAME_RECORDS; i++)
    if (!(frameRecordMask & (1 << i)))
      break;
  if (i == MAX_FRAME_RECORDS)
    return -1;

  for (ProfileFuncLevelTracer *p = head; p; p = p->next)
  {
    Function *f;
    for (int k = 0; (f = p->func.Enumerate(k)) != NULL; k++)
    {
      Profile &p = f->cur[i];
      p.caller.Clear();
      p.callCount = p.tickPure = p.tickTotal = 0;
    }
  }

  frameRecordMask |= 1 << i;
  return i;
}

void ProfileFuncLevelTracer::FrameEnd(int which, int mixIndex)
{
  DFAIL_IF(which < 0 || which >= MAX_FRAME_RECORDS)
    return;
  DFAIL_IF(!(frameRecordMask & (1 << which)))
    return;
  DFAIL_IF(mixIndex >= curFrame)
    return;

  ProfileFastCS::Lock lock(cs);

  frameRecordMask ^= 1 << which;
  if (mixIndex < 0)
    curFrame++;
  for (ProfileFuncLevelTracer *p = head; p; p = p->next)
  {
    Function *f;
    for (int k = 0; (f = p->func.Enumerate(k)) != NULL; k++)
    {
      Profile &p = f->cur[which];
      if (p.callCount)
      {
        if (mixIndex < 0)
          f->frame.Append(curFrame, p);
        else
          f->frame.MixIn(mixIndex, p);
      }
      p.caller.Clear();
      p.callCount = p.tickPure = p.tickTotal = 0;
    }
  }
}

void ProfileFuncLevelTracer::ClearTotals(void)
{
  ProfileFastCS::Lock lock(cs);

  for (ProfileFuncLevelTracer *p = head; p; p = p->next)
  {
    Function *f;
    for (int k = 0; (f = p->func.Enumerate(k)) != NULL; k++)
    {
      f->glob.caller.Clear();
      f->glob.callCount = 0;
      f->glob.tickPure = 0;
      f->glob.tickTotal = 0;
    }
  }
}

// ... rest of the implementation unchanged (UnsignedMap, ProfileMap, FunctionMap, etc.)
// Include the original implementation from line 360 onwards

#include "profile_funclevel_common.inl"

#else // !defined HAS_PROFILE

// Stub implementations when profiling disabled
bool ProfileFuncLevel::IdList::Enum(unsigned index, Id &id, unsigned *) const { return false; }
const char *ProfileFuncLevel::Id::GetSource(void) const { return NULL; }
const char *ProfileFuncLevel::Id::GetFunction(void) const { return NULL; }
unsigned ProfileFuncLevel::Id::GetAddress(void) const { return 0; }
unsigned ProfileFuncLevel::Id::GetLine(void) const { return 0; }
unsigned long long ProfileFuncLevel::Id::GetCalls(unsigned frame) const { return 0; }
unsigned long long ProfileFuncLevel::Id::GetTime(unsigned frame) const { return 0; }
unsigned long long ProfileFuncLevel::Id::GetFunctionTime(unsigned frame) const { return 0; }
ProfileFuncLevel::IdList ProfileFuncLevel::Id::GetCaller(unsigned frame) const { return ProfileFuncLevel::IdList(); }
bool ProfileFuncLevel::Thread::EnumProfile(unsigned index, Id &id) const { return false; }
bool ProfileFuncLevel::EnumThreads(unsigned index, Thread &thread) { return false; }
ProfileFuncLevel::ProfileFuncLevel(void) {}

#endif // !defined HAS_PROFILE

ProfileFuncLevel ProfileFuncLevel::Instance;
#if defined(_MSC_VER)
HANDLE ProfileFastCS::testEvent = ::CreateEvent(NULL, FALSE, FALSE, "");
#else
// Linux: the yield in ThreadSafeSetFlag needs no event object.
#endif