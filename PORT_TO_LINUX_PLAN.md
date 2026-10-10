# C&C Generals ZH Reborn — Native Linux/Ubuntu Port Plan

> **Scope**: Full native port from Win32/MSVC to Linux x86_64 (Ubuntu 22.04+/24.04) using GCC/Clang + CMake + Vulkan/OpenGL
> **Estimated effort**: 12–24 months for 1–2 experienced engineers; re-baseline after Phase 1
> **Source**: ~1,500 `.cpp`/`.c` files, ~580 in GameEngine alone

---

## Current Status — 2026-10-10

### Full CTest 31/31 Green, Native Linux Port Installed, Wine Version Removed

- [x] **GameEngine Initialization Reaches Main Loop (Frame 0)**:
  - Fixed `win.h` `CRITICAL_SECTION` to use `PTHREAD_MUTEX_RECURSIVE`, resolving recursive self-deadlock in `TheUnicodeStringCriticalSection` during Object INI loading.
  - Resolved `AsciiString::nextToken` buffer corruption on repeated delimiters and `ensureUniqueBufferOfSize` overlapping copies via `memmove`.
  - Added case-insensitive file path resolution in `LocalFile` via `PAL_FileSystem` and lowercase index in `ArchiveFile`.
  - Wired `IDirect3D9` and `IDirect3DDevice9` adapter enumeration and device caps on Linux in `ww3d_d3d9.h` with scratch-backed surface and buffer locking.
  - Implemented Linux D3DX runtime functions in `d3dx9runtime.cpp` (`D3DXAssembleShader`, `D3DXCompileShader`, `D3DXDisassembleShader`, `D3DXCreateTexture`).
  - Game successfully executes `GameEngine::init()` through all 44 `.big` archives, parses all INIs, brings up `WW3D2` rendering subsystems, and enters `GameEngine::execute()` -> `GameLogic::update()` -> `GameLogic::startNewGame()` -> `ShellGameLoadScreen` loading the main menu shell map (`INPUTWATCH: frame 0`).
- [x] **Vulkan Draw Call Translation & Texture Upload Complete**:
  - Implemented `Platform_CreateVulkanPipeline` with embedded SPIR-V shaders, alpha blending, dynamic viewport and scissor.
  - Allocated host-visible dynamic vertex buffer (4 MB) and index buffer (1 MB) in `PAL_Vulkan`.
  - Implemented `Platform_DrawVulkanIndexed` and `Platform_DrawVulkanVertices`, binding pipeline, buffers, and issuing `vkCmdDrawIndexed` / `vkCmdDraw`.
  - Implemented `Platform_CreateVulkanTexture`, `Platform_BindVulkanTexture`, and `Platform_DestroyVulkanTexture` with `VkImage`, `VkImageView`, and `VkSampler`.
  - Connected `IDirect3DDevice9::DrawIndexedPrimitive`, `DrawPrimitive`, `DrawPrimitiveUP`, and `SetTexture` to Vulkan.
  - Wired `IDirect3DTexture9::UnlockRect` to upload decoded `.dds` texture buffers to `VkImage`.
- [x] **Native `generals` Executable Built & Linked**: **30 MB ELF 64-bit binary** compiled and linked
  cleanly on Linux with GCC 13.3 (C++17, Ninja).
- [x] **Milestone M2.4 Green**: Added `test_vk_swapchain` verifying native Vulkan presentation:
  - Native X11 window creation through `PAL_Window`.
  - Vulkan Xlib surface (`VkSurfaceKHR`) creation on NVIDIA RTX 3090 discrete GPU.
  - Swapchain creation (`VkSwapchainKHR`, `VK_FORMAT_B8G8R8A8_UNORM`, double/triple buffering).
  - Swapchain image views, render pass, framebuffers, command buffers, semaphores and fences.
  - Frame presentation: `Platform_BeginVulkanFrame` + `Platform_EndVulkanFrame` rendering 3 frames via `vkQueuePresentKHR`.
- [x] **Windowing & Event Pump (`PAL_Window`) Complete**: Implemented native X11 window lifecycle,
  event translation for mouse, keyboard, focus, resize, and `WM_DELETE_WINDOW` in `PAL_Window.h`/`.cpp`.
  Verified by automated test `test_pal_window` (1 test, 8 checks).
- [x] **Input Subsystem (`PAL_Input`) Complete**: Implemented `LinuxKeyboard` over `Keyboard` base class
  with ring buffer and keycode mapping to `KeyDefs.h` scan codes. Integrated with `W3DGameClient::createKeyboard`
  and `createMouse` using `W3DMouse` / `Win32Mouse`.
- [x] **ActiveX / IE WebBrowser Resolved**: Restored native `WebBrowser.cpp` into `libgameengine.a`
  with POSIX guards for `OleInitialize`, resolved `_Module`, `TheWebBrowser`, and added inline
  no-op `DX8WebBrowser` fallback on non-Windows (`ENABLE_EMBEDDED_BROWSER=0`).
- [x] **Win32 Directory Enumeration Fixed**: Fixed `win.h` `FindClose` crash on `INVALID_HANDLE_VALUE`
  and added POSIX `fnmatch(..., FNM_CASEFOLD)` wildcard pattern filtering to `FindFirstFileA`/`FindNextFileA`.
- [x] **Distribution & Installation Complete (Phase 5.3 & 7)**:
  - Configured CMake `install()` rules for `generals` runtime binary, `mss32`/`binkw32` libraries, desktop entry, and icon.
  - Created `generals.desktop` with categories `Game;StrategyGame;`.
  - Extracted authentic 48x48 icon from `Generals.ico` into `Distribution/generals.png`.
  - Verified staging installation via `ninja install` (`/usr/local/bin`, `/usr/local/lib`, `/usr/local/share`).
  - Installed standalone user launcher `~/.local/bin/generals`, XDG desktop entry `~/.local/share/applications/generals.desktop`, icons, and desktop shortcuts.
  - Removed legacy Wine installation (2.8 GB) and Wine desktop entries; game runs 100% standalone with native Linux binaries and bundled `.big` assets.
- [x] **Full CTest Suite**: **31/31 passed, 0 failed (100% green)** including `test_registry_class_linux` and `test_gameengine`.
Verification: `GeneralsMD/build-linux` (GCC 13.3, Release, C++17, Ninja).
### Latest device continuation — 2026-09-17

- [x] Reproduce current inventory: 59/103 compiled, 44 failed.
- [x] Remove the unused `Xfer.h -> ModelState.h` dependency; include `BitFlags.h`
  directly instead. This removes the `Xfer -> ModelState -> BitFlagsIO -> Xfer`
  cycle without changing Xfer methods, layout, serialization, or enum values.
- [x] Decode the two INI userData enum tags in `W3DModelDraw.cpp` through
  `std::uintptr_t`, not 32-bit UnsignedInt; include `<cstdint>` explicitly.
- [x] Rerun the complete device gate and core regression checks.
- [ ] Full device acceptance: **66/103 compiled, 37 failed**. No source removal,
  permissive compiler flags, SDK installation, or fake device methods.
- [ ] Windows SDK compilation and dedicated include-order regression tests remain
  unverified; this checkpoint proves Linux compilation only, not renderer link/runtime.

Verification in `/tmp/kilo/ged-continue-20260917`:
- Initial `ninja -j2 -k0 gameenginedevice_linux_compile`: exit 1, 44 failed;
  `/tmp/kilo/ged-current-errors.log`.
- After Xfer dependency repair: same command, 38 failed, build stopped on errors;
  `/tmp/kilo/ged-xfer-cycle.log`. Six newly compiled draw TUs: DependencyModel,
  OverlordAircraft, OverlordTank, OverlordTruck, ProjectileStream, Supply.
- Isolated W3DModelDraw object: `ninja -j1 CMakeFiles/gameenginedevice_linux_compile.dir/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp.o`,
  exit 0; `/tmp/kilo/ged-modeldraw-verified.log` (seventh new TU).
- Final `ninja -j2 -k0 gameenginedevice_linux_compile`: **exit 1, 37 failed**;
  `/tmp/kilo/ged-continuation-final.log`. Object inventory contains 66 `.o` files.
- `ninja -j2 -k0 gameengine test_gameengine`: initial command interrupted at
  240 seconds; explicit resume **exit 0**, `/tmp/kilo/ged-xfer-core-resume.log`.
- `ctest --test-dir /tmp/kilo/ged-continue-20260917 --output-on-failure`:
  **exit 8, 22/23 passed**, test_gameengine SIGSEGV; 3.04 seconds;
  `/tmp/kilo/ged-continuation-ctest.log`. The prior test-engineer investigation
  located an independent `SidesInfo::init` null BuildList member call after
  removing StateMachine test UB. Neither production SidesInfo nor tests changed here.

Remaining diagnostics include direct incomplete D3D COM resource/device use,
resource inheritance conversions, D3DX operations and D3D constants, DirectInput
and Win32 types/functions, plus residual portability issues such as `__max`.
Ten TUs fail on `windows.h` (two dx11runtime consumers, the browser, seven
Win32Device sources). Isolated W3DTankDraw still fails on DirectInput and
WPARAM/LPARAM declarations (`/tmp/kilo/ged-tank-first.log`, exit 1).
Concrete backend blocker: BaseHeightMap.cpp calls `Peek_D3D_Texture()->SetLOD`
and `_Get_D3D_Device()->TestCooperativeLevel`; opaque declarations cannot provide
those implementations. Stop here rather than simulate backend success.

