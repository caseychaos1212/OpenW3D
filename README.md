# OpenW3D

OpenW3D is a modernization fork of the released Command & Conquer Renegade source code.
The near-term goal is to keep retail gameplay compatibility while moving the codebase and tools to modern build systems and platforms.
Active development discussion happens on Discord: https://discord.gg/wSsghDAF4B

## Project Goals

Current goals are to modernize the codebase in a practical order while keeping momentum and maintaining compatibility where possible.

- 64-bit support and stability.
- Cross-platform support.
- Engine improvements.

In the near term, the project targets minimal gameplay-impacting changes while compatibility work is underway. Linux builds are also useful for sanitizer-driven debugging, which helps improve x64 stability across platforms.

## Build Instructions

### Prerequisites
- CMake 3.25 or newer.
- Ninja.
- A C++20-capable compiler.
- Network access for CMake `FetchContent` dependencies used by this tree.
- On Windows, Visual Studio with C++ tools installed.

### Current platform status
- Windows: primary supported build path.
- Linux: partial/in-progress support (some modules build, full game build is not yet the default expectation).

### 1) Modern CMake build (up-to-date compiler path)

Use a Visual Studio Developer Command Prompt (for example, x64 Native Tools) so MSVC and Windows SDK environment variables are set correctly.

Configure:
```powershell
cmake --preset win
```

Build:
```powershell
cmake --build --preset win --config Release
```

### 2) Linux (partial/in-progress)

Linux support is currently best-effort and typically focuses on non-client targets.

Configure:
```bash
cmake --preset linux -DRENEGADE_CLIENT=OFF -DRENEGADE_FDS=OFF
```

Build:
```bash
cmake --build build/linux --config Release
```

### 3) Useful CMake options
- `-DRENEGADE_CLIENT=ON|OFF` to build or skip the game client target.
- `-DRENEGADE_FDS=ON|OFF` to build or skip dedicated server target.
- `-DRENEGADE_TOOLS=ON|OFF` to build or skip legacy tool targets.
- `-DW3D_BUILD_OPTION_FFMPEG=ON|OFF` to toggle FFmpeg integration.

## License

OpenW3D is licensed under GPL v3 with additional terms.

See `LICENSE.md` for the full text, including:
- GNU GPL v3 terms.
- Additional Section 7 terms from Electronic Arts (including trademark/publicity restrictions and required notices).
