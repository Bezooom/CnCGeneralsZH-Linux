<a id="readme-top"></a>
<!-- best-readme-template -->
<div align="center">

<h1 align="center">C&C Generals: Zero Hour on Linux (Vulkan / SDL3)</h1>

<p align="center">
  C&C Generals: Zero Hour natively on Linux (Vulkan / SDL3), based on CnCGeneralsZH-Reforged: build, X11 vs Wayland measurements, renderer notes. Bring your own game data.
  <br />
  <br />
  <a href="https://github.com/Bezooom/CnCGeneralsZH-Linux/tree/HEAD/docs"><strong>Docs »</strong></a> · <a href="https://github.com/Bezooom/CnCGeneralsZH-Linux/issues/new?labels=bug">Report Bug</a> · <a href="https://github.com/Bezooom/CnCGeneralsZH-Linux/issues/new?labels=enhancement">Request Feature</a> · <a href="README.md">Русский</a>
</p>

<p align="center">
  <a href="https://github.com/Bezooom/CnCGeneralsZH-Linux/blob/HEAD/LICENSE.md"><img src="https://img.shields.io/badge/License-GPL--3.0%20%2B%20EA%20terms-blue?style=for-the-badge" alt="License" /></a>
  <a href="https://github.com/Bezooom/CnCGeneralsZH-Linux/stargazers"><img src="https://img.shields.io/github/stars/Bezooom/CnCGeneralsZH-Linux?style=for-the-badge" alt="Stars" /></a>
  <a href="https://github.com/Bezooom/CnCGeneralsZH-Linux/commits"><img src="https://img.shields.io/github/last-commit/Bezooom/CnCGeneralsZH-Linux?style=for-the-badge" alt="Last commit" /></a>
  <a href="https://github.com/Bezooom/CnCGeneralsZH-Linux/issues"><img src="https://img.shields.io/github/issues/Bezooom/CnCGeneralsZH-Linux?style=for-the-badge" alt="Issues" /></a>
</p>

</div>

<details open>
<summary><b>📑 Table of Contents</b></summary>

- [📖 About The Project](#-about-the-project)
- [🗺️ Open Source Ecosystem](#-open-source-ecosystem)
- [📦 Distribution Model (OpenRA Style)](#-distribution-model-openra-style)
- [🔬 Engineering Field Notes: RTX 3090 Benchmarks](#-engineering-field-notes-rtx-3090-benchmarks)
- [🚀 How to Run on Linux](#-how-to-run-on-linux)
- [🤝 Upstream Collaboration](#-upstream-collaboration)
- [🙏 Based on / Credits](#-based-on--credits)
- [🗺️ Roadmap](#-roadmap)
- [🤝 Contributing](#-contributing)
- [📄 License](#-license)
- [🧱 Built With](#-built-with)
- [📬 Contact](#-contact)

</details>

## 📖 About The Project

> **Native execution of the classic RTS on Linux without Wine or Proton: open-source ecosystem map, OpenRA-style distribution model (Bring-Your-Own-Assets), and engineering post-mortem on renderer optimizations.**

**Tested on hardware: Ubuntu 24.04 LTS, x86_64, NVIDIA GeForce RTX 3090 24 GB, X11 / Wayland.**

![Platform](https://img.shields.io/badge/Platform-Linux%20x86__64%20%7C%20Steam%20Deck-blue)
![Renderer](https://img.shields.io/badge/Renderer-Vulkan%20%7C%20SDL3-brightgreen)
![Model](https://img.shields.io/badge/Distribution-OpenRA%20Style%20%28BYOG%29-orange)
![Hardware](https://img.shields.io/badge/Tested%20on-RTX%203090-purple)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0%20%2B%20EA%20terms-blue)](LICENSE.md)

[Русская версия](README.md)

---

## 🗺️ Open Source Ecosystem

Following Electronic Arts' 2025 release of the *Command & Conquer: Generals* and *Zero Hour* source code under GPL v3 (with EA additional terms), the community maintains two primary repositories:

| Project | Purpose | Rendering & Platforms | Link |
| :--- | :--- | :--- | :--- |
| **Zero Hour Reforged** | Canonical modernized engine: 64-bit, Direct3D 11 / Vulkan / Metal, enhanced AI, built-in matchmaking, packages for Linux and macOS. | Windows (D3D11), macOS (Metal + SDL3), **Linux / Steam Deck (Vulkan + SDL3)** | [olcayseygan/CnCGeneralsZH-Reforged](https://github.com/olcayseygan/CnCGeneralsZH-Reforged) |
| **Generals Game Code** | Community codebase modernization and cleanup for C++20, supporting Visual Studio 2022 and cross-platform build environments (Docker). | Windows, Linux (builds/tooling) | [TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode) |
| **This Repository** | Engineering companion guide for Linux: profiling bottlenecks, X11 vs. Wayland benchmarks, renderer optimization analysis, and upstream proposals. | Linux-port working tree based on Reforged (upstream code and history) + benchmarks and notes | [Bezooom/CnCGeneralsZH-Linux](https://github.com/Bezooom/CnCGeneralsZH-Linux) |

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

## 🙏 Based on / Credits

- **Electronic Arts** — *Command & Conquer: Generals* and *Zero Hour* source code, released in 2025 under GPL v3 with EA additional terms: [electronicarts/CnC_Generals_Zero_Hour](https://github.com/electronicarts/CnC_Generals_Zero_Hour).
- **Olcay Seygan** — [CnCGeneralsZH-Reforged](https://github.com/olcayseygan/CnCGeneralsZH-Reforged) (64-bit, Vulkan / Metal / D3D11, SDL3). This repository is based on that tree and keeps its commit history; most of the engine code is his and the community's work.
- **TheSuperHackers** — [GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode), codebase modernization.
- My part (Bezooom): Linux build and testing on Ubuntu 24.04, X11 / Wayland measurements, renderer notes, documentation and upstream proposals (listed above).

---

## 🗺️ Roadmap

- [ ] Send the proposals from “Upstream Collaboration” to CnCGeneralsZH-Reforged
- [ ] Linux build check in GitHub Actions

See the [open issues](https://github.com/Bezooom/CnCGeneralsZH-Linux/issues) for the full list of ideas and known problems.

## 🤝 Contributing

Contributions are welcome. If you have an idea that would make this better, open an issue or a pull request:

1. Fork the repository
2. Create a branch (`git checkout -b feature/my-idea`)
3. Commit (`git commit -m 'feat: my idea'`)
4. Push (`git push origin feature/my-idea`)
5. Open a pull request

Detailed guidelines: [CONTRIBUTING.md](CONTRIBUTING.md).

## 📄 License

- All code in this repository is under **GPL v3 with EA additional terms** — see [LICENSE.md](LICENSE.md). This includes my changes.
- Game assets (`.big` archives, textures, audio, maps) are not included and remain the property of Electronic Arts.
- Research project, not affiliated with Electronic Arts. Command & Conquer and Generals are trademarks of Electronic Arts Inc.

## 🧱 Built With

<p>
<img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" alt="C++" />
<img src="https://img.shields.io/badge/Vulkan-1.1-AC162C?style=for-the-badge&logo=vulkan&logoColor=white" alt="Vulkan" />
<img src="https://img.shields.io/badge/SDL-3-1D4ED8?style=for-the-badge" alt="SDL" />
<img src="https://img.shields.io/badge/CMake-build-064F8C?style=for-the-badge&logo=cmake&logoColor=white" alt="CMake" />
<img src="https://img.shields.io/badge/Linux-x86__64-FCC624?style=for-the-badge&logo=linux&logoColor=white" alt="Linux" />
</p>

## 📬 Contact

**Pavel Naumov** (Bezooom) — [naumovai.ru](https://naumovai.ru) · [X @Bezooooom](https://x.com/Bezooooom) · naumov122@gmail.com

Project: [https://github.com/Bezooom/CnCGeneralsZH-Linux](https://github.com/Bezooom/CnCGeneralsZH-Linux)

<p align="right">(<a href="#readme-top">back to top</a>)</p>