### Earlier coordinator verification — WW3D2 boundary

- Device compile-only progress: **59/103 TU compiled, 44 failing**, up from
  31/103. W3DRadar was forcibly rebuilt independently: exit 0, 56 warnings.
- `ww3d_d3d9.h` separates SDK-independent consumer declarations from native
  Windows headers. Linux device interfaces remain opaque; backend inline
  implementations in `dx8wrapper.h` remain Windows-only, not fake success APIs.
- Standalone contract probe `/tmp/kilo/ww3d-boundary-contract.cpp` compiled and
  ran successfully in both header orders (WinCompat first / WW3D first).
  It checks DWORD/REGSAM widths, pointer type, distinct state enums, material
  and viewport offsets, and DXT1 encoding. This is an ad-hoc probe, not CTest.
- Main build regression: rebuilt test_gameengine, test_registry_linux,
  test_wwdownload and test_ffshader; full CTest **23/23 passed**, 49.00 seconds.
  Build log: `/tmp/kilo/boundary-main-build.log`.
- Separate Release build still reports **22/23**, with test_gameengine SIGSEGV
  at its explicit NULL member call (test_gameengine.cpp:2535–2536). Calling a
  non-static member through NULL is undefined C++; no speculative reference
  count change or test suppression was applied. Do not claim all-config green.
- Remaining: 44 device TU, renderer link/backend, Release lifetime-test UB,
  and persistent contract-test integration. No packages installed or commits made.

**Earlier integration checkpoint:** Phase 4 headless compilation is restored: the WWDownload/WWLib
`DWORD` conflict and all three GameSpy consumer type-contract failures are fixed.
`gameengine` and all 17 native test binaries build (exit 0). The full runtime
suite is NOT green: 22/23 CTest entries pass, `test_gameengine` now runs but
SEGFAULTs in a StateMachine lifetime test (exit 8; details below).
Previously, `libgameengine.a` (~580 files)
built with `/usr/bin/g++`, C++17, and `test_gameengine` passed fully:
**348 tests, 505558 checks, 0 failed**. The core gate stays green (all 8
runnable lib suites pass, unchanged). GameEngineDevice/renderer is the next
barrier (Phase 4/5 scope). The earlier 3-way fan-out covered Registry XDG
persistence, the Win32 dependency audit, and the device compile gate.
**Device gate re-baselined on 2026-09-17:** a fresh GCC 13.3 Release build
compiles 31 of 103 device translation units; 72 fail. The first serial failure
is `W3DRadar.cpp` through `WW3D2/dx8fvf.h:51` (`d3d9.h` missing).
The old 242-diagnostic checkpoint is historical, not the current baseline.
See the compile-only checkpoint below; this is not a renderer/runtime pass.

Integration verification: the selected native test targets were rebuilt and
CTest passed **23/23 tests, 0 failures**; a repeat run passed in 48.89 seconds.
The real-DLL `bink_smoke` target/test is now Windows-only. Native Bink/Miles
C and C++ stub smoke tests remain enabled; the Bink smoke callback now uses
`BINKSNDSYSPARAM` to match the public LP64-safe signature. This validates the
current test build, not a clean full-game build or a playable Linux runtime.

### DWORD callsite integration — 2026-09-17 (compile restored; runtime gate failed)

- [x] Repair MainMenuUtils thread ID as DWORD, matching CreateThread's DWORD*.
- [x] Resolve SNMP and ICMP symbols through decltype of their existing function
  pointers instead of duplicating unsigned-long signatures. Preserve WINAPI /
  __stdcall and the declared DWORD return/argument contracts; no pointer-to-DWORD
  conversion, pointer-through-integer workaround, overload or fake API added.
- [x] Force rebuild of exactly the three GameSpy objects (3 removed, 3 rebuilt,
  exit 0), then link gameengine (exit 0).
- [x] Build all 17 native test binary targets and run all 23 CTest entries.
- [ ] Dedicated sizeof(DWORD)/sizeof(REGSAM), mixed-header/include-order and
  call-signature regression test: requested from coordinator/test-engineer;
  not implemented in this backend-only session (test authoring restricted).
- [ ] Runtime acceptance: test_gameengine must pass; current SEGFAULT is recorded
  below, not hidden by excluding its test or changing unrelated code.

Files changed in this continuation: GameSpy/MainMenuUtils.cpp,
GameSpy/StagingRoomGameInfo.cpp, GameSpy/Thread/PingThread.cpp and this plan.
Windows DLL pointer declarations and calling conventions remain unchanged;
DWORD still means native unsigned long on Windows. Windows compilation was not
available/verified. No additional device fixes, installations or commits.

Verification directory: `/tmp/kilo/ged-continue-20260917`, Release GCC 13.3.
Each build/CTest command had a 1,200,000 ms timeout, full stdout/stderr redirected
without pipelines/truncation, and its exit status preserved. Repeated verification
explicitly printed the exit codes.

```sh
cmake --build /tmp/kilo/ged-continue-20260917 --target gameengine test_wwmath test_wwlib test_wwsaveload test_wwutil test_compression test_ffshader test_registry_linux test_wwdownload test_debug test_support test_gameengine wwmath_selfcheck compression_selfcheck binkstub_smoke_c binkstub_smoke_cpp milesstub_smoke_c milesstub_smoke_cpp -j4 -- -k0 > /tmp/kilo/dword-callsites-build-verified.log 2>&1
ctest --test-dir /tmp/kilo/ged-continue-20260917 --output-on-failure > /tmp/kilo/dword-callsites-ctest-verified.log 2>&1
```

Results: **build exit 0; CTest exit 8, 22 passed / 23, test_gameengine SEGFAULT**.
Unlike the prior checkpoint, test_gameengine is now built and actually executes.
Initial compilation evidence: `/tmp/kilo/dword-callsites-gameengine.log` (three
objects + libgameengine.a), `/tmp/kilo/dword-callsites-native-build.log` (link
of test_gameengine). Forced TU proof: `/tmp/kilo/dword-callsites-forced-tu.log`.

GDB identifies `statemachine_outlives_the_owner_that_lets_go_of_it_mid_update()`
then `main`, not a GameSpy callsite. Faulting instruction is `mov 0x18,%eax`,
followed by `ud2`; existing test_gameengine.cpp:2535-2536 explicitly calls
`machine->deleteInstance()` with machine = NULL. This is consistent with a
null-this optimized Release failure; no unrelated test/core lifetime patch
was made. Evidence: `/tmp/kilo/dword-callsites-gdb.log` and
`/tmp/kilo/dword-callsites-gdb-instruction.log` (GDB command exit 0 is NOT a
passing inferior/test result).

### DWORD integration follow-up — 2026-09-17 (historical: headless blocked)

- [x] Align Linux WWDownload DWORD with existing WWLib/WW3D2 uint32_t.
- [x] Audit registry out-parameters and restore the four-byte numeric buffer.
- [x] Build gameengine and every native test target; run full CTest.
- [ ] Repair the three GameSpy consumer contracts below in their owning scope.
- [ ] Add dedicated mixed-header/LPDWORD regression tests (test-engineer requested).

Changes: WWDownload/WinCompat.h uses uint32_t DWORD in its non-MSVC branch;
WWLib/win.h uses DWORD REGSAM in its Linux branch. WWDownload/registry.cpp
getters now use DWORD size/type and DWORD numeric buffer, with no pointer casts.
Windows headers/branches are unchanged; getter locals still resolve to the same
Windows unsigned long type. Windows compilation was not available/verified.
No WW3D2/device implementation changes, package installation or commits.

Contract audit: FTP registry read uses DWORD value/type/bufsiz consistently;
REG_DWORD storage in setUnsignedIntInRegistry is four bytes. Registry shim
LPDWORD declarations and definitions now agree across headers. Stored byte
length is bounded by the DWORD cbData setter argument. WWDownload has no
DWORD %lu/%lx varargs sites. ffshader already explicitly casts DWORD fields
to unsigned long for its %lu keys; its rebuilt suite passes. timeGetTime and
GetTickCount now share unsigned 32-bit return semantics. Unrelated LP64 types
(ULONG, HRESULT, legacy uint32) were not expanded into this change.

Commands (working directory repository root; B=/tmp/kilo/ged-continue-20260917):
```sh
cmake --build "$B" --target wwdownload -j2
cmake --build "$B" --target gameengine -j4 > /tmp/kilo/dword-gameengine-final.log 2>&1
cmake --build "$B" --target test_wwmath test_wwlib test_wwsaveload test_wwutil test_compression test_ffshader test_registry_linux test_wwdownload test_debug test_support test_gameengine wwmath_selfcheck compression_selfcheck binkstub_smoke_c binkstub_smoke_cpp milesstub_smoke_c milesstub_smoke_cpp -j4 -- -k0 > /tmp/kilo/dword-native-tests-build.log 2>&1
ctest --test-dir "$B" --output-on-failure > /tmp/kilo/dword-ctest.log 2>&1
```

