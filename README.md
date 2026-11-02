
# OpenW3D

This project is a continuation of the "Command & Conquer: Renegade" game engine that was released as open source software by EA [here](https://github.com/electronicarts/CnC_Renegade).
It is being developed to be more portable to other platforms, to fix bugs and to develop a platform to build other Renegade like games on top of.

## Campaign Co-op

The `feature/campaign-coop` branch adds cooperative play to the original Renegade campaign. Co-op is still experimental; mission scripts and cinematics need further multiplayer playtesting. The instructions below cover the Windows client.

### Features

- Campaign hosting and joining through the **Co-op Campaign** menu, LAN discovery, or direct IPv4 connection. Sessions default to four players, including the host.
- Pre-game and between-mission lobbies with player lists, ready indicators, chat, host settings, and a host-controlled Start button.
- Shared campaign progression with synchronized mission transitions, objective updates and markers, scripted object visibility, and cinematic camera/control state.
- Shared weapon, ammunition, and key grants, including catch-up for joining and respawning players.
- Respawning near a safe teammate, with a waiting message and teammate spectating when an ally is in an unsafe location. A manual **Respawn** action is also available in the in-game menu.
- Character model selection through the EVA Characters screen, preserving health and inventory and retaining the selection across respawns.
- Teammate markers and player names, in-game chat and radio commands, scoring, and configurable death penalties.
- Mission results with completion time, objectives, team deaths, star ratings, and individual player statistics.
- Optional sprint, campaign dive animations, and host controls for difficulty, friendly fire, pickups, enemy strength, and AI behavior.
- A separate co-op network update setting: 60 updates per second by default, configurable from 5 to 120 on each machine.

### Set Up Each Player

1. Use matching Release builds of `feature/campaign-coop` on every machine. Build the branch using the [compilation instructions](#compiling-the-project), including the game client, scripts, and WWConfig. Keep `renegade.exe`, `scripts.dll`, `wwconfig.exe`, and their required runtime DLLs together. The `win` preset's Release output is normally in `build/win/Release`.
2. Provide a complete original Renegade installation on each machine. The game directory is the folder **containing** `Data`, not `Data` itself; the launcher checks for `Data/always.dat`. See [Running the Game](#running-the-game) for supported game data.
3. Copy all five files from [tools/coop-launcher](tools/coop-launcher) into the directory containing `renegade.exe` and `wwconfig.exe`. Run the copies from that directory. These scripts locate the executables beside themselves.
4. Run `Play.cmd`. On first use, enter the Renegade installation folder and a unique player name, then save the video/audio settings when WWConfig opens. The launcher saves the game path and player name in `launcher-settings.json` and uses `openw3d.conf` beside the executables. Use `Configure.cmd` to change video/audio settings later.

All players need the updated co-op client and scripts together: co-op network packets differ from older builds. Game assets are not included in this repository.

### Host and Join from the Menu

1. The host launches `Play.cmd`, opens **Co-op Campaign**, enters a player name, and chooses the host option. Set the player limit, select a campaign mission, and adjust the co-op settings before starting the session. This opens the pre-game lobby.
2. Each guest opens **Co-op Campaign** and joins the host's LAN listing. If the listing is unavailable, enter the host's IPv4 address and port in the direct-connect fields and click **Connect**. The default port is `4848`.
3. Players use **Ready** to show they are ready. The host clicks **Start** to load the mission; ready indicators do not prevent the host from starting early.
4. After a mission, the group returns to the results lobby. Review the statistics and ready up, then the host clicks **Start** to continue to the next mission.

For a LAN game, use the host's LAN IPv4 address. For an Internet game, guests use the host's public IPv4 address; the host must allow the game through the firewall and forward the chosen **UDP** port to the hosting PC when behind a router. Use the same port on the host and guests. Direct connect expects a numeric IPv4 address, not a hostname.

### Launch Directly into a Co-op Session

Run these commands in PowerShell from the directory where you copied the launchers. Replace the sample game path, player names, and host address with your own.

Host:

```powershell
.\Host-Coop.cmd -GameDirectory "C:\Games\Renegade" -PlayerName "Host" -Mission "M01.mix" -Port 4848
```

Guest, on another machine:

```powershell
.\Join-Coop.cmd -GameDirectory "C:\Games\Renegade" -PlayerName "Player2" -Server "192.168.1.100:4848"
```

`Host-Coop.cmd` defaults to `M01.mix` and port `4848`; `Join-Coop.cmd` prompts for the host address if omitted. The game directory and player name can be omitted after the first launch because they are saved locally. To use a different port, pass `-Port` to the host and include `:port` in the guest's `-Server` value. Direct hosting also opens the lobby before the mission starts.

To launch without the helper scripts, first configure the game, then run the appropriate host or guest command from the executable directory:

```powershell
.\wwconfig.exe --ini ".\openw3d.conf"

# Host
.\renegade.exe --gamedir "C:\Games\Renegade" --ini ".\openw3d.conf" --multi --netplayername "Host" --coop-host "M01.mix" --coop-port 4848

# Guest (run on the guest's machine)
.\renegade.exe --gamedir "C:\Games\Renegade" --ini ".\openw3d.conf" --multi --netplayername "Player2" --coop-connect "192.168.1.100:4848"
```

The root-level `host-coop-direct.bat` and `connect-player2.bat` contain developer-specific paths, names, and an IP address. Use the configurable launchers above for your own setup.

### Host Settings and In-game Controls

The co-op host options expose the following settings. Choose these before starting the session; the lobby displays a summary for guests.

| Setting | Effect / default |
| --- | --- |
| Difficulty | Easy, Normal, or Hard campaign difficulty. |
| Player limit, friendly fire, radar | Configure session size and team rules; four players by default. |
| Enable Sprint | Enabled by default. Bind **Sprint** in the controls menu; the default mapping uses Left Shift when available. |
| No Health / Armor / Ammo Pickups | Independently disable these pickup effects; all are allowed by default. |
| Enemy HP (%) / Enemy Dmg (%) | Scale enemy health/armor and damage; both default to 100%. For example, 200 HP and 50 Dmg doubles enemy health/armor and halves enemy damage to players. |
| Death Loss | Score deducted for a player death; defaults to 100 points. |
| AI Sight (%) / AI Hear (%) | Perception multipliers; default to 125% and 150%. |
| AI Agg + (%) / AI Cover + (%) | Bonuses to aggression and taking cover; both default to 15 percentage points. |
| AI Share (m) | Information-sharing radius; defaults to 15 meters. |
| AI Aim Err (%) | Weapon error multiplier; defaults to 75%. Lower values make AI aim more accurately. |
| AI Lock (%) | Chance that special damage, such as fire, locks an AI soldier into its reaction animation; defaults to 35%. |
| AI Attack Wander / AI Retarget / AI Combat Types | Combat movement, retargeting after damage, and unit-specific combat behavior; all enabled by default. |

During a mission, use **Character** in the in-game menu to open EVA Characters, choose an available character, and click **Select**. This changes the visual model without replacing the player's health or inventory. Use **Respawn** to request a manual respawn; the usual safe-spawn checks still apply. If the HUD says you are waiting for a safe location, have a surviving teammate move clear of combat and obstacles.

### Network Update Rate

While in co-op, enter `nur` in the game console to display the local update limit, or `nur 60` to set it. Co-op accepts values from **5 to 120** and defaults to **60**. The setting is saved separately as `CoopNetUpdateRate`; changing it affects only the machine where you enter the command. Regular multiplayer retains its separate 5-30 range.

Start with 60 on each machine. For movement comparisons, try `nur 30`, `nur 90`, or `nur 120`. Actual updates also depend on frame rate, bandwidth, and object priority; higher settings are not a guarantee that movement jitter will disappear. Dedicated servers can set `CoopNetUpdateRate=60` in the `[Server]` section of `server.ini`.

### Current Co-op Limitations

- Campaign scripts still contain assumptions about a single player. In particular, the M11 Petrova cinematic can lock teammates who are outside the intended arena; participant positioning remains unresolved.
- Recent fixes cover missing players after map loads and stale cinematic camera/control state, but the branch still needs full campaign playtesting with multiple machines.
- The network update-rate changes have regression coverage, but movement jitter still needs live multiplayer testing. Raising the update limit is not a verified fix.
- Session size is configurable beyond the four-player default, but the lobby currently tracks up to 16 players. Larger sessions have not been validated.

See the [co-op review and playtest checklist](campaign-coop-review.md) and [campaign script audit](campaign-coop-script-audit.md) for remaining work and test cases.

## Dependencies

OpenW3D makes use of several open source libraries to provide cross platform support.
In order to build the project they will need to be made available to the build system either via a package manager or manually installing and updating your path environment variables.

For convenience we provide support for using vcpkg to install the required dependencies:

[SDL3](https://wiki.libsdl.org/SDL3/FrontPage)

[OpenAL-soft](https://openal-soft.org/)

[FFMpeg](https://www.ffmpeg.org/)

[FreeType](https://freetype.org/)

[ICU4C](https://icu.unicode.org/)

[Qt](https://www.qt.io/)

In addition to these, we also use these less commonly packaged libraries that are pulled in by the build system. You do not need to provide these manually unless you are building off line:

[Crunch](https://github.com/binomialllc/crunch)

[GameSpySDK](https://github.com/TheAssemblyArmada/GamespySDK)


## Compiling the Project

For most platforms the following command should build the project with the default configuration when run from the root folders of a cloned copy of this repository:

`cmake -B build . && cmake --build build`

The build artifacts will end up in the `build` subdirectory which will be created.

It is also possible to build from various IDE by using the presets provided in `CMakePresets.json`.

You can also create your own prests by creating and populating `CMakeUserPresets.json` in the root folder of the cloned repository. This is useful for development or testing purposes to create additional build options.
See [cmake-presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) for more information.

## Running the Game

To use the compiled binaries, you must provide game data. At the time of writing only the original C&C: Renegade game is tested and supported.
The C&C Ultimate Collection is available for purchase on [EA App](https://www.ea.com/en-gb/games/command-and-conquer/command-and-conquer-the-ultimate-collection/buy/pc) or [Steam](https://store.steampowered.com/bundle/39394/Command__Conquer_The_Ultimate_Collection/).
The compiled binaries will also work with a fully patched installation from the original CDs.

## Known Issues

The “Debug” configuration of the “Commando” project (the Renegade main project) will sometimes fail to link the final executable.
This is due to Windows Defender incorrectly detecting RenegadeD.exe containing a virus (possibly due to the embedded browser code).
Excluding the output `/Run/` folder found in the root of this repository in Windows Defender should resolve this for you.

## Contributing

We welcome contributions to the project! If you’re interested in contributing, you need to have knowledge of C++.
Join the developer chat on [Discord](https://discord.com/invite/jMmmRa2) for more information on how to get started.
Please make sure to read our [Contributing Guidelines](CONTRIBUTING.md) before submitting a pull request.

## License

EA has not endorsed and does not support this project. All trademarks are the property of their respective owners.

This repository and its contents are licensed under the GPL v3 license, with additional terms applied. Please see [LICENSE.md](LICENSE.md) for details.
