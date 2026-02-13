# OpenW3D

OpenW3D is a modernization fork of the released Command & Conquer Renegade source code.
The near-term goal is to keep retail gameplay compatibility while moving the codebase and tools to modern build systems and platforms.

## Roadmap

### Phase I: Build with an up-to-date C++ compiler
- Status: mostly complete.
- Goal: keep compatibility with the retail game so binaries can still run as a drop-in replacement in a Renegade install.

### Phase II: Full 64-bit support
- Status: in progress.
- Goal: support 64-bit builds while preserving compatibility with the retail game.

Phases I and II are intended to include only minimal bug fixes needed for playability and stability. Campaign completion and multiplayer operation should remain possible, but retail-era netcode limitations and exploits are expected until compatibility is intentionally broken.

### Phase III: Cross-platform support
- Initial direction: keep DX8-era rendering path where needed, with DXVK on Linux as a bridge.
- Tooling direction: move developer tools (for example LevelEdit and W3DViewer) toward cross-platform Qt-based implementations.

### Phase IV: Updated netcode
- Intentionally break retail multiplayer compatibility.
- Remove known hacks/exploits and improve behavior such as rubberbanding.

### Phase V: Updated renderer
- Replace DX8-era rendering with a Vulkan-based renderer.

Phases III, IV, and V are not strictly sequential and may be developed in parallel.

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
