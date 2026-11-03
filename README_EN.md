# C&C Generals: Zero Hour on Linux (Vulkan / SDL3)

> **Native execution of the classic RTS on Linux without Wine or Proton: open-source ecosystem map, OpenRA-style distribution model (Bring-Your-Own-Assets), and engineering post-mortem on renderer optimizations.**

**Tested on hardware: Ubuntu 24.04 LTS, x86_64, NVIDIA GeForce RTX 3090 24 GB, X11 / Wayland.**

![Platform](https://img.shields.io/badge/Platform-Linux%20x86__64%20%7C%20Steam%20Deck-blue)
![Renderer](https://img.shields.io/badge/Renderer-Vulkan%20%7C%20SDL3-brightgreen)
![Model](https://img.shields.io/badge/Distribution-OpenRA%20Style%20%28BYOG%29-orange)
![Hardware](https://img.shields.io/badge/Tested%20on-RTX%203090-purple)
![License](https://img.shields.io/badge/License-MIT-yellow)

[Русская версия](README.md)

---

## 🗺️ Open Source Ecosystem

Following Electronic Arts' 2025 release of the *Command & Conquer: Generals* and *Zero Hour* source code under GPL v3 (with EA additional terms), the community maintains two primary repositories:

| Project | Purpose | Rendering & Platforms | Link |
| :--- | :--- | :--- | :--- |
| **Zero Hour Reforged** | Canonical modernized engine: 64-bit, Direct3D 11 / Vulkan / Metal, enhanced AI, built-in matchmaking, packages for Linux and macOS. | Windows (D3D11), macOS (Metal + SDL3), **Linux / Steam Deck (Vulkan + SDL3)** | [olcayseygan/CnCGeneralsZH-Reforged](https://github.com/olcayseygan/CnCGeneralsZH-Reforged) |
| **Generals Game Code** | Community codebase modernization and cleanup for C++20, supporting Visual Studio 2022 and cross-platform build environments (Docker). | Windows, Linux (builds/tooling) | [TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode) |
| **This Repository** | Engineering companion guide for Linux: profiling bottlenecks, X11 vs. Wayland benchmarks, renderer optimization analysis, and upstream proposals. | Field notes, benchmark results, draft pull requests | [Bezooom/CnCGeneralsZH-Linux](https://github.com/Bezooom/CnCGeneralsZH-Linux) |

---

## 📦 Distribution Model (OpenRA Style)

Open-source reimplementations of classic games (OpenRA, fheroes2, GZDoom, DXX-Rebirth) maintain a strict separation between code and game data:

1. **Open Engine**: Engine source code is released under GPL v3. Anyone can compile, modify, and distribute the binary.
2. **No Proprietary Game Files**: This repository contains **no** EA copyrighted game assets (no `.big` archives, maps, models, textures, sounds, or videos).
3. **Bring Your Own Game (BYOG)**:
   - Players legally acquire a copy of the game (e.g. *Command & Conquer: The Ultimate Collection* on Steam or EA App, or physical CD-ROMs).
   - Linux packages (Flatpak/Flathub, AppImage, `.deb`) distribute only the compiled engine binary.
   - On first launch, the engine discovers game files automatically via Steam library folders (`~/.local/share/Steam/steamapps/common/...`) or prompts the user to select the Zero Hour directory.
   - Original `.big` files are mounted strictly read-only.

---

## 🔬 Engineering Field Notes: RTX 3090 Benchmarks

During local profiling on a test rig (Ubuntu 24.04, NVIDIA GeForce RTX 3090 24 GB), several critical performance and visual bottlenecks were analyzed:

### 1. Presentation Latency: Wayland vs. X11

When running an X11 client under a Wayland desktop session via XWayland, frames experienced substantial queue delays:

| Metric | Wayland Session (XWayland) | Native X11 Session | Difference |
| :--- | :--- | :--- | :--- |
| Full frame time (`total`) | ~78–135 ms | **11.1 ms** | **7–12× faster** |
| `vkQueuePresentKHR` latency | 40–62 ms | **0.05 ms** (47–52 µs) | **800× reduction** |
| CPU draw submission | 13–52 ms | **8.6 ms** | 2–6× faster |
| In-game HUD render rate | 7–13 FPS | **20 FPS** (11.1 ms frame) | Smooth response |

**Takeaway**: In a Wayland session, measured presentation latency hovered at 40–62 ms, while in a pure X11 session it dropped to 0.05 ms (most likely due to XWayland compositor handoff queues). For peak performance on Xlib windows, running in an X11 session («GNOME on Xorg») is strongly recommended.

---

### 2. Terrain Tile Frustum Culling (`Cull_Box`)

Retail render loops iterated across every map tile on every shader pass without visibility checks:

| Parameter | Without Culling | With AABB `Cull_Box` | Impact |
| :--- | :--- | :--- | :--- |
| Total draw calls / frame | 1380 → 771 | **432** | Minus **948 calls** |
| Terrain calls (`catTerrain`) | 624 → 508 | **172** | **72% reduction** |
| CPU draw submission | 52 ms | **14.6 ms** | **37 ms saved** / frame |
| Frame time | 134 ms | **81 ms** (Wayland) / **11 ms** (X11) | Direct performance gain |

**Solution**: Test conservative tile bounding boxes (covering full map height range) using `CameraClass::Cull_Box()` in both base passes and additional passes. Experimental terrain tile frustum culling proposal (detailed below in upstream contributions).

---

### 3. UI Batching by Texture Atlas Page

The retail 2D renderer reset state on every distinct image object:

- `same2DTexture()` optimization: Keep batches open across distinct `Image` instances if they reference the same texture file (same atlas page).
- `winRepaint()` scope: Hold the window repaint pass in a unified batch, eliminating redundant state flushes between buttons and font glyphs.
- Call reduction: Dropped from 1380 to 885 calls per frame in the test scene.

---

### 4. Player Color Tint Fix

- **Symptom**: Battlefield terrain was tinted with a rectangular colored haze (red for China, blue for USA, purple for GLA).
- **Root Cause**: `W3DScene::renderStenciledPlayerColor()` draws a full tactical-view quad expecting stencil masking for occluded objects. Without stencil state configured in the Vulkan pipeline, the quad rendered unmasked across the viewport.
- **Solution**: Route through the standard non-stencil occlusion path (`Has_Stencil() = false` on Linux).

---

### 5. Radar / Minimap 24-Bit Surface Fix

- **Symptom**: Black radar terrain at match start.
- **Root Cause**: Radar format selection preferred 24-bit `R8G8B8` (3 bytes per pixel), but `SurfaceClass::Draw_Pixel()` only handled 1, 2, and 4 bytes (pixel writes were silent no-ops).
- **Solution**: Added 3-byte BGR pixel writing and prioritized 32-bit `X8R8G8B8` in the radar format preference list.

---

## 🚀 How to Run on Linux

### Option 1. Pre-built Upstream Packages (Recommended)

1. Own Zero Hour on Steam (*Command & Conquer: The Ultimate Collection*).
2. Download the latest Linux release from [Reforged](https://github.com/olcayseygan/CnCGeneralsZH-Reforged/releases/latest) (available as AppImage, Flatpak, `.deb`, `.rpm`, Arch package).
3. Point the launcher to your game files (or let it auto-detect Steam libraries).
4. Select an X11 session («GNOME on Xorg») on the login screen for minimal presentation delay.

### Option 2. Build From Upstream Source

```bash
git clone https://github.com/olcayseygan/CnCGeneralsZH-Reforged.git
cd CnCGeneralsZH-Reforged
./build-linux.sh Release generals
```

---

## 🤝 Upstream Collaboration

Observations and optimizations are documented and offered to the canonical Reforged repository:

- [ ] Terrain tile frustum culling: culling offscreen terrain VB tiles in `HeightMap.cpp` against camera frustum (`Cull_Box`)
- [ ] UI texture batching (`same2DTexture`)
- [ ] Stenciled player color guard (`Has_Stencil() = false` fallback)
- [ ] 24-bit BGR `Draw_Pixel` support

---

## 📄 License

- Notes, benchmarks, and documentation in this repository are licensed under [MIT](LICENSE).
- The original Command & Conquer Generals Zero Hour source code was released by Electronic Arts under GPL v3 with additional EA terms ([source license](https://github.com/TheSuperHackers/GeneralsGameCode/blob/main/LICENSE)).
- This is an independent research project and is not affiliated with Electronic Arts. Command & Conquer and Generals are trademarks of Electronic Arts Inc.
