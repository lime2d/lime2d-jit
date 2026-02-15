# Lime2D (LuaJIT Edition)

**Lua Integrated Monochromatic Engine**

Lime2D is a lightweight monochromatic graphics engine for Windows with an integrated Lua interpreter. It provides a retro-inspired, pixel-perfect rendering environment for creating games, tools, and interactive applications — all driven by Lua scripting.

The engine renders to a 1-bpp logical canvas (640×400 or 640×360 depending on display aspect ratio) using an 8×16 IBM VGA-style monospace font, with support for geometric primitives, text, and 1-bit images. Drawing is performed efficiently via an OpenGL SSBO-backed pipeline. The Release EXE is under 1 MB with no external DLL dependencies.

## Variants

Lime2D comes in two variants, each in its own repository:

| Variant | Lua Runtime | Repository |
|---------|-------------|------------|
| **Lime2D (LuaJIT)** | LuaJIT (Lua 5.1) | [lime2d/lime2d-jit](https://github.com/lime2d/lime2d-jit) ← you are here |
| **Lime2D** | Lua 5.5 | [lime2d/lime2d](https://github.com/lime2d/lime2d) |

Both variants expose an identical Lua scripting API. Choose LuaJIT for faster script execution, or Lua 5.5 for the latest language features.

## Quick Start (Download & Use)

If you just want to create Lime2D applications without building from source, grab the latest release package:

**[⬇ Download Lime2D v5.1.4.2 (Win64)](https://github.com/lime2d/lime2d-jit/releases/tag/v5.1.4.2)**

The release package contains the ready-to-use EXE, example scripts (Hello World, Pong, Snake, and an image demo), and full documentation. To run an example, drag any `.lua` file onto `lime2d-jit.exe` or isolate them together in the same folder.

Minimal script to get started:

```lua
-- MAINSCRIPT

local lg = lime.graphics

function lime.draw()
    lg.clear()
    lg.locate(0, 0)
    lg.print("Hello World")
end
```

Save this as a `.lua` file, place it alongside `lime2d-jit.exe`, and run the EXE.

For the complete API reference covering graphics, keyboard input, window management, filesystem, profiling, modules, and fused executable distribution, see the **[Lime2D Reference Manual](https://github.com/lime2d/lime2d-jit/blob/main/src/Lime2D%20Reference%20Manual.md)**.

## Building from Source

The repository includes Visual Studio solution and project files (`.sln` / `.vcxproj`) targeting C++20 on Windows (x64). Pre-built `lua51.lib` static libraries for both Debug and Release configurations are included in the repo.

Detailed build instructions — including project property configuration, library dependencies, and post-build steps — can be found in the comment block at the top of [`src/main.cpp`](https://github.com/lime2d/lime2d-jit/blob/main/src/main.cpp).

## System Hotkeys

| Key | Action |
|-----|--------|
| **F10** | System info screen (version, CP437 character map) |
| **F11** | Toggle fullscreen |
| **F12** | Console output screen |

These hotkeys are reserved by the engine and cannot be overridden by scripts.

## License

Lime2D is released under the MIT License. See [LICENSE](LICENSE) for details.
