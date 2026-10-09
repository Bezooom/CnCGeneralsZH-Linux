/*
**  Platform Abstraction Layer — Implementation
*/

#include "PAL_Timing.h"
#include <atomic>
#include <cstring>
#include <algorithm>

#if defined(PLATFORM_LINUX)
  #include <time.h>
  #include <pthread.h>
  #include <unwind.h>
  #include <dlfcn.h>
  #include <cxxabi.h>
  #include <stdlib.h>
  #include <stdio.h>
  #include <link.h>
#elif defined(PLATFORM_WINDOWS)
  #include <windows.h>
  #include <intrin.h>
  #include <dbghelp.h>
#endif

// ------------------------------------------------------------
// Timer calibration
// ------------------------------------------------------------
PlatformTimerCalibration g_timer_calibration;

void Platform_InitPrecisionTimer() {
#if defined(PLATFORM_WINDOWS)
  // Windows: use QueryPerformanceFrequency
  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  g_timer_calibration.ticks_per_sec = freq.QuadPart;
  g_timer_calibration.ticks_per_ms = static_cast<double>(freq.QuadPart) / 1000.0;
  g_timer_calibration.ticks_per_us = static_cast<double>(freq.QuadPart) / 1000000.0;
#else
  // Linux: calibrate RDTSC against CLOCK_MONOTONIC_RAW
  // Run 3 measurements of 20ms each, take median (matches original logic)
  uint64_t measurements[3];

  for (int k = 0; k < 3; ++k) {
    // Wait for tick boundary
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts_start);
    uint64_t start_ns = ts_start.tv_sec * 1000000000ULL + ts_start.tv_nsec;
    uint64_t start_tsc = PLATFORM_RDTSC();

    // Busy-wait ~20ms
    uint64_t target_ns = start_ns + 20000000ULL;
    uint64_t now_ns;
    do {
      clock_gettime(CLOCK_MONOTONIC_RAW, &ts_end);
      now_ns = ts_end.tv_sec * 1000000000ULL + ts_end.tv_nsec;
    } while (now_ns < target_ns);

    uint64_t end_tsc = PLATFORM_RDTSC();
    measurements[k] = end_tsc - start_tsc;

    // Convert to per-second rate using actual elapsed time
    uint64_t elapsed_ns = now_ns - start_ns;
    if (elapsed_ns > 0) {
      measurements[k] = (measurements[k] * 1000000000ULL) / elapsed_ns;
    }
  }

  // Find two closest values (median of three)
  uint64_t d01 = measurements[0] > measurements[1] ? measurements[0] - measurements[1] : measurements[1] - measurements[0];
  uint64_t d02 = measurements[0] > measurements[2] ? measurements[0] - measurements[2] : measurements[2] - measurements[0];
  uint64_t d12 = measurements[1] > measurements[2] ? measurements[1] - measurements[2] : measurements[2] - measurements[1];

  uint64_t avg;
  if (d01 < d02) {
    avg = (d01 < d12) ? (measurements[0] + measurements[1]) : (measurements[1] + measurements[2]);
  } else {
    avg = (d02 < d12) ? (measurements[0] + measurements[2]) : (measurements[1] + measurements[2]);
  }

  g_timer_calibration.ticks_per_sec = avg / 2;
  g_timer_calibration.ticks_per_ms = g_timer_calibration.ticks_per_sec / 1000.0;
  g_timer_calibration.ticks_per_us = g_timer_calibration.ticks_per_sec / 1000000.0;
#endif
}

// ------------------------------------------------------------
// CRC32 (slicing-by-8, matches zlib's algorithm)
// ------------------------------------------------------------
static uint32_t crc32_table[8][256];
static bool crc32_table_init = false;

static void crc32_init_tables() {
  if (crc32_table_init) return;
  // Standard CRC32 polynomial (reversed): 0xEDB88320
  const uint32_t poly = 0xEDB88320u;
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t crc = i;
    for (int j = 0; j < 8; ++j) {
      crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
    }
    crc32_table[0][i] = crc;
  }
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t crc = crc32_table[0][i];
    for (int k = 1; k < 8; ++k) {
      crc = crc32_table[0][crc & 0xFF] ^ (crc >> 8);
      crc32_table[k][i] = crc;
    }
  }
  crc32_table_init = true;
}

