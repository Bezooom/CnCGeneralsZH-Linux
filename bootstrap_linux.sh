#!/usr/bin/env bash
# Sprint 0 — Linux Port Bootstrap
# Run on Ubuntu 24.04:  chmod +x bootstrap_linux.sh && ./bootstrap_linux.sh

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${REPO_DIR}/build-linux"
JOBS="$(nproc)"

echo "=== C&C Generals ZH Reborn — Linux Port Bootstrap ==="
echo "Repo: ${REPO_DIR}"
echo "Build: ${BUILD_DIR}"
echo "Jobs: ${JOBS}"

# 1. System dependencies
echo -e "\n[1/6] Installing system packages..."
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build ccache mold \
  gcc-13 g++-13 clang-17 lld-17 \
  libvulkan-dev vulkan-tools \
  libsdl3-dev \
  libopenal-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
  zlib1g-dev \
  libunwind-dev \
  libfmt-dev \
  git python3 python3-venv \
  renderdoc

# 2. Compiler setup (prefer clang for better diagnostics)
export CC=clang-17
export CXX=clang++-17
export LDFLAGS="-fuse-ld=mold"
export CFLAGS="-O2 -g -fms-extensions -fno-strict-aliasing -Wno-deprecated-declarations"
export CXXFLAGS="${CFLAGS} -std=c++17"

# 3. Configure
echo -e "\n[2/6] Configuring CMake (Linux profile)..."
cmake -S "${REPO_DIR}/GeneralsMD/Code" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER="${CC}" \
  -DCMAKE_CXX_COMPILER="${CXX}" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DPLATFORM_LINUX=ON \
  -DRELEASE_DEBUG_LOGGING=ON \
  -DPERF_TIMERS=OFF

# 4. Build core libraries only (no WinMain, no renderer yet)
echo -e "\n[3/6] Building core libraries (wwmath, compression, wwlib, wwutil, wwsaveload)..."
cmake --build "${BUILD_DIR}" --target wwmath compression wwlib wwutil wwsaveload wwdebug -- -j"${JOBS}"

# 5. Run self-checks
echo -e "\n[4/6] Running self-checks..."
cd "${BUILD_DIR}"
./wwmath_selfcheck
./compression_selfcheck
ctest --test-dir . -R "test_wwmath|test_compression|test_wwlib|test_wwutil|test_wwsaveload" --output-on-failure -j"${JOBS}"

# 6. Generate dependency audit
echo -e "\n[5/6] Generating DEPENDENCY_AUDIT.md..."
cat > "${REPO_DIR}/DEPENDENCY_AUDIT.md" << 'AUDIT_EOF'
# Dependency Audit — Generated $(date)

## Windows.h / Win32 API Usage
AUDIT_EOF
grep -r "#include <windows.h>" "${REPO_DIR}/GeneralsMD/Code" --include="*.cpp" --include="*.h" | cut -d: -f1 | sort -u >> "${REPO_DIR}/DEPENDENCY_AUDIT.md"

cat >> "${REPO_DIR}/DEPENDENCY_AUDIT.md" << 'AUDIT_EOF'

## DirectX / D3D Headers
AUDIT_EOF
grep -r "#include <d3d\|#include <dinput\|#include <dsound" "${REPO_DIR}/GeneralsMD/Code" --include="*.cpp" --include="*.h" | cut -d: -f1 | sort -u >> "${REPO_DIR}/DEPENDENCY_AUDIT.md"

cat >> "${REPO_DIR}/DEPENDENCY_AUDIT.md" << 'AUDIT_EOF'

## Inline Assembly (__asm / _asm)
AUDIT_EOF
grep -r "__asm\|_asm" "${REPO_DIR}/GeneralsMD/Code" --include="*.cpp" --include="*.h" | grep -v "test_" | cut -d: -f1 | sort -u >> "${REPO_DIR}/DEPENDENCY_AUDIT.md"

cat >> "${REPO_DIR}/DEPENDENCY_AUDIT.md" << 'AUDIT_EOF'

## GameSpy / Network
AUDIT_EOF
grep -r "GameSpy\|gamespy" "${REPO_DIR}/GeneralsMD/Code" --include="*.cpp" --include="*.h" -l | sort -u >> "${REPO_DIR}/DEPENDENCY_AUDIT.md"

echo -e "\n[6/6] Done. Next steps:"
echo "  1. Review ${REPO_DIR}/DEPENDENCY_AUDIT.md"
echo "  2. Start replacing inline asm in PerfTimer.h, crc.h, wwmath.h"
echo "  3. Implement Platform/ abstraction layer"
echo "  4. Build ww3d2 with Vulkan stub (see PORT_TO_LINUX_PLAN.md Phase 2)"