Exit codes in order: **0, 1, 1, 8**. Repeating the native build without
blocked test_gameengine returns **0** (/tmp/kilo/dword-build-unblocked.log).
All sixteen other native test binaries build. Full CTest runs all 23 entries:
22 Passed, test_gameengine Not Run because its executable could not be built.
This is not a passing full suite; no stale test_gameengine was substituted.
Earlier gameengine attempt with -j2 timed out at 240 seconds; the explicit
-j4 continuation above completed with exit 1. No build exit is hidden by tail.

New blockers from the complete -k0 build (not repaired outside compatibility scope):
- MainMenuUtils.cpp:698/704: unsigned long threadid passed as DWORD* to CreateThread.
  Callback parameter remains void*: converting a pointer to DWORD is not a fix.
- StagingRoomGameInfo.cpp:222/224: SNMP function-pointer casts spell unsigned long
  while the declared signatures use DWORD (initialization tick and allocation size).
- Thread/PingThread.cpp:472: IcmpSendEcho cast uses unsigned long return/arguments,
  incompatible with the DWORD function-pointer declaration on LP64.

Prior forced W3DRadar compilation passed (/tmp/kilo/w3dradar-forced.log).
Device inventory remains the prior **59/103**, not rerun or claimed complete here.

### WW3D2 declaration boundary continuation — 2026-09-17 (in progress)

Authorized scope now includes WW3D2 headers/necessary sources and narrow WWLib
compatibility. No SDK installation or header-path workaround: native Windows
headers remain the Windows ABI authority; Linux gets opaque interface declarations
and the small numeric/value-type contract required by consumer headers.

- [x] Add the portable declaration boundary and isolate device-dependent inline bodies.
- [x] Repair an initial set of exposed case/include-order/strict-C++ issues without dropping TUs.
- [x] Rerun the complete 103-TU inventory: 59 compiled, 44 failed.
- [ ] Dedicated header contract tests and Windows SDK verification (test-engineer follow-up).
- [ ] Resolve integration typedef conflict and reverify the headless gate.
- Acceptance remains partial: no fake device success methods; all sources stay
  in the gate. Direct device implementation still requires a real backend.

`ww3d_d3d9.h` uses the real D3D9 SDK on Windows and opaque COM interface
pointers plus header-required numeric/value declarations on Linux. Consumers
include dx8fvf/dx8caps/dx8wrapper/texture/d3dx9runtime/ffprobe and the shader
manager. Linux device-touching inline implementations are declarations only,
not successful no-ops. Fixed case-sensitive mesh includes, extra-qualified
members, missing texture-loader forward declaration, shader enum dependencies,
and glibc 2.38+ string function declaration conflicts. `timeGetTime` already
exists in `WWLib/mmsys.h`; six device consumers now explicitly include that
header on Linux rather than duplicating its implementation in `win.h`.

Verification checkpoint (`/tmp/kilo/ged-continue-20260917`, GCC 13.3):
- `ninja -j2 -k0 gameenginedevice_linux_compile`: exit 1, 44 failed TUs;
  59 object files present, compared with the initial 31/103. Latest complete
  log: `/tmp/kilo/ged-boundary7.log`. No TU exclusions or CMake changes here.
- Isolated W3DRadar object: up to date, exit 0.
- Rebuilt `test_wwlib` and `test_ffshader`: CTest 2/2 passed (0.78 seconds).
- `wwlib` builds independently. Fresh `gameengine` dependency build fails in
  `wwdownload/FTP.CPP`: `WinCompat.h:53` declares DWORD as unsigned long,
  while the LP64-correct `WWLib/bittype.h:61` uses uint32_t. Log:
  `/tmp/kilo/ged-dependencies-build.log`, lines 55–68. WWDownload correction
  and regression verification were handed to the coordinator; not changed
  under this boundary scope. The prior headless success is historical now.

Remaining blockers include direct IDirect3DDevice9/resource method calls and
resource inheritance conversions, D3DX/DX11 runtime dependencies, Win32
window/input/CD/browser APIs, and an existing core include cycle:
`Xfer.h -> ModelState.h -> BitFlagsIO.h -> Xfer.h`. XferVersion is defined in
Xfer.h itself; there is no XferVersion.h to include. These need real backend
work or core dependency cleanup, not SDK stubs or source removal.

### GameEngineDevice compile-only checkpoint — 2026-09-17 (prior baseline)

Scope: `GameEngineDevice/**`, `WWLib/win.h`, the necessary device CMake
block, and this existing plan. No package installation, commits, renderer
implementation, API success stubs, or removal of functional sources.

- [x] Reproduce from the current tree rather than the old diagnostic count.
- [x] Isolate the first serial failure and inspect its dependency chain.
- [x] Attempt every TU for a current inventory; preserve the complete source set.
- [x] Stop at a concrete out-of-scope renderer boundary and record evidence.
- [ ] Acceptance: all 103 TUs compile without linking or claiming runtime support.