uint32_t Platform_CRC32(const void* data, size_t len, uint32_t crc) {
  crc32_init_tables();
  const uint8_t* p = static_cast<const uint8_t*>(data);
  crc = ~crc;

  // Align to 8 bytes
  while (len > 0 && (reinterpret_cast<uintptr_t>(p) & 7)) {
    crc = crc32_table[0][(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    --len;
  }

  // Process 8 bytes at a time.  The trailing term carries the running CRC
  // into the next block: on x86 MSVC this was written as (crc >> 32), which
  // the 32-bit shr executes as a nop (count masked to zero), preserving the
  // whole word.  A 64-bit shift compiles to a real shift - always zero -
  // which drops the state and makes long inputs collide.  Spelled out so
  // every compiler computes the Windows values bit for bit.
  while (len >= 8) {
    uint64_t chunk;
    std::memcpy(&chunk, p, 8);
    crc = crc32_table[7][ chunk        & 0xFF] ^
          crc32_table[6][(chunk >>  8) & 0xFF] ^
          crc32_table[5][(chunk >> 16) & 0xFF] ^
          crc32_table[4][(chunk >> 24) & 0xFF] ^
          crc32_table[3][(chunk >> 32) & 0xFF] ^
          crc32_table[2][(chunk >> 40) & 0xFF] ^
          crc32_table[1][(chunk >> 48) & 0xFF] ^
          crc32_table[0][(chunk >> 56) & 0xFF] ^
          crc;
    p += 8;
    len -= 8;
  }

  // Remaining bytes
  while (len--) {
    crc = crc32_table[0][(crc ^ *p++) & 0xFF] ^ (crc >> 8);
  }

  return ~crc;
}

uint32_t Platform_CRC32_Combine(uint32_t crc1, uint32_t crc2, size_t len2) {
  // Not needed for current use, but placeholder for completeness
  (void)len2;
  return crc1 ^ crc2;
}

// ------------------------------------------------------------
// Memory allocation
// ------------------------------------------------------------
void* Platform_AllocMemory(size_t bytes) {
  return malloc(bytes);
}

void* Platform_ReAllocMemory(void* ptr, size_t new_size) {
  return realloc(ptr, new_size);
}

void Platform_FreeMemory(void* ptr) {
  free(ptr);
}

// ------------------------------------------------------------
// Stack walking (Linux)
// ------------------------------------------------------------
#if defined(PLATFORM_LINUX)

struct UnwindContext {
  void** frames;
  size_t max_frames;
  size_t skip_frames;
  size_t count;
};

_Unwind_Reason_Code unwind_callback(_Unwind_Context* ctx, void* arg) {
  UnwindContext* uc = static_cast<UnwindContext*>(arg);
  if (uc->skip_frames > 0) {
    --uc->skip_frames;
    return _URC_NO_REASON;
  }
  if (uc->count < uc->max_frames) {
    uc->frames[uc->count++] = reinterpret_cast<void*>(_Unwind_GetIP(ctx));
    return _URC_NO_REASON;
  }
  return _URC_END_OF_STACK;
}

size_t Platform_CaptureStackBackTrace(void** frames, size_t max_frames, size_t skip_frames) {
  UnwindContext uc{frames, max_frames, skip_frames + 1, 0}; // +1 to skip this function
  _Unwind_Backtrace(unwind_callback, &uc);
  return uc.count;
}

void Platform_SymbolizeStack(void** frames, size_t count, PlatformStackFrame* out) {
  for (size_t i = 0; i < count; ++i) {
    Dl_info info;
    if (dladdr(frames[i], &info) && info.dli_sname) {
      int status = 0;
      char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
      out[i].symbol = (status == 0 && demangled) ? demangled : info.dli_sname;
      out[i].filename = info.dli_fname;
      out[i].lineno = 0; // Would need libdw for line numbers
    } else {
      out[i].symbol = "<unknown>";
      out[i].filename = nullptr;
      out[i].lineno = 0;
    }
    out[i].addr = frames[i];
  }
}

// ------------------------------------------------------------
// D3DX9 runtime math fallback implementations (Linux)
// ------------------------------------------------------------
#include "LinuxDX/d3dx9math.h"

static D3DXMATRIX * D3DXMatrixMultiply_Fallback(D3DXMATRIX *out, const D3DXMATRIX *m1, const D3DXMATRIX *m2)
{
    if (!out || !m1 || !m2) return nullptr;
    D3DXMATRIX temp;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            temp.m[i][j] = m1->m[i][0] * m2->m[0][j] +
                           m1->m[i][1] * m2->m[1][j] +
                           m1->m[i][2] * m2->m[2][j] +
                           m1->m[i][3] * m2->m[3][j];
        }
    }
    *out = temp;
    return out;
}

