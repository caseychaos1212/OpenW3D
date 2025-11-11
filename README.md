
# Command & Conquer Renegade

This repository includes source code for Command & Conquer Renegade. This release provides support to the [Steam Workshop](https://steamcommunity.com/workshop/browse/?appid=2229890) for the game.


## Dependencies

If you wish to rebuild the source code and tools successfully you will need to find or write new replacements (or remove the code using them entirely) for the following libraries;

- DirectX SDK (Version 8.0 or higher) (expected path `\Code\DirectX\`)
- RAD Bink SDK - (expected path `\Code\BinkMovie\`)
- RAD Miles Sound System SDK - (expected path `\Code\Miles6\`)
- NvDXTLib SDK - (expected path `\Code\NvDXTLib\`)
- Lightscape SDK - (expected path `\Code\Lightscape\`)
- Umbra SDK - (expected path `\Code\Umbra\`)
- GameSpy SDK - (expected path `\Code\GameSpy\`)
- GNU Regex - (expected path `\Code\WWLib\`)
- SafeDisk API - (expected path `\Code\Launcher\SafeDisk\`)
- Microsoft Cab Archive Library - (expected path `\Code\Installer\Cab\`)
- RTPatch Library - (expected path `\Code\Installer\`)
- Java Runtime Headers - (expected path `\Code\Tools\RenegadeGR\`)


## Compiling (Win32 Only)

To use the compiled binaries, you must own the game. The C&C Ultimate Collection is available for purchase on [EA App](https://www.ea.com/en-gb/games/command-and-conquer/command-and-conquer-the-ultimate-collection/buy/pc) or [Steam](https://store.steampowered.com/bundle/39394/Command__Conquer_The_Ultimate_Collection/). 

### Renegade

The quickest way to build all configurations in the project is to open `commando.dsw` in Microsoft Visual Studio C++ 6.0 (SP5 recommended for binary matching to patch 1.037) and select Build -> Batch Build, then hit the “Rebuild All” button.

If you wish to compile the code under a modern version of Microsoft Visual Studio, you can convert the legacy project file to a modern MSVC solution by opening the `commando.dsw` in Microsoft Visual Studio .NET 2003, and then opening the newly created project and solution file in MSVC 2015 or newer.

NOTE: As modern versions of MSVC enforce newer revisions of the C++ standard, you will need to make extensive changes to the codebase before it successfully compiles, even more so if you plan on compiling for the Win64 platform.

When the workspace has finished building, the compiled binaries will be copied to the `/Run/` directory found in the root of this repository. 


### Free Dedicated Server
It’s possible to build the Windows version of the FDS (Free Dedicated Server) for Command & Conquer Renegade from the source code in this repository, just uncomment `#define FREEDEDICATEDSERVER` in [Combat\specialbuilds.h](Combat\specialbuilds.h) and perform a “Rebuild All” action on the Release config.


### Level Edit (Public Release)
To build the public release build of Level Edit, modify the LevelEdit project settings and add `PUBLIC_EDITOR_VER` to the preprocessor defines.

## Modern CMake + Qt toolchain (x64)

The active development branch adds 64-bit, cross-platform Qt replacements for the legacy MFC utilities (WWConfig, WDump, LevelEdit, W3DView). To configure this toolchain:

1. Install [vcpkg](https://github.com/microsoft/vcpkg) and set the `VCPKG_ROOT` environment variable to its checkout.
2. (Windows) Create short, writable paths for the build artifacts to avoid long-path issues, e.g. `mkdir C:\vcpkg.installed` and `mkdir C:\build\openw3d`.
3. Run `vcpkg install --triplet x64-windows` (or `x64-linux`) to build the dependencies declared in `vcpkg.json` (`qtbase`, `qttools`, `qtimageformats`, `qtsvg`, `qt5compat`, `ffmpeg`, etc.).
4. Configure with one of the provided presets:
   - Windows: `cmake --preset windows-qt`
   - Windows tools-only: `cmake --preset windows-qt-tools`
   - Linux: `cmake --preset linux-qt`
5. Build with `cmake --build --preset windows-qt` (or `linux-qt`). The Qt-enabled tools can be toggled at configure time with `-DW3D_BUILD_QT_TOOLS=ON`.

Presets assume Ninja and the vcpkg toolchain file. If you keep vcpkg elsewhere, override `CMAKE_TOOLCHAIN_FILE`/`VCPKG_TARGET_TRIPLET` when configuring.

When the Qt option is enabled, the legacy WWConfig (MFC) binary still builds, and a new prototype `wwconfig_qt` target is produced alongside the other tools in your build output directory.

### Offline-friendly Miles stub

The build uses a small stub of the Miles Sound System, fetched from `https://github.com/TheSuperHackers/miles-sdk-stub`. If you are on a restricted network, clone or copy that repository yourself (e.g. into `C:\deps\miles-sdk-stub`) and point CMake at it with either:

- Environment variable: `set MILES_STUB_SOURCE_DIR=C:\deps\miles-sdk-stub`
- Configure flag: `cmake --preset windows-qt -DMILES_STUB_SOURCE_DIR=C:\deps\miles-sdk-stub`

When `MILES_STUB_SOURCE_DIR` is set, CMake skips the network fetch and uses the local sources instead.


## Known Issues

The “Debug” configuration of the “Commando” project (the Renegade main project) will sometimes fail to link the final executable. This is due to Windows Defender incorrectly detecting RenegadeD.exe containing a virus (possibly due to the embedded browser code). Excluding the output `/Run/` folder found in the root of this repository in Windows Defender should resolve this for you. 


## Contributing

This repository will not be accepting contributions (pull requests, issues, etc). If you wish to create changes to the source code and encourage collaboration, please create a fork of the repository under your GitHub user/organization space.


## Support

This repository is for preservation purposes only and is archived without support. 


## License

This repository and its contents are licensed under the GPL v3 license, with additional terms applied. Please see [LICENSE.md](LICENSE.md) for details.