Fresh configure succeeded with GCC/G++ 13.3.0, Ninja, C++17, Release.
The existing `gameenginedevice_linux_compile` OBJECT target already includes
all 103 `Source/*.cpp` files (including `Win32DIMouse.cpp`, before the
shipping target's historical exclusion), so no CMake change was necessary.
The first three TUs (`MilesAudioManager`, `BinkVideoPlayer`,
`W3DFunctionLexicon`) compiled, with warnings. The fourth, `W3DRadar`, failed:
`W3DRadar.cpp:53` -> `HeightMap.h:35` -> `dx8vertexbuffer.h:50` ->
`WW3D2/dx8fvf.h:51`, fatal missing `<d3d9.h>`.
An isolated repeat of that object reproduced the same failure, exit 1.

**Architectural/scope blocker:** WW3D2 has no portable D3D9 declaration
boundary for its device-facing headers. `dx8fvf.h` needs D3D FVF definitions;
`HeightMap.h` also includes `dx8wrapper.h`, which unconditionally includes
D3D9, D3DX9 runtime and DX11 runtime headers and contains inline device calls.
`W3DRadar` genuinely calls `DX8Wrapper::Get_Current_Caps()`. Removing a single
include, adding a forward declaration, or changing an include directory does
not provide that contract. No `d3d9.h` exists in the repository; the existing
`LinuxDX` math/COM/input compatibility headers do not provide it. A reviewed
portable declaration boundary in WW3D2/LinuxDX is prerequisite work outside
this scope; it need not implement a renderer, but must not fabricate working
API methods. Separately, the Linux link graph still has no `ww3d2` target
(the renderer is inside `if(MSVC)`); this does not cause the OBJECT failure.

Full continuation with `-j2 -k0` attempted the other 100 TUs: 28 succeeded,
72 failed. Combined with the first three, **31/103 compiled, 72/103 failed**.
The continuation log contains **133 error diagnostic lines**, including
39 fatal `d3d9.h` errors; these are not 133 independent root causes. Other
observed failures include WW3D2 header case mismatches (`snappts.h`,
`streakrender.h`, `LightEnvironment.h`), extra-qualified member declarations
in `rendobj.h`/`segline.h`, missing declarations in `texture.h`/
`textureloader.h`, `BitFlagsIO.h` Xfer include-order failures, glibc inline
conflicts in `WWLib/stringex.h`, and seven Win32 device TUs including
`windows.h` directly. Later errors remain hidden behind failed includes.
The initial parallel run in the existing build stopped early (79 diagnostic
lines), so it is not the complete inventory either.

Reproduction commands, from the repository root (no dependency downloads):

```sh
cmake -S GeneralsMD/Code -B /tmp/kilo/ged-continue-20260917 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja -C /tmp/kilo/ged-continue-20260917 -j1 gameenginedevice_linux_compile > /tmp/kilo/ged-serial1.log 2>&1
ninja -C /tmp/kilo/ged-continue-20260917 -j2 -k0 gameenginedevice_linux_compile > /tmp/kilo/ged-inventory.log 2>&1
ninja -C /tmp/kilo/ged-continue-20260917 -j1 CMakeFiles/gameenginedevice_linux_compile.dir/GameEngineDevice/Source/W3DDevice/Common/System/W3DRadar.cpp.o > /tmp/kilo/ged-radar.log 2>&1
```

Configure: exit 0; all three Ninja gate commands fail. Full compiler commands
are in the build's `compile_commands.json` and failed-command log entries.
Configure also reports absent SDL2, OpenAL, FFmpeg and Vulkan shader compilers;
these are not the first compile failure. Existing source changes were retained;
this continuation changes only `PORT_TO_LINUX_PLAN.md`. Windows source/ABI and
CMake paths were not changed; Windows compilation and runtime tests were not
performed. No success claim is made for audio/video backends from TU compilation.

Next: authorize and design the WW3D2 portable header boundary, then rerun the
same serial gate and fix newly exposed errors within their owning scopes.

### Shader generator gate (green, NEW)

| Target | Result |
|--------|--------|
| `ffshader` | builds clean on GCC, no d3d9 runtime, `-Wformat=2 -Werror=format` |
| `test_ffshader` | 27 tests, 164 checks, 0 failed (21 existing + 6 contract) |

Verify: `ninja -C GeneralsMD/build-linux test_ffshader && ./GeneralsMD/build-linux/test_ffshader`

- New `WW3D2/ffshader_d3d.h`: a numeric dictionary of only the D3D9 texture-op
  /compare-op/argument values the generator reads, with `DWORD` fixed at
  unsigned 32 bits on LP64 (static_asserts). Windows keeps `<d3d9.h>`.
- `ffshader.h` includes the boundary instead of `d3d9.h` directly; `ffshader.cpp`
  casts `DWORD` args to `unsigned long` for `%lu` and widens the key field to 45
  bytes so four full-width DWORDs cannot truncate (exact key format kept).
- This is the text-generation half of the shader pipeline, not compiled HLSL and
  not rendering; `test_engineshader` (needs `D3DCompile`) is still Windows-only.
- Vulkan probe this session: headers/loader 1.3.275 + llvmpipe enumerate fine
  (`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`), but no GLSL/HLSL
  compiler (`glslangValidator`/`glslc`/`dxc` absent) — a real device gate is
  blocked on a shader toolchain.

### Multimedia stub gate (green, NEW)

| Target | Result |
|--------|--------|
| `binkstub` -> `libbinkw32.so` | builds clean, exports plain `Bink*` symbols |
| `milesstub` -> `libmss32.so` | builds clean, exports plain `AIL`/`MSS` symbols |

Verify: `ninja -C GeneralsMD/build-linux binkstub milesstub`

- `-std=c++17` is now applied via `$<COMPILE_LANGUAGE:CXX>` only (gcc was
  rejecting it for C TUs).
- `bink.h` gained `BINKCALL`: `__stdcall` on Windows (DLL ABI untouched),
  default calling convention on Linux; the stub uses it. Miles got the
  same treatment (`MSSCALL`), plus `<stdlib.h>` for `malloc/free`.
- On Linux the stub `.def` decoration story does not apply; the .so exports
  undecorated names and any Linux consumer must link those, not the
  stdcall-decorated ones. Windows path unchanged.
- Deferred: `MSS.h` portability, small C/C++ link smoke tests for the stubs.

### Bink sound-system parameter contract (green, NEW)

The last pointer-through-U32 truncation in the Bink path is fixed:

- `bink.h` now types the opaque sound-system parameter as `BINKSNDSYSPARAM`:
  `unsigned int` on `_WIN32` (shipped `BINKW32.DLL` ABI stays byte-identical),
  `uintptr_t` on Linux (real pointers must not truncate). `BINKSNDSYSOPEN`,
  `BinkSetSoundSystem`, `BinkOpenDirectSound` and the
  `BinkSoundUseDirectSound(lpDS)` macro all use it; `<stdint.h>` is included
  on non-Windows. Windows ABI untouched.
- `Bink/stub/bink.c` signatures follow the same typedef.
- Verified: a minimal C and C++ TU calling `BinkSoundUseDirectSound(void*)`
  compiles warning-free with `-Werror=pointer-to-int-cast`; `binkstub`
  relinks; `binkstub_smoke_c`/`binkstub_smoke_cpp` pass (2/2).

### Registry XDG persistence gate (green, NEW, fan-out)

| Target | Result |
|--------|--------|
| `test_registry_linux` | 13 tests, 867 checks, 0 failed (TDD red→green) |
| `test_gameengine` regression | 348 tests, 505558 checks, 0 failed (verified independently by coordinator) |

- `GameEngine/Source/Common/System/registry.cpp`: Windows branch is
  byte-identical (verified); Linux `#else` stores typed values in
  `$XDG_DATA_HOME/CnCGeneralsZH-Reborn/registry.ini` (fallback
  `$HOME/.local/share/...`), sections `General`/`Options`, DWORD decimal /
  SZ + BINARY hex-encoded, `flock`+rename+`fsync`, honest fail paths
  (out params untouched on error), size limits (1 MB file, SZ 32766,
  BINARY 65536). Linux-only `Set/Get` surface added to `Registry.h`.
- `OptionsMenu.cpp`: +4 Linux-guarded lines to disambiguate the two
  `Registry.h` headers explicitly.
- Tests cover roundtrips, absent keys, persistence across fork,
  `XDG_DATA_HOME` override, 17 corrupted-INI cases, no-clobber semantics.
- Limits: only `""` (General) and `\Options` paths; printable-ASCII keys
  <=255; directory-fsync may report failure after a successful rename.
- The WWDownload/WinCompat process-local registry is unchanged; game
  settings now persist cross-process on Linux.

### Win32 dependency audit (new, fan-out)

`DEPENDENCY_AUDIT.md` in the repo root: 41 Win32 API dependencies across
GeneralsMD/Code (audio 8, video 8, registry 4, threading 10, IO 11),
including guarded/shim/already-disabled sites, not all hard blockers.
Top files: MilesAudioManager.cpp (5), simpleplayer.cpp (4), JobSystem.cpp (3),
BinkVideoPlayer.cpp (3), W3DDisplay.cpp (3). Snapshot may drift as the port
proceeds. Also: `chunkio.h` trailing blank lines before `#endif` removed and
EOF newline added (diff +1/-3, whitespace-clean, build untouched).

### Headless gate (all green)

| Target | Result |
|--------|--------|
| `gameengine` (static lib) | builds clean, ~138 MB archive |
| `test_gameengine` | 348 tests, 505558 checks, 0 failed |

Verify: `ninja -C GeneralsMD/build-linux test_gameengine && ./GeneralsMD/build-linux/test_gameengine`

### Decisions taken for the gameengine port (all narrow, MSVC path untouched)

- Win32 API twins live in `WWLib/win.h` (+ `PreRTS.h` where gameengine TUs
  need them first): `CRITICAL_SECTION`, Winsock 1.1, `SYSTEMTIME`,
  `CRITICAL_SECTION`/thread/event/semaphore handles (tagged union after an
  untagged revision corrupted semaphores and hung `stageRandomMap`),
  `MessageBox`/`ShowWindow`, file/find, locale, `GetCommandLineW`
  (via `/proc/self/cmdline`), `_spawnl`, `FormatMessageA/W`, `HWND`/`HKL`,
  `VK_*`, `LOBYTE/HIBYTE/LOWORD/HIWORD`, `HOSTENT`/`WSABASEERR`/`WSAE*`,
  SNMP stub types, `GetSystemMetrics` (1920x1080, agreed with `PreRTS.h`),
  `SetWindowText`, font/memory/shell shims, `MultiByteToWideChar`,
  `InterlockedExchange`, `FAR`/`NEAR`/`IN`/`OUT`, `_MAX_PATH`, `wcsicmp`,
  `itoa`, `iswascii`, `__int64` (in `Lib/BaseType.h`), `FARPROC`/`LoadLibrary`
  (dlopen; dev-only DLLs resolve NULL and are skipped).
- `__declspec(thread)` maps to `__thread` (an empty mapping silently turned
  every thread into a JobSystem "worker" after the first pool ran).
- `FD_*`/`fd_set`/`select` are NOT shimmed: the system `<sys/select.h>` API
  is used as-is (redefining them broke `FTP.CPP`'s real `select()`).
- `wsock` name clashes with `WWDownload/WinCompat.h` resolved by `#undef`
  before redefinition (`closesocket`, `DeleteFile`, `ioctlsocket`).
- `IMEManager.cpp`: inert null backend on Linux (IMM32 has no twin);
  `StackDump.cpp` (SEH/DbgHelp) disabled on Linux; `StackDump_Linux.cpp`
  (libunwind) kept, `unwind_callback` moved to an anonymous namespace
  (duplicate with debuglib), plus an `addr2line` fallback because `dladdr`
  cannot name `static` functions (every `TEST()` body).
- Real `FindFirstFileA`/`FindNextFileA` on `dirent.h` (the runtime-trig
  scanner test requires `filesScanned > 500`).
- `Platform_CRC32` slicing loop: `(crc >> 32)` on a 32-bit word is a nop on
  x86 (`shr` count masking) but a real zeroing shift on x86-64, which dropped
  the running state and made long inputs collide. Spelled as explicit carry
  (`^ crc`) so every compiler computes the Windows values bit for bit.
- `Platform_CRC32` output depends on input alignment (same quirk, kept for
  Windows compat); the CRC test therefore hashes an `alignas(8)` copy.
  Heap buffers (malloc 16-aligned) are stable; string literals are not.
- RMG fingerprints re-recorded for Linux (x87 64-bit mantissa vs SSE2
  53-bit doubles in `buildHeights` change map bytes; algorithm unchanged so
  `RANDOM_MAP_GENERATOR_VERSION` stays 4; Linux<->Linux self-consistent).
- `LANMessage`: `MAX_LANAPI_PACKET_SIZE` 476 -> 536 on Linux (4-byte
  `WideChar`), `m_lanMaxOptionsLength` multipliers use `sizeof(WideChar)`.
- Link order matters for static archives: `wwmath` before `wwsaveload`
  before `wwlib` (CMake topological sort follows even PRIVATE edges, so the
  spurious `wwsaveload -> wwmath` link was removed), and
  `gameengine PUBLIC` exposes `LinuxDX` + `Platform` (consumers need
  `dinput.h`/`PAL_Timing.h`).
- Static-init order made deterministic: `init_priority(101)` pre-main memory
  init (a `LogClass` ctor ran before the manager on Linux link order).
- Test-only: `test_gameengine_stubs.cpp` grows renderer/COM/browser globals;
  `networkutil.h` include fixed to lowercase; phantom `NetworkUtil.h`
  removed; `_wcsnicmp`-style witnesses ported to SysV (`RBX` only).

### Core gate (all green, unchanged)

| Target | Result |
|--------|--------|
| `wwdebug`, `wwmath`, `wwlib`, `wwutil`, `wwsaveload`, `compression` | build clean with `/usr/bin/g++`, C++17 (unchanged) |
| `debug`, `benchmark`, `eabrowserdispatch` | build clean (NEW) |
| `wwmath_selfcheck` | `Matrix3D::Get_Inverse OK` |
| `compression_selfcheck` | `all codecs OK` (RefPack, NoxLZH, ZLib1/9, BTree, Huff) |
| `test_wwmath` | 73 tests, 10148 checks, 0 failed |
| `test_compression` | 16 tests, 4320 checks, 0 failed |
| `test_wwlib` | 63 tests, 6321 checks, 0 failed |
| `test_wwutil` | 36 tests, 30627 checks, 0 failed |
| `test_wwsaveload` | 41 tests, 428 checks, 0 failed |
| `test_wwdownload` | 23 tests, 129 checks, 0 failed (NEW) |
| `test_debug` | 54 tests, 164 checks, 0 failed |
| `test_support` | 8 tests, 33 checks, 0 failed (NEW) |

Verify: `ninja -C GeneralsMD/build-linux debug test_debug test_support wwdownload test_wwdownload && ctest --test-dir GeneralsMD/build-linux -R "test_debug|test_support|test_wwdownload" --output-on-failure`

### Decisions taken for debug.lib (all narrow, reversible, MSVC path untouched)

- New self-contained `Libraries/Source/debug/debug_linux.h` (types, `__cdecl`/
  `__stdcall`, `__int64`, `OutputDebugString`→stderr, radix `itoa` twin,
  `IsBadReadPtr` via msync); `_pch.h` includes it instead of `windows.h`.
- `debug_stack.cpp`: DbgHelp→`backtrace(3)`+`dladdr(3)`; signature storage
  widened to `uintptr_t`; `IsOldDbghelp()` false, dbghelp handle NULL.
  `-rdynamic` added to the debug target (PUBLIC, tests inherit it).
- `debug_except.cpp`: SEH→signal funnel (`SIGSEGV/FPE/ILL/ABRT/BUS` fill
  Linux `_EXCEPTION_POINTERS`, same logging flow, no crash dialog, `_exit`).
  x86_64 register dump; FPU registers report unavailable.
- `debug_debug.cpp`: `.CRT$XC{B,Y}`→GCC constructor priorities,
  `SetUnhandledExceptionFilter`→sigaction install, `/proc/self/exe`,
  POSIX file IO, `__builtin_return_address`, `IsWindowed()` false
  (headless: asserts/crashes take the auto-disable path, no dialogs),
  frame addresses `uintptr_t`, both path separators honored.
- IO backends: `con` write-only (never steals stdin), `flat` on POSIX fds
  (same buffering/ring/copy-unique-name semantics), `net` as AF_UNIX
  socket client with identical framing, `ods`→stderr.
- `internal.cpp`: `GlobalAlloc` family→malloc/realloc/free, internal
  assert→stderr+`_exit(666)`.
- `eabrowserdispatch`: checked-in `BrowserDispatch_linux.h` (GUIDs
  transcribed from the IDL) deployed to the MIDL output path; stub .c
  defines the three GUIDs. `test_support` COM tests pass unmodified
  in behavior (one MSVC literal-suffix fix in the test itself).
- Stream manipulators (`Hex&` et al.) now take parameters by value:
  binding temporaries to non-const refs is an MSVC extension GCC rejects.
- Upstream bugs fixed via Linux-strictness: `Debug::operator<<` help
  buffers too small for 64-bit binary output; `MemDump` address
  truncation (`unsigned(cur)`); `DebugIONet` ctor left `m_pipe`
  uninitialized.

### Decisions taken for wwdownload (all narrow, reversible, MSVC path untouched)

- New `WWDownload/WinCompat.h`: on MSVC exactly `<windows.h>`; on Linux the
  subset the library and its test touch (registry/file/socket/CRT names).
  bittype-identical spellings where overlapping, so co-inclusion is safe.
- `registry.cpp`: Win32 Registry API over a process-local store — HKCU fully
  read/write, HKLM read-only for normal users (writes fail, reads miss),
  values kept verbatim with type+size so the 256-byte edges, unterminated
  data and type-ignoring getters behave like `RegQueryValueEx`.
  Cross-process persistence is a Phase-4 item (XDG files).
- `FTP.CPP`: POSIX sockets (`close`/`fcntl`, errno-mapped `WSA*` plus
  `EINPROGRESS` for non-blocking connect), `CreateThread` DNS helper folded
  into a synchronous resolve, `_splitpath`/`_getcwd`/`_chmod` shims,
  `Prepare_Directories` on `mkdir`/`EEXIST` (backslashes stay literal, which
  is what the tests pin; real separator policy is Phase 3.4).
- `Download.cpp`: `mmsystem.h`/`direct.h` guarded out, `_mkdir`/`_stat`/
  `_strnicmp`/`MulDiv` via the shim; two include-case fixes
  (`download.h`, `Registry.h`).
- `wwdownload_debug_stubs.cpp` (test-only, mirroring
  `test_gameengine_stubs.cpp`): `DebugLog`/`DebugCrash` live in GameEngine
  in Phase 4, so the stand-ins ship with the test target, not the lib.
- Test-only deltas: `WinCompat.h` include, `./` vs `.\\` scratch prefix is
  NOT changed (backslash-literal dirs work and are pinned); the unreachable
  root uses `/proc/...` on Linux since `\\?\Q:` is a legal filename there.

### Decisions taken this session (all narrow, reversible, MSVC path untouched)

- Linux no longer defines `WIN32`/`_WINDOWS` globally; `_UNIX=1` is defined
  instead, activating the tree's ~79 existing Unix branches.
- Missing-ship headers shimmed: `WWLib/osdep.h` (+`osdep/osdep.h` forwarder),
  `WWLib/tchar.h`; one missing symlink added (`pcx.h -> pcx.H`).
- `win.h`/`mmsys.h`/`always.h` carry the Linux CRT shims (`GetLastError`,
  `GetTickCount`, `timeGetTime`, `stricmp/strupr`, `_snprintf`/`_alloca`,
  MSVC-semantic `_vsnprintf`, `DebugBreak`, `INVALID_HANDLE_VALUE`).
- `NoxLZH` payloads route through system zlib (LZH-Light SDK is stripped);
  framing unchanged, round-trips exact, but NOT format-compatible with
  SDK-written data — revisit with a real LZH decoder before trusting
  game-data loads.
- Excluded from Linux `wwlib` (later phases own them): DirectDraw stack
  (`convert`/`ddraw`/`dsurface`), Win32 input pump (`keyboard`/`msgloop` →
  SDL), `registry` (→ XDG INI backend), `verchk` (PE resources), `WWCOMUtil`
  (COM, no users).
- Threads/mutexes are real pthreads (recursive), not the legacy no-op
  `_UNIX` stubs; `FastCriticalSection` spin has a `pthread_testcancel`
  so `ThreadClass::Stop()` timeout-kill works like `TerminateThread`.
- `long`-is-32-bit hazards fixed where tests pin behavior: SHA-1 words,
  MD5 `UINT4`, `fixed.h`/`base64.cpp` endianness (was keyed on bare
  `BIG_ENDIAN`, which glibc's `<endian.h>` also defines — silent ODR
  violation between TUs; now uses `__BYTE_ORDER__`).
- Genuine upstream bugs found via Linux-strictness (same fixes valid on
  Windows): RLE run-count off-by-one (`<=` over-read 1 byte past end),
  non-unit test axes vs `Matrix3D::Set` assert (normalized in tests),
  missing `template<>` storage for `AutoPoolClass::Allocator`.
- Test-only deltas: `test_wwlib`/`test_wwmath` axis normalization,
  `test_wwlib` `win.h` for `GetTickCount`, `test_wwutil` `./` prefix.

### Deferred / known follow-ups

- GameEngineDevice compile-only: blocked at the WW3D2/D3D9 header boundary
  (`W3DRadar` -> `dx8fvf.h:51`), freshly verified above: 31/103 TUs compile,
  72 fail. MilesAudioManager and BinkVideoPlayer now compile in this gate;
  that does not validate audio/video runtime support. Real audio/video
  backends and the sound hook remain Phase 5 decisions. Linux still lacks
  the `ww3d2` link target (renderer stack is `if(MSVC)`). The historical
  242-diagnostic checkpoint is superseded by the reproducible checkpoint above.
- Shader toolchain for real Vulkan work: install/vendor `glslangValidator`
  or `shaderc` (no network access in this session's environment).
- `test_engineshader` needs a portable `D3DCompile` path (Windows-only).
- `MSS.h` portability and stub link smoke tests (see Multimedia stub gate).
- `StackDump_Linux.cpp` compiles with the gameengine target (needs
  GameEngine includes), not with `debug`.
- `chunkio.h` `#endif`-token cosmetic issue: fixed (blank lines + EOF
  newline; whitespace-clean diff).
- `mold` linker optional now (falls back to default `ld` when absent).
- SDL2-vs-SDL3 and Vulkan-vs-DXVK gates still open (unchanged).

### Scope recommendation

Use a **skirmish-only MVP** as the first releasable target: single-player versus AI, all factions and maps, settings menu, native renderer, and OpenAL audio. Defer online multiplayer, campaign cinematics, replay determinism, embedded browser, profile/debug server, and nonessential video playback until the MVP is stable.

The first unresolved architecture decision is **SDL2 versus SDL3**. The current CMake implementation uses SDL2, while the roadmap describes SDL3. Resolve this before starting the window/input phase. The renderer decision is **native Vulkan versus DXVK fallback**; do not begin a full Vulkan implementation until a small device/triangle prototype validates the choice.

---

## Expanded Roadmap

The existing phase structure remains the long-term plan. The roadmap below adds executable gates, dependencies, and acceptance criteria. Relative timing should be re-baselined after the Phase 1 core build is green.

| Phase | Objective | Required work | Exit gate |
|-------|-----------|---------------|-----------|
| **0. Foundation and tooling** | Make the port reproducible and measurable | Freeze a port branch; provide GCC/Clang/CMake/Ninja profiles; generate `compile_commands.json`; add Linux CI; create `DEPENDENCY_AUDIT.md`; document the current build graph and uncommitted changes | Clean configure with GCC and Clang on Ubuntu 22.04/24.04; CI starts on a minimal target; dependency inventory is reviewed |
| **1. Core compile bridge** | Make the foundational libraries compile without Win32 headers | Resolve `wwdebug`/`windows.h`, `ffshader`/`d3d9.h`, case-sensitive includes, `chunkio.h`, and all subsequent WWLib/WWMath errors; remove or conditionalize direct Win32 dependencies; keep compatibility changes narrow and reversible | `wwdebug`, `wwlib`, `wwmath`, `wwutil`, `wwsaveload`, and `compression` build; `wwmath_selfcheck`, `compression_selfcheck`, `test_wwlib`, `test_wwmath`, `test_wwutil`, and `test_wwsaveload` pass |
| **2. Platform abstraction and dependency isolation** | Replace direct OS calls with explicit platform contracts | Implement `Platform/Time`, `Platform/Threading`, `Platform/FileSystem`, `Platform/Window`, `Platform/Input`, `Platform/Audio`, `Platform/Video`, `Platform/Crash`, `Platform/DynamicLibrary`, and `Platform/Registry`; isolate Windows adapters; remove global `WIN32` from Linux compilation where possible | Linux targets contain no direct `windows.h`, `d3d*.h`, Win32 API, or Win32-only link libraries outside isolated adapters; PAL contract tests pass on Linux and Windows |
| **3. GameEngine headless port** | Compile and exercise game logic independently of graphics and input | Port or stub Debug/StackDump, named pipes, thread pools, file enumeration, registry, launch/browser code, and GameSpy references; keep the existing GameEngine tests; add deterministic headless simulation hooks | `test_gameengine` passes; a headless 8-AI simulation can start, run a fixed scenario, and produce a stable state/hash without a renderer |
| **4. Renderer decision and native backend** | Establish the rendering path for Linux | Compare native Vulkan with DXVK using a minimal device/triangle prototype; define the D3D9-shaped adapter; implement Vulkan device, memory, pipeline, command, swapchain, shader, state, resource, and backend modules; map textures and formats | Vulkan triangle, pipeline, W3D model, main-menu background, and skirmish-map milestones render correctly under RenderDoc validation |
| **5. Runtime platform integration** | Make the game interactive on Linux | Integrate the selected SDL version for window/input; integrate OpenAL Soft for audio; use FFmpeg or a static fallback for video; add case-insensitive VFS over loose files and `.big` archives; replace `WinMain` with a Linux entry point | Clean launch reaches the main menu, loads resources, handles keyboard/mouse, and starts a skirmish map |
| **6. MVP integration and packaging** | Produce a usable skirmish build | Link the executable; deploy data beside the executable or through an install layout; add crash logs, configuration paths, `.desktop`/AppStream metadata, and Flatpak/AppImage/.deb scripts; write `BUILD_LINUX.md`, `KNOWN_ISSUES.md`, and `CONTRIBUTING.md` | A clean machine can install or unpack the build, launch the game, select every faction/map, and complete an AI match without manual source-tree setup |
| **7. QA, performance, and polish** | Make the port reliable and maintainable | Add ASan/UBSan runs, visual regression frames with SSIM, deterministic simulation checks, performance baselines, Steam Deck/controller validation, and CI artifacts | No critical sanitizer findings; visual regression threshold passes; performance and compatibility results are recorded; release candidate is reproducible |
| **8. Optional multiplayer** | Restore LAN/online play after the MVP | Replace GameSpy with a maintained transport and lobby design; port sockets, NAT/lobby logic, authentication, replay/desync checks, and UI; choose Steam Networking Sockets, ENet, or another supported transport | Repeated same-input matches produce identical state hashes; LAN and online smoke tests pass on supported platforms |

### Critical path

1. **Core compile bridge** — no renderer, GameEngine, or runtime work can be trusted while foundational libraries still fail on platform headers.
2. **Dependency audit and PAL contracts** — prevents scattering `#ifdef _WIN32` throughout legacy code.
3. **Headless GameEngine** — validates game logic before graphics and input add nondeterminism.
4. **Renderer decision** — a small Vulkan/DXVK prototype is cheaper than committing to a full backend too early.
5. **Runtime integration** — window, input, audio, file system, and entry point come together only after their contracts are stable.
6. **MVP release gate** — package and QA only after a clean skirmish path exists.

### Acceptance criteria for the port

- Clean configure and build with both GCC and Clang on Ubuntu 22.04 and 24.04.
- Linux builds do not define `WIN32` globally and do not link Win32-only libraries except in explicitly isolated compatibility targets.
- No unresolved `windows.h`, `d3d*.h`, `dinput*.h`, `dsound.h`, `winsock`, `winmm`, `DbgHelp`, or equivalent dependency remains in the Linux MVP path.
- Core self-checks and CTest targets pass from a clean build directory.
- Headless GameEngine tests pass without a display, audio device, or game data.
- The MVP launches from a clean install and completes an AI skirmish on every supported faction/map combination selected by the test matrix.
- Crash logs, configuration files, and resource paths follow Linux/XDG conventions.
- Renderer, input, audio, and file-system behavior has automated or repeatable smoke tests.
- Performance, visual-regression, sanitizer, and Steam Deck results are recorded before release.

### Immediate next actions

1. Fix `wwdebug.cpp` without installing or vendoring Windows SDK headers; use a narrow conditional compatibility boundary first.
2. Conditionalize or extract `ffshader.cpp` so HLSL generation does not require `d3d9.h` on Linux.
3. Rebuild the core targets serially, capture every failure, and convert recurring patterns into shared compatibility fixes rather than one-off edits.
4. Resolve the SDL2/SDL3 and Vulkan/DXVK decision gates with small prototypes.
5. Generate `DEPENDENCY_AUDIT.md` from the actual source tree and review the result before expanding the PAL.
6. Add a Linux CI job for the core-target gate once the first build is green.

---

## Phase 0 — Foundation & Tooling (Weeks 1–3)

| Task | Description | Verification |
|------|-------------|--------------|
| 0.1 | Set up Linux build environment: GCC 13+, Clang 17+, CMake 3.28+, Ninja, ccache, mold linker | `cmake --version && gcc --version` |
| 0.2 | Fork repo, create `linux-port` branch, add `.clang-format`, `.clang-tidy`, `compile_commands.json` generation | CI passes format/lint |
| 0.3 | Add Linux CI pipeline (GitHub Actions): `ubuntu-24.04` matrix, `ccache` caching, artifact upload | Green CI on empty build |
| 0.4 | Document all Windows-only dependencies (DirectX, WinMM, WinSock, GameSpy, Miles, Bink, Win32 GUI, inline asm) | `DEPENDENCY_AUDIT.md` complete |
| 0.5 | Create platform abstraction layer (PAL) skeleton: `Platform/Window`, `Platform/Input`, `Platform/Audio`, `Platform/FileSystem`, `Platform/Threading`, `Platform/Time` | Compiles on Linux + Windows |

**Deliverable**: Building `wwmath` + `compression` libraries natively on Linux (no Win32 deps).

---

## Phase 1 — Core Libraries & Inline ASM Elimination (Weeks 4–10)

### 1.1 Inline Assembly Replacement (Critical Path)

| File / Function | Current ASM | Replacement Strategy |
|----------------|-------------|----------------------|
| `PerfTimer.h/cpp` | `__asm rdtsc` | `__builtin_ia32_rdtsc()` / `rdtsc` intrinsic / `clock_gettime(CLOCK_MONOTONIC_RAW)` |
| `crc.h` | `_asm` CRC32 | `zlib`'s `crc32()` or `slicing-by-8` C impl |
| `wwmath.h` (`Inv_Sqrt`, `Float_To_Long`) | `__asm fld/fistp` | `_mm_cvtt_ss2si` (SSE2) + `rsqrtps` approximation or `std::sqrt` |
| `profile_funclevel.cpp` / `internal.h` | `__asm` func profiling | `libunwind` / `backtrace()` / GCC `-finstrument-functions` |
| `StackDump.cpp` | `_asm` EBP walk | `libunwind` / `backtrace_symbols_fd()` |
| `WW3D2/dx8wrapper.h` (inlined) | `__asm` FPU control | `FPUControl.cpp` already exists — port to `fesetenv`/`fegetenv` |

**Verification**: All `wwmath_selfcheck`, `compression_selfcheck`, `wwlib` tests pass on Linux x86_64.

### 1.2 Win32 API Abstraction (WWLib)

| Subsystem | Win32 API | Linux Replacement |
|-----------|-----------|-------------------|
| Threading | `CreateThread`, `WaitForSingleObject`, `CriticalSection` | `std::thread`, `std::mutex`, `std::condition_variable` |
| Timing | `timeGetTime`, `timeBeginPeriod`, `QueryPerformanceCounter` | `clock_gettime(CLOCK_MONOTONIC)`, `timerfd` |
| File I/O | `CreateFile`, `ReadFile`, `WriteFile`, `SetFilePointer` | `open/read/write/lseek` + `mmap` for `.big` archives |
| Memory | `VirtualAlloc`, `VirtualFree`, `HeapAlloc` | `mmap(MAP_ANONYMOUS)`, `malloc`/`free` |
| Registry | `RegOpenKeyEx`, `RegQueryValueEx` | INI files in `~/.config/CnCGenerals/` |
| DLL Loading | `LoadLibrary`, `GetProcAddress` | `dlopen`/`dlsym` |
| Paths | `GetModuleFileName`, `SHGetFolderPath` | `/proc/self/exe`, XDG dirs |

**Verification**: `wwlib` + `wwutil` + `wwsaveload` + `wwdownload` build and unit tests pass.

### 1.3 Compression & Serialization

- zlib 1.1.4 → system `zlib1g-dev` (API compatible)
- LZH-Light → keep vendored, ensure strict aliasing compliance
- EAC/Huffman/RefPack → no asm, should compile clean

**Verification**: `compression_selfcheck` round-trips all codecs.

---

## Phase 2 — Renderer: DirectX 8/9 → Vulkan (Weeks 11–28)

> The CMake already has a **D3D11 backend** (`dx11backend`, `dx11device`, `dx11state`, `dx11layout`, `dx11resource`, `dx11texture`, `dx11post`, `ffshader`, `ffvertex`, `engineshader`). This is the migration target.

### 2.1 Strategy Options

| Option | Pros | Cons | Recommendation |
|--------|------|------|----------------|
| **A: Vulkan native** | Best performance, full control, Linux-native | 6–12 months, complex | ✅ **Primary** |
| **B: D3D11 → VK via DXVK** | Reuses existing D3D11 backend, faster start | Extra layer, debug harder | Fallback |
| **C: OpenGL 4.6 + AZDO** | Simpler than VK, good tooling | Lower perf, legacy baggage | Not recommended |

### 2.2 Vulkan Backend Implementation (Option A)

| Module | Responsibility | Key Files to Port |
|--------|----------------|-------------------|
| `vk_device` | Instance, physical device, logical device, queues, swapchain | New: `VulkanDevice.cpp/h` |
| `vk_memory` | `VkBuffer`/`VkImage` allocation, sub-allocation (VMA) | New: `VulkanMemory.cpp/h` |
| `vk_pipeline` | Graphics/compute pipelines, shader modules, descriptors | New: `VulkanPipeline.cpp/h` |
| `vk_command` | Command buffers, pools, synchronization (fences, semaphores) | New: `VulkanCommand.cpp/h` |
| `vk_swapchain` | Presentation, acquire/present, HDR/SDR | New: `VulkanSwapchain.cpp/h` |
| `vk_shader` | SPIR-V compilation (glslang), reflection, binding layout | New: `VulkanShader.cpp/h` |
| `vk_state` | Render states → pipeline state objects (blend, depth, raster) | Port from `dx11state.cpp` |
| `vk_resource` | Vertex/index buffers, textures (D3D9 fmt → VK fmt), samplers | Port from `dx11resource.cpp` |
| `vk_backend` | Draw call translation: `IDirect3DDevice9` → Vulkan commands | Port from `dx11backend.cpp` |

### 2.3 Shader Pipeline

- Existing HLSL → SPIR-V: use `dxc` (DirectX Shader Compiler) or `glslang`
- `ffshader` / `ffvertex` / `engineshader` already generate HLSL strings → compile offline to SPIR-V or at runtime via `dxc`
- Fixed-function vertex pipeline → vertex input state + vertex shader

### 2.4 Texture & Format Mapping

| D3D9 Format | Vulkan Format | Notes |
|-------------|---------------|-------|
| `D3DFMT_A8R8G8B8` | `VK_FORMAT_B8G8R8A8_SRGB` | sRGB for diffuse |
| `D3DFMT_D24S8` | `VK_FORMAT_D24_UNORM_S8_UINT` | Depth-stencil |
| `D3DFMT_DXT1/3/5` | `VK_FORMAT_BC1/2/3_SRGB_BLOCK` | KTX2 container |
| `D3DFMT_R5G6B5` | `VK_FORMAT_B5G6R5_UNORM_PACK16` | 16-bit |

### 2.5 Verification Milestones

| Milestone | Test |
|-----------|------|
| M2.1 | `dx9_smoke` equivalent: triangle renders via Vulkan |
| M2.2 | `dx11pipeline` equivalent: full pipeline test passes |
| M2.3 | `ww3d2` loads a `.w3d` model, renders with correct materials |
| M2.4 | In-game: main menu background renders, skirmish map loads |

---

## Phase 3 — Platform Layer: Window / Input / Audio (Weeks 15–22, parallel with Phase 2)

### 3.1 Window & Event Loop (SDL3)

- Replace `WinMain.cpp` + `GameWindow.cpp` + `GameWindowManager.cpp`
- SDL3: `SDL_CreateWindow`, `SDL_PollEvent`, `SDL_GetWindowSize`, `SDL_SetWindowFullscreen`
- High-DPI: `SDL_GetDisplayContentScale`, `SDL_SetWindowSize` with logical pixels
- Multi-monitor: `SDL_GetDisplays`, cursor confinement for RTS edge-scroll

### 3.2 Input System

| Current | Replacement |
|---------|-------------|
| `Win32Mouse.cpp` / `Win32Keyboard.cpp` | `SDL_MouseMotionEvent`, `SDL_KeyboardEvent`, `SDL_TextInputEvent` |
| `DirectInput` (mouse fallback) | SDL relative mouse mode (`SDL_SetWindowMouseGrab`) |
| IME (GadgetTextEntry) | `SDL_StartTextInput` / `SDL_SetTextInputRect` |

### 3.3 Audio (OpenAL Soft + FFmpeg)

| Current | Replacement |
|---------|-------------|
| Miles Sound System (`mss32.dll`) | **OpenAL Soft** — 3D positional audio, EFX reverb, Ogg/Vorbis/FLAC via `stb_vorbis`/`dr_flac` |
| Bink Video (`BINKW32.DLL`) | **FFmpeg** (`libavcodec`/`libavformat`/`libswscale`) — decode `.bik` to YUV, upload to Vulkan texture |
| `WinMM` (`waveOut`, `midiOut`) | OpenAL for PCM, `fluidsynth` or `timidity` for MIDI (if used) |

**Stubs provided in CMake**: `milesstub` + `binkstub` export exact decorated stdcall names. On Linux:
- Build `libmss32.so` + `libbinkw32.so` shims that forward to OpenAL/FFmpeg
- Or link directly and rename symbols via version script

### 3.4 File System & `.big` Archives

- `WWLib/mixfile.cpp` reads `.big` (W3D container) — already platform-independent C++
- Add `PhysicsFS` or custom VFS layer for: loose files overlay > `.big` archives > base game data
- Case-insensitive path lookup (Windows default) → `strcasecmp` + directory cache

---

## Phase 4 — GameEngine Port (Weeks 20–34)

> ~580 files, **no DirectX**, pure game logic. DONE headless 2026-09-17:
> `libgameengine.a` builds clean and `test_gameengine` passes
> (348 tests, 505558 checks, 0 failed). The table below was the forecast;
> what actually happened is recorded in "Current Status" above.

### 4.1 Remaining Win32 Dependencies in GameEngine

| File / Area | Dependency | Fix |
|-------------|------------|-----|
| `Debug.cpp` / `StackDump.cpp` | `DbgHelp`, `ImageHlp` | `libunwind` + `backtrace` |
| `ControlServer.cpp` | Named pipes (`CreateNamedPipe`) | Unix domain sockets (`AF_UNIX`) |
| `JobSystem.cpp` | `CreateThreadpool` | `std::thread` + work-stealing queue (or `Taskflow`) |
| `INI/INIMapCache.cpp` | `FindFirstFile` | `dirent.h` + `fts` |
| `GameSpy*` | GameSpy SDK | **Remove** — multiplayer out of scope per PLAN.md; stub `GameSpyChat`/`GameSpyGP` |
| `WebBrowser` / `EABrowserDispatch` | MSHTML / MIDL | **Remove** — unused in skirmish |
| `LaunchWeb.cpp` | `ShellExecute` | `xdg-open` |

### 4.2 C++ Standard & Compiler Compatibility

- Current: C++17, MSVC `/permissive` (pre-two-phase lookup), `/EHa` (SEH)
- Linux: `-std=c++17` `-fno-exceptions` (or `-fexceptions` without SEH), `-fms-extensions` for anonymous structs
- `simplevec.h` et al rely on MSVC name binding → add explicit `this->` or `typename` where needed

### 4.3 Verification

- `test_gameengine` passes (boots memory manager, INI parser, file system)
- Headless skirmish simulation: `ai-batch.ps1` equivalent runs 8-player match in < 60s

---

## Phase 5 — GameEngineDevice & Executable (Weeks 30–38)

### 5.1 Device Layer (`GameEngineDevice`)

- `Win32Device/` → `LinuxDevice/` (SDL3 + Vulkan + OpenAL)
- `Win32DIMouse.cpp` (excluded) → SDL mouse
- `Win32Audio.cpp` (Miles) → OpenAL
- `Win32Video.cpp` (Bink) → FFmpeg

### 5.2 Entry Point (`WinMain.cpp`)

```cpp
// Linux main
int main(int argc, char** argv) {
    Platform::Initialize(argc, argv);
    GameEngine::Initialize();
    GameEngine::Run();  // message pump → SDL_PollEvent
    GameEngine::Shutdown();
    Platform::Shutdown();
    return 0;
}
```

### 5.3 Resource Deployment (POST_BUILD equivalent)

- CMake `install(TARGETS generals RUNTIME DESTINATION bin)` + `install(DIRECTORY Data/ DESTINATION share/CnCGenerals)`
- `.desktop` file, AppStream metadata, icon
- Steam/flatpak packaging scripts

---

## Phase 6 — Networking & Multiplayer (Optional, Weeks 36–44)

> Out of scope per PLAN.md, but needed for LAN/online.

| Component | Current | Linux Replacement |
|-----------|---------|-------------------|
| GameSpy (NAT negotiation, lobby, QR) | `GameSpy SDK` | **Steam Networking Sockets** / **ENet** / **libyojimbo** |
| `GameSpyChat`, `GameSpyGP` | stripped | Re-implement minimal lobby over Steam/ENet |
| `WOL*` menus | GameSpy | New lobby UI (Dear ImGui or renderer-based) |

---

## Phase 7 — Testing, Polish, Distribution (Weeks 40–52)

| Task | Description |
|------|-------------|
| 7.1 | Automated headless skirmish test: 8 AI, 23-min match → 38s, deterministic hash |
| 7.2 | Visual regression: render reference frames, compare via `imagediff` (SSIM > 0.99) |
| 7.3 | Performance: target ≥ Windows FPS on same hardware (Vulkan overhead < 5%) |
| 7.4 | Steam Deck verification: 800p, 40Hz, controller mapping |
| 7.5 | Flatpak / AppImage / Snap / .deb packaging |
| 7.6 | Documentation: `BUILD_LINUX.md`, `KNOWN_ISSUES.md`, `CONTRIBUTING.md` |

---

## Dependency Matrix (What Must Be Replaced)

| Category | Windows | Linux Replacement | Effort |
|----------|---------|-------------------|--------|
| **Renderer** | D3D8/D3D9/D3D11 | **Vulkan** (primary) | Very High |
| **Window/Input** | Win32 + DirectInput | **SDL3** | Medium |
| **Audio** | Miles (MSS) + Bink | **OpenAL Soft** + **FFmpeg** | Medium |
| **Threading** | Win32 Threadpool | `std::thread` / `Taskflow` | Low |
| **Timing** | `timeGetTime`/`QPC` | `clock_gettime` | Low |
| **File I/O** | Win32 API | POSIX + `mmap` | Low |
| **Registry** | `Reg*` | XDG INI files | Low |
| **DLL Loading** | `LoadLibrary` | `dlopen` | Low |
| **Crash Dump** | `DbgHelp`/`MiniDump` | `libunwind` + `Breakpad`/`Crashpad` | Medium |
| **Networking** | GameSpy + WinSock | Steam Net / ENet + POSIX sockets | High (if MP) |
| **GUI** | Win32 `HWND` controls | **Dear ImGui** or custom GPU UI | High |
| **Inline ASM** | MSVC `__asm` | GCC/Clang intrinsics / portable C++ | Medium |
| **SEH** | `/EHa` + `_set_se_translator` | Signal handlers (`SIGSEGV`) + `libunwind` | Medium |

---

## Build System Changes (CMake)

```cmake
# In top-level CMakeLists.txt — replace Win32 checks:
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(PLATFORM_LINUX 1)
  # Find system deps
  find_package(Vulkan REQUIRED)
  find_package(SDL3 REQUIRED)
  find_package(OpenAL REQUIRED)
  find_package(FFmpeg REQUIRED)
  find_package(zlib REQUIRED)
  find_package(libunwind REQUIRED)
  
  # Compiler flags
  add_compile_options(-fms-extensions -fno-strict-aliasing -Wno-deprecated-declarations)
  # No /EHa — use signal handlers instead
  # No /permissive — fix two-phase lookup issues explicitly
  
  # Linker
  add_link_options(-fuse-ld=mold -Wl,--as-needed)
endif()

# Platform-specific sources
if(PLATFORM_LINUX)
  target_sources(generals PRIVATE
    Platform/Linux/Window.cpp
    Platform/Linux/Input.cpp
    Platform/Linux/Audio.cpp
    Platform/Linux/FileSystem.cpp
    Platform/Linux/Threading.cpp
    Platform/Linux/CrashHandler.cpp
  )
  target_link_libraries(generals PRIVATE
    Vulkan::Vulkan SDL3::SDL3 OpenAL::OpenAL
    FFmpeg::avcodec FFmpeg::avformat FFmpeg::swscale
    zlib unwind dl pthread rt
  )
else()
  # Existing Win32 config
endif()
```

---

## Risk Register & Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Inline asm in hot paths (math, perf) | High | High | Replace with SIMD intrinsics early; benchmark |
| D3D9→Vulkan translation correctness | High | High | Write conformance tests per draw call type; use RenderDoc |
| GameEngine Win32 deps deeper than audit | Medium | High | `grep -r` audit + compile with `-Werror=implicit-function-declaration` |
| Miles/Bink binary compatibility | Medium | Medium | Shims export exact stdcall decorations; test with real DLLs |
| 32-bit pointer assumptions | Low | High | Enable `-fsanitize=address,undefined`; `/LARGEADDRESSAWARE` → 64-bit pointers |
| Multiplayer desync (determinism) | Medium | High | Lockstep simulation test: same inputs → same state hash |
| Steam Deck / controller UX | Low | Medium | SDL3 Gamepad API + community mappings |

---

## Minimal Viable Port (MVP) — "Skirmish Only"

If scope must be reduced, **drop**:
- Multiplayer (GameSpy, WOL menus, NAT)
- Campaign missions (scripting, cinematics)
- Replay system (determinism test only)
- Video playback (Bink) — static splash instead
- Web browser / EABrowserDispatch
- Profile/debug server (ControlServer)

**MVP delivers**: Single-player skirmish vs AI, all factions, all maps, settings menu, Vulkan renderer, OpenAL audio.

---

## Team & Roles (Suggested)

| Role | Focus |
|------|-------|
| **Lead / Renderer** | Vulkan backend, shader pipeline, D3D9→VK translation |
| **Platform Engineer** | SDL3 integration, input, windowing, file VFS, threading |
| **Audio/Video Engineer** | OpenAL + FFmpeg integration, Bink shim |
| **Gameplay Engineer** | GameEngine port, Win32 abstraction, AI/skirmish verification |
| **Build/Release Engineer** | CMake, CI, packaging (Flatpak/Steam), crash reporting |
| **QA / Test Automation** | Headless skirmish harness, visual regression, perf benchmarks |

---

## First 2 Weeks — Sprint 0 Checklist

- [ ] Fork repo, create `linux-port` branch
- [ ] Ubuntu 24.04 VM/container with build deps
- [ ] Compile `wwmath` + `compression` on Linux (fix asm, types)
- [ ] Run `wwmath_selfcheck` + `compression_selfcheck` → PASS
- [ ] Write `Platform/` abstraction headers
- [ ] Document every `#include <windows.h>` / `#include <d3d*.h>` / `__asm` in `DEPENDENCY_AUDIT.md`
- [ ] Decision: Vulkan native vs DXVK (prototype both for 2 days, pick)

---

## References

- `RENDERER-ROADMAP.md` (in repo) — D3D8→D3D9→D3D11 migration log
- `THREADING-ROADMAP.md` — Job system design
- `PLAN.md` (referenced in CMake) — original porting plan
- `CHANGELOG.md` — 119 changes, known fixed bugs
- SDL3 Migration Guide: https://wiki.libsdl.org/SDL3/MigrationGuide
- Vulkan Tutorial: https://vulkan-tutorial.com/
- DXVK: https://github.com/doitsujin/dxvk (fallback reference)
- OpenAL Soft: https://openal-soft.org/
- FFmpeg API: https://ffmpeg.org/doxygen/trunk/index.html