static D3DXMATRIX * D3DXMatrixTranspose_Fallback(D3DXMATRIX *out, const D3DXMATRIX *m)
{
    if (!out || !m) return nullptr;
    D3DXMATRIX temp;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            temp.m[i][j] = m->m[j][i];
        }
    }
    *out = temp;
    return out;
}

static D3DXMATRIX * D3DXMatrixScaling_Fallback(D3DXMATRIX *out, float sx, float sy, float sz)
{
    if (!out) return nullptr;
    D3DXMatrixIdentity(out);
    out->_11 = sx;
    out->_22 = sy;
    out->_33 = sz;
    return out;
}

static D3DXMATRIX * D3DXMatrixTranslation_Fallback(D3DXMATRIX *out, float x, float y, float z)
{
    if (!out) return nullptr;
    D3DXMatrixIdentity(out);
    out->_41 = x;
    out->_42 = y;
    out->_43 = z;
    return out;
}

static D3DXMATRIX * D3DXMatrixRotationZ_Fallback(D3DXMATRIX *out, float angle)
{
    if (!out) return nullptr;
    D3DXMatrixIdentity(out);
    float cosA = std::cos(angle);
    float sinA = std::sin(angle);
    out->_11 = cosA;
    out->_12 = sinA;
    out->_21 = -sinA;
    out->_22 = cosA;
    return out;
}

static long D3DXMatrixInverse_Fallback(D3DXMATRIX *pOut, float *pDeterminant, const D3DXMATRIX *pM)
{
    if (!pOut || !pM) return 0;
    float a0 = pM->_11 * pM->_22 - pM->_12 * pM->_21;
    float a1 = pM->_11 * pM->_23 - pM->_13 * pM->_21;
    float a2 = pM->_11 * pM->_24 - pM->_14 * pM->_21;
    float a3 = pM->_12 * pM->_23 - pM->_13 * pM->_22;
    float a4 = pM->_12 * pM->_24 - pM->_14 * pM->_22;
    float a5 = pM->_13 * pM->_24 - pM->_14 * pM->_23;
    float b0 = pM->_31 * pM->_42 - pM->_32 * pM->_41;
    float b1 = pM->_31 * pM->_43 - pM->_33 * pM->_41;
    float b2 = pM->_31 * pM->_44 - pM->_34 * pM->_41;
    float b3 = pM->_32 * pM->_43 - pM->_33 * pM->_42;
    float b4 = pM->_32 * pM->_44 - pM->_34 * pM->_42;
    float b5 = pM->_33 * pM->_44 - pM->_34 * pM->_43;

    float det = a0 * b5 - a1 * b4 + a2 * b3 + a3 * b2 - a4 * b1 + a5 * b0;
    if (pDeterminant) *pDeterminant = det;
    if (std::abs(det) < 1e-12f) return 0;

    float invDet = 1.0f / det;
    D3DXMATRIX temp;
    temp._11 = ( pM->_22 * b5 - pM->_23 * b4 + pM->_24 * b3) * invDet;
    temp._12 = (-pM->_12 * b5 + pM->_13 * b4 - pM->_14 * b3) * invDet;
    temp._13 = ( pM->_42 * a5 - pM->_43 * a4 + pM->_44 * a3) * invDet;
    temp._14 = (-pM->_32 * a5 + pM->_33 * a4 - pM->_34 * a3) * invDet;

    temp._21 = (-pM->_21 * b5 + pM->_23 * b2 - pM->_24 * b1) * invDet;
    temp._22 = ( pM->_11 * b5 - pM->_13 * b2 + pM->_14 * b1) * invDet;
    temp._23 = (-pM->_41 * a5 + pM->_43 * a2 - pM->_44 * a1) * invDet;
    temp._24 = ( pM->_31 * a5 - pM->_33 * a2 + pM->_34 * a1) * invDet;

    temp._31 = ( pM->_21 * b4 - pM->_22 * b2 + pM->_24 * b0) * invDet;
    temp._32 = (-pM->_11 * b4 + pM->_12 * b2 - pM->_14 * b0) * invDet;
    temp._33 = ( pM->_41 * a4 - pM->_42 * a2 + pM->_44 * a0) * invDet;
    temp._34 = (-pM->_31 * a4 + pM->_32 * a2 - pM->_34 * a0) * invDet;

    temp._41 = (-pM->_21 * b3 + pM->_22 * b1 - pM->_23 * b0) * invDet;
    temp._42 = ( pM->_11 * b3 - pM->_12 * b1 + pM->_13 * b0) * invDet;
    temp._43 = (-pM->_41 * a3 + pM->_42 * a1 - pM->_43 * a0) * invDet;
    temp._44 = ( pM->_31 * a3 - pM->_32 * a1 + pM->_33 * a0) * invDet;

    *pOut = temp;
    return 0;
}

