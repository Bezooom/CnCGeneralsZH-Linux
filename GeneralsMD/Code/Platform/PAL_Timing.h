/*
**  Platform Abstraction Layer — Timing & Threading
**  Portable replacements for Win32 inline assembly
*/

#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>

#if defined(_WIN32)
  #define PLATFORM_WINDOWS 1
  #include <intrin.h>
  #include <windows.h>
#else
  #define PLATFORM_LINUX 1
  #include <time.h>
  #include <x86intrin.h>  // GCC/Clang intrinsics for RDTSC, etc.
  #include <pthread.h>
  #include <unistd.h>
#endif

// ------------------------------------------------------------
// Calling convention macros
// ------------------------------------------------------------
#if defined(_MSC_VER)
#define PLATFORM_CDECL __cdecl
#define PLATFORM_STDCALL __stdcall
#define PLATFORM_FASTCALL __fastcall
#else
#define PLATFORM_CDECL
#define PLATFORM_STDCALL
#define PLATFORM_FASTCALL
#endif

// Force inline macros
#if defined(_MSC_VER)
#define PLATFORM_FORCEINLINE __forceinline
#else
#define PLATFORM_FORCEINLINE inline __attribute__((always_inline))
#endif
#if defined(PLATFORM_WINDOWS)
  #define PLATFORM_RDTSC() __rdtsc()
#else
  // GCC/Clang: __builtin_ia32_rdtsc() or inline asm
  #if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    #define PLATFORM_RDTSC() __builtin_ia32_rdtsc()
  #else
    // Fallback: clock_gettime (nanoseconds)
    inline uint64_t platform_rdtsc_fallback() {
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
      return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }
    #define PLATFORM_RDTSC() platform_rdtsc_fallback()
  #endif
#endif

// ------------------------------------------------------------
// Fast spinlock (replaces ProfileFastCS)
// ------------------------------------------------------------
class PlatformSpinLock {
  alignas(64) std::atomic<uint32_t> flag_{0};

public:
  PlatformSpinLock() = default;
  PlatformSpinLock(const PlatformSpinLock&) = delete;
  PlatformSpinLock& operator=(const PlatformSpinLock&) = delete;

  void lock() noexcept {
    uint32_t expected = 0;
    // x86: pause instruction in spin loop
    while (!flag_.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
      expected = 0;
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
      __builtin_ia32_pause();
#elif defined(_MSC_VER)
      _mm_pause();
#endif
    }
  }

  void unlock() noexcept {
    flag_.store(0, std::memory_order_release);
  }

  class ScopedLock {
    PlatformSpinLock& lock_;
  public:
    explicit ScopedLock(PlatformSpinLock& l) noexcept : lock_(l) { lock_.lock(); }
    ~ScopedLock() noexcept { lock_.unlock(); }
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;
  };
};

// ------------------------------------------------------------
// Thread-local storage (replaces __declspec(thread) / TlsAlloc)
// ------------------------------------------------------------
template<typename T>
class PlatformTLS {
#if defined(PLATFORM_WINDOWS)
  DWORD tls_index_ = TLS_OUT_OF_INDEXES;
public:
  PlatformTLS() { tls_index_ = TlsAlloc(); }
  ~PlatformTLS() { if (tls_index_ != TLS_OUT_OF_INDEXES) TlsFree(tls_index_); }
  T* get() noexcept { return static_cast<T*>(TlsGetValue(tls_index_)); }
  void set(T* ptr) noexcept { TlsSetValue(tls_index_, ptr); }
#else
  pthread_key_t tls_key_;
public:
  PlatformTLS() { pthread_key_create(&tls_key_, nullptr); }
  ~PlatformTLS() { pthread_key_delete(tls_key_); }
  T* get() noexcept { return static_cast<T*>(pthread_getspecific(tls_key_)); }
  void set(T* ptr) noexcept { pthread_setspecific(tls_key_, ptr); }
#endif
};

// ------------------------------------------------------------
// High-resolution timer initialization (replaces InitPrecisionTimer)
// ------------------------------------------------------------
struct PlatformTimerCalibration {
  uint64_t ticks_per_sec = 0;
  double ticks_per_ms = 0.0;
  double ticks_per_us = 0.0;
};

extern PlatformTimerCalibration g_timer_calibration;

void Platform_InitPrecisionTimer();
inline uint64_t Platform_GetPrecisionTimer() { return PLATFORM_RDTSC(); }
inline void Platform_GetPrecisionTimer(uint64_t* t) { *t = PLATFORM_RDTSC(); }
inline uint64_t Platform_GetTicksPerSec() { return g_timer_calibration.ticks_per_sec; }
inline double Platform_GetTicksPerMSec() { return g_timer_calibration.ticks_per_ms; }
inline double Platform_GetTicksPerUSec() { return g_timer_calibration.ticks_per_us; }

// ------------------------------------------------------------
// CRC32 (replaces inline asm in crc.h)
// ------------------------------------------------------------
uint32_t Platform_CRC32(const void* data, size_t len, uint32_t crc = 0);
uint32_t Platform_CRC32_Combine(uint32_t crc1, uint32_t crc2, size_t len2);

// ------------------------------------------------------------
// Float-to-int conversion (replaces wwmath.h Float_To_Long)
// ------------------------------------------------------------
inline long Platform_FloatToLong(float f) {
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && defined(__SSE2__)
  // SSE2: cvttss2si - truncate toward zero
  long retval;
  __asm__("cvttss2si %1, %0" : "=r"(retval) : "x"(f));
  return retval;
#elif defined(_MSC_VER) && defined(_M_IX86)
  long retval;
  __asm fld dword ptr [f]
  __asm fistp dword ptr [retval]
  return retval;
#else
  return static_cast<long>(f);  // C++ truncates toward zero
#endif
}

inline long Platform_DoubleToLong(double f) {
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && defined(__SSE2__)
  long retval;
  __asm__("cvttsd2si %1, %0" : "=r"(retval) : "x"(f));
  return retval;
#elif defined(_MSC_VER) && defined(_M_IX86)
  long retval;
  __asm fld qword ptr [f]
  __asm fistp dword ptr [retval]
  return retval;
#else
  return static_cast<long>(f);
#endif
}

// ------------------------------------------------------------
// Stack walking (replaces StackDump.cpp inline asm)
// ------------------------------------------------------------
#if defined(PLATFORM_LINUX)
  #include <unwind.h>
  #include <dlfcn.h>

  struct PlatformStackFrame {
    void* addr;
    const char* symbol;
    const char* filename;
    int lineno;
  };

  // Capture up to max_frames frames, skipping skip_frames
  size_t Platform_CaptureStackBackTrace(void** frames, size_t max_frames, size_t skip_frames = 0);

  // Symbolize captured frames
  void Platform_SymbolizeStack(void** frames, size_t count, PlatformStackFrame* out);
#endif

// ------------------------------------------------------------
// Memory allocation hooks (for profiler)
// ------------------------------------------------------------
void* Platform_AllocMemory(size_t bytes);
void* Platform_ReAllocMemory(void* ptr, size_t new_size);
void Platform_FreeMemory(void* ptr);