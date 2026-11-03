# Linux port

This fork builds and runs Command & Conquer: Generals Zero Hour on Linux. The game content is the
2003 release, unchanged; what this fork adds is the layer underneath it: the engine's Direct3D 8
calls are decoded into one canonical vertex format and replayed through Vulkan, and PAL supplies
the files, timing, registry, input and window the engine expects from Windows.

The work is tracked in [`PORT_TO_LINUX_PLAN.md`](../PORT_TO_LINUX_PLAN.md).

## Requirements

- GCC 13 and a C++17 toolchain
- CMake 3.20+ and Ninja
- Vulkan loader and headers, with a working Vulkan 1.1 device
- OpenAL, libav (for audio and video)
- SDL2 is not used; the window is a plain Xlib client

## Build and test

```console
cmake -S GeneralsMD/Code -B GeneralsMD/build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build GeneralsMD/build-linux
ctest --test-dir GeneralsMD/build-linux --output-on-failure
```

The build copies `generals` into `GeneralsMD/Run/`. You still need your own Zero Hour install for
the data files, exactly as the Windows build does: put the game's `*.big` archives and the base
game's `*.big` files where the Windows instructions say.

## Running

Run it from an X11 session. The window is an Xlib client, so under a Wayland session it goes
through XWayland. In the same match the present call measured about 40-60 ms there against 0.05 ms
in an X11 session; the XWayland path is the likely cost, and a native Wayland backend is the
planned fix.

## What the port contains

| Area | Where |
| :--- | :--- |
| PAL: files, timing, registry, input, window | `GeneralsMD/Code/Platform/PAL_*.cpp` |
| Direct3D 8 to Vulkan shim | `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/ww3d_d3d9.h`, `GeneralsMD/Code/Platform/PAL_Vulkan.cpp` |
| Tests, including the Vulkan smoke and PAL suites | `GeneralsMD/Code/Tests` |

The Vulkan surface owns its own Xlib connection, closed before the window's. Sharing the window's
`Display` crashed in `XCloseDisplay` at shutdown; the separate connection is the empirically
working arrangement, not a proved root cause.

## Known limits

- The minimap is black at match start for China until its radar upgrade is bought; the USA radar
  shows immediately.
- The Vulkan renderer keeps a single pipeline per blend mode and one canonical vertex layout, so
  the shim is the cost centre on slow paths; the terrain now culls its tiles and the interface
  batches by texture page.

## Licensing

The source is GPL v3, inherited from the code EA released. The repository history was scrubbed of
every asset blob it carried: the tree now contains no `.big` archive, map, model or texture from any
source, and none may be added.

The WW3D engine expects four `RequiredAssets` files at runtime (`w3d_missing_texture.tga` and the
projector and shatter samples). They are not in this repository; take them from the WW3D source drop
alongside your game data if a path needs them.