static D3DXVECTOR4 * D3DXVec4Transform_Fallback(D3DXVECTOR4 *pOut, const D3DXVECTOR4 *pV, const D3DXMATRIX *pM)
{
    if (!pOut || !pV || !pM) return nullptr;
    D3DXVECTOR4 v(pV->x * pM->_11 + pV->y * pM->_21 + pV->z * pM->_31 + pV->w * pM->_41,
                  pV->x * pM->_12 + pV->y * pM->_22 + pV->z * pM->_32 + pV->w * pM->_42,
                  pV->x * pM->_13 + pV->y * pM->_23 + pV->z * pM->_33 + pV->w * pM->_43,
                  pV->x * pM->_14 + pV->y * pM->_24 + pV->z * pM->_34 + pV->w * pM->_44);
    *pOut = v;
    return pOut;
}

static D3DXVECTOR4 * D3DXVec3Transform_Fallback(D3DXVECTOR4 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM)
{
    if (!pOut || !pV || !pM) return nullptr;
    D3DXVECTOR4 v(pV->x * pM->_11 + pV->y * pM->_21 + pV->z * pM->_31 + pM->_41,
                  pV->x * pM->_12 + pV->y * pM->_22 + pV->z * pM->_32 + pM->_42,
                  pV->x * pM->_13 + pV->y * pM->_23 + pV->z * pM->_33 + pM->_43,
                  pV->x * pM->_14 + pV->y * pM->_24 + pV->z * pM->_34 + pM->_44);
    *pOut = v;
    return pOut;
}

D3DXMatrixInverseFunction   D3DXMatrixInverse   = D3DXMatrixInverse_Fallback;
D3DXMatrixBinaryFunction    D3DXMatrixMultiply  = D3DXMatrixMultiply_Fallback;
D3DXMatrixUnaryFunction     D3DXMatrixTranspose = D3DXMatrixTranspose_Fallback;
D3DXMatrixTripleFunction    D3DXMatrixScaling   = D3DXMatrixScaling_Fallback;
D3DXMatrixTripleFunction    D3DXMatrixTranslation = D3DXMatrixTranslation_Fallback;
D3DXMatrixAngleFunction     D3DXMatrixRotationZ = D3DXMatrixRotationZ_Fallback;
D3DXVec4TransformFunction   D3DXVec4Transform   = D3DXVec4Transform_Fallback;
D3DXVec3TransformFunction   D3DXVec3Transform   = D3DXVec3Transform_Fallback;
#endif // PLATFORM_LINUX