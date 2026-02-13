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

Current Qt tool targets are `wwconfig_qt` and `wdump_qt` on Windows/Linux, plus `w3dview_qt` on Windows.

### Prerequisites
- CMake 3.25 or newer.
- Ninja.
- A C++20-capable compiler.
- [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set.
- Network access for CMake `FetchContent` dependencies used by this tree.

### 1) Install vcpkg dependencies

Windows:
```powershell
vcpkg install --triplet x64-windows
```

Linux:
```bash
vcpkg install --triplet x64-linux
```

Dependencies are declared in `vcpkg.json` (Qt + FFmpeg packages).

### 2) Configure

Windows full build:
```powershell
cmake --preset windows-qt
```

Windows tools-only build:
```powershell
cmake --preset windows-qt-tools
```

Linux (Qt-focused preset):
```bash
cmake --preset linux-qt
```

If you only want tools on Linux, disable game targets explicitly:
```bash
cmake --preset linux-qt -DRENEGADE_CLIENT=OFF -DRENEGADE_FDS=OFF
```

### 3) Build

Windows:
```powershell
cmake --build --preset windows-qt --config Release
```

Windows tools-only:
```powershell
cmake --build --preset windows-qt-tools --config Release
```

Linux:
```bash
cmake --build --preset linux-qt --config Release
```

### 4) Useful CMake options
- `-DW3D_BUILD_QT_TOOLS=ON|OFF` to toggle Qt tool targets.
- `-DRENEGADE_CLIENT=ON|OFF` to build or skip the game client target.
- `-DRENEGADE_FDS=ON|OFF` to build or skip dedicated server target.
- `-DRENEGADE_TOOLS=ON|OFF` to build or skip legacy tool targets.
- `-DW3D_BUILD_OPTION_FFMPEG=ON|OFF` to toggle FFmpeg integration.

## License

OpenW3D is licensed under GPL v3 with additional terms.

See `LICENSE.md` for the full text, including:
- GNU GPL v3 terms.
- Additional Section 7 terms from Electronic Arts (including trademark/publicity restrictions and required notices).
