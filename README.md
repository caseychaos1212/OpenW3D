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
- Network access for CMake `FetchContent` dependencies used by this tree (unless using offline mode below).
- On Windows, Visual Studio with C++ tools installed.
- For vcpkg-based presets, set `VCPKG_ROOT` and install bootstrap tools (`curl`, `zip`, `unzip`, `tar` on Linux).

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

Linux support is currently best-effort and currently focuses on core non-DX/non-Qt targets.

Configure:
```bash
cmake --preset linux-core
```

Build:
```bash
cmake --build --preset linux-core
```

For offline Linux builds, add local dependency paths as needed (for example `-DW3D_FETCHCONTENT_OFFLINE=ON -DW3D_MILES_SOURCE_DIR=... -DW3D_GAMESPY_SOURCE_DIR=...`).

### 3) Useful CMake options
- `-DW3D_CLIENT=ON|OFF` to build or skip the game client target.
- `-DW3D_FDS=ON|OFF` to build or skip dedicated server target.
- `-DW3D_TOOLS=ON|OFF` to build or skip legacy tool targets.
- `-DW3D_BUILD_OPTION_FFMPEG=ON|OFF` to toggle FFmpeg integration.
- `-DW3D_BUILD_LEVELEDIT_QT=ON|OFF` to build the experimental Qt LevelEdit target on Windows when Qt tools are enabled.
- `-DW3D_LEVELEDIT_QT_PROFILE=public|full` to select Qt LevelEdit profile surface (`public` default).
- `-DW3D_LEVELEDIT_PROFILE=public|full` to select legacy LevelEdit profile behavior (`public` default).
- `-DW3D_LEVELEDIT_GIT_SCM=ON|OFF` to enable Git-backed source-control integration scaffolding for LevelEdit full profile.
- `-DW3D_LEVELEDIT_DDB_JSON_MIRROR=ON|OFF` to emit deterministic `.ddb.json` sidecars when preset DB files are saved.
- `-DW3D_FETCHCONTENT_OFFLINE=ON|OFF` to disable network downloads for `FetchContent`.
- `-DW3D_MILES_SOURCE_DIR=/path/to/miles-sdk-stub` (or `FETCHCONTENT_SOURCE_DIR_MILES`) to use a local Miles stub checkout.
- `-DW3D_GAMESPY_SOURCE_DIR=/path/to/GamespySDK` (or `FETCHCONTENT_SOURCE_DIR_GAMESPY`) to use a local GameSpy checkout.
- `-DW3D_BINK_SOURCE_DIR=/path/to/bink-sdk-stub` (or `FETCHCONTENT_SOURCE_DIR_BINK`) to use a local Bink stub checkout.
- `-DW3D_CRUNCH_SOURCE_DIR=/path/to/crunch` (or `FETCHCONTENT_SOURCE_DIR_CRUNCH`) to use a local Crunch checkout (required when tools are enabled in offline mode).

### 4) LevelEdit settings file (registry-free)

`leveledit` and `leveledit_qt` use an INI settings file instead of registry-backed profiles.

- Default path: next to `leveledit.exe`, as `LevelEdit.ini`.
- Optional override: set `OPENW3D_LEVELEDIT_CONFIG_INI` (or `LEVELEDIT_CONFIG_INI`) to a full file path.
- Asset tree location is stored under section/key: `[Config]` / `Asset Tree`.
- Renegade source-data lookup for `Always.dbs`/`Always.dat` can be set without registry:
  - Environment override: `OPENW3D_RENEGADE_INSTALL_PATH` (fallback `RENEGADE_INSTALL_PATH`).
  - INI key override: `[Config]` / `Renegade Install Path`.
  - Accepted values: game install folder, `Data` folder, or full path to `Renegade.exe`.
- Final fallback for legacy compatibility is still Renegade registry key `HKCU\Software\Westwood\Renegade\InstallPath`.

### 5) Offline FetchContent mode

When `-DW3D_FETCHCONTENT_OFFLINE=ON`, configure will fail early unless local source directories are provided for each required dependency (`Miles`, `GameSpy`, `Bink` when enabled on Windows, and `Crunch` when tools are enabled). This avoids slow retries and makes dependency requirements explicit for offline environments.

### 6) LevelEdit DDB mirror verification

When tools are enabled, `leveledit_dbmirror` is built and can validate expected JSON sidecar structure for preset DB mirrors:

```bash
leveledit_dbmirror verify --ddb <path-to-ddb> --json <path-to-ddb.json>
```

## License

OpenW3D is licensed under GPL v3 with additional terms.

See `LICENSE.md` for the full text, including:
- GNU GPL v3 terms.
- Additional Section 7 terms from Electronic Arts (including trademark/publicity restrictions and required notices